/**
 * @file motor_control.c
 * @brief 电机控制协议实现 - 物理值与CAN原始值换算、控制数据打包解包
 * @note 本文件实现电机控制相关的换算和打包函数
 */

#include "motor.h"
#include <string.h>
#include <stdio.h>

/* ==================== 物理值与CAN原始值换算函数 ==================== */
/* 换算公式（官方 float_to_uint / uint_to_float，EC-A6408-P2-25型号）：
 * raw = (phys - phys_min) / (phys_max - phys_min) * (2^bits - 1)
 * phys = raw / (2^bits - 1) * (phys_max - phys_min) + phys_min
 *
 * KP  物理值(0~500)      12bit  raw = kp / 500 * 4095
 * KD  物理值(0~5)        9bit   raw = kd / 5 * 511
 * 位置(-12.5~+12.5 rad)  16bit  raw = (pos + 12.5) / 25 * 65535
 * 速度(-18~+18 rad/s)    12bit  raw = (vel + 18) / 36 * 4095
 * 扭矩(-30~+30 Nm)       12bit  raw = (tor + 30) / 60 * 4095
 */

uint16_t Motor_KP_PhysToRaw(float kp_phys)
{
    /* 限制范围 */
    if (kp_phys < KP_MIN) kp_phys = KP_MIN;
    if (kp_phys > KP_MAX) kp_phys = KP_MAX;

    /* raw = (kp - 0) / (500 - 0) * (4096 - 1) */
    uint16_t raw = (uint16_t)(kp_phys / 500.0f * 4095.0f + 0.5f);
    return raw;
}

float Motor_KP_RawToPhys(uint16_t kp_raw)
{
    /* 限制范围 */
    if (kp_raw > 4095) kp_raw = 4095;

    /* phys = raw / 4095 * 500 */
    return kp_raw * 500.0f / 4095.0f;
}

uint16_t Motor_KD_PhysToRaw(float kd_phys)
{
    /* 限制范围 */
    if (kd_phys < KD_MIN) kd_phys = KD_MIN;
    if (kd_phys > KD_MAX) kd_phys = KD_MAX;

    /* raw = (kd - 0) / (5 - 0) * (512 - 1) */
    uint16_t raw = (uint16_t)(kd_phys / 5.0f * 511.0f + 0.5f);
    return (raw > 511) ? 511 : raw;
}

float Motor_KD_RawToPhys(uint16_t kd_raw)
{
    /* 限制范围 */
    if (kd_raw > 511) kd_raw = 511;

    /* phys = raw / 511 * 5 */
    return kd_raw * 5.0f / 511.0f;
}

uint16_t Motor_Pos_PhysToRaw(float pos_phys)
{
    /* 限制范围 */
    if (pos_phys < POS_MIN) pos_phys = POS_MIN;
    if (pos_phys > POS_MAX) pos_phys = POS_MAX;

    /* raw = (pos - (-12.5)) / (12.5 - (-12.5)) * (65536 - 1) */
    uint16_t raw = (uint16_t)((pos_phys - POS_MIN) / (POS_MAX - POS_MIN) * 65535.0f + 0.5f);
    return raw;
}

float Motor_Pos_RawToPhys(uint16_t pos_raw)
{
    /* 限制范围 */
    if (pos_raw > 65535) pos_raw = 65535;

    /* phys = raw / 65535 * 25 - 12.5 */
    return pos_raw * (POS_MAX - POS_MIN) / 65535.0f + POS_MIN;
}

uint16_t Motor_Vel_PhysToRaw(float vel_phys)
{
    /* 限制范围 */
    if (vel_phys < VEL_MIN) vel_phys = VEL_MIN;
    if (vel_phys > VEL_MAX) vel_phys = VEL_MAX;

    /* raw = (vel - (-18)) / (18 - (-18)) * (4096 - 1) */
    uint16_t raw = (uint16_t)((vel_phys - VEL_MIN) / (VEL_MAX - VEL_MIN) * 4095.0f + 0.5f);
    return (raw > 4095) ? 4095 : raw;
}

float Motor_Vel_RawToPhys(uint16_t vel_raw)
{
    /* 限制范围 */
    if (vel_raw > 4095) vel_raw = 4095;

    /* phys = raw / 4095 * 36 - 18 */
    return vel_raw * (VEL_MAX - VEL_MIN) / 4095.0f + VEL_MIN;
}

uint16_t Motor_Torque_PhysToRaw(float torque_phys)
{
    /* 限制范围 */
    if (torque_phys < TORQUE_MIN) torque_phys = TORQUE_MIN;
    if (torque_phys > TORQUE_MAX) torque_phys = TORQUE_MAX;

    /* raw = (tor - (-30)) / (30 - (-30)) * (4096 - 1) */
    uint16_t raw = (uint16_t)((torque_phys - TORQUE_MIN) / (TORQUE_MAX - TORQUE_MIN) * 4095.0f + 0.5f);
    return (raw > 4095) ? 4095 : raw;
}

float Motor_Torque_RawToPhys(uint16_t torque_raw)
{
    /* 限制范围 */
    if (torque_raw > 4095) torque_raw = 4095;

    /* phys = raw / 4095 * 60 - 30 */
    return torque_raw * (TORQUE_MAX - TORQUE_MIN) / 4095.0f + TORQUE_MIN;
}

/* ==================== 电机控制数据打包/解包函数 ==================== */
/* 位域定义（64位，Big-Endian）：
 * Bit63-61: 电机模式，固定0x00
 * Bit60-49: KP参数，12bit
 * Bit48-40: KD参数，9bit
 * Bit39-24: 期望位置，16bit
 * Bit23-12: 期望速度，12bit
 * Bit11-0:  前馈扭矩，12bit
 */

void Motor_PackControlData(const MotorControlParam *param, uint8_t *can_data) 
{
    uint64_t bits = 0;

    /* 电机模式 (Bit63-61, 3bit) */
    bits |= ((uint64_t)MOTOR_MODE_FORCE_POSITION & 0x07ULL) << 61;

    /* KP参数 (Bit60-49, 12bit) */
    uint16_t kp_raw = Motor_KP_PhysToRaw(param->kp);
    bits |= ((uint64_t)kp_raw & 0xFFFULL) << 49;

    /* KD参数 (Bit48-40, 9bit) */
    uint16_t kd_raw = Motor_KD_PhysToRaw(param->kd);
    bits |= ((uint64_t)kd_raw & 0x1FFULL) << 40;

    /* 期望位置 (Bit39-24, 16bit) */
    uint16_t pos_raw = Motor_Pos_PhysToRaw(param->position);
    bits |= ((uint64_t)pos_raw & 0xFFFFULL) << 24;

    /* 期望速度 (Bit23-12, 12bit) */
    uint16_t vel_raw = Motor_Vel_PhysToRaw(param->velocity);
    bits |= ((uint64_t)vel_raw & 0xFFFULL) << 12;

    /* 前馈扭矩 (Bit11-0, 12bit) */
    uint16_t torque_raw = Motor_Torque_PhysToRaw(param->torque);
    bits |= ((uint64_t)torque_raw & 0xFFFULL) << 0;

    /* 打包为8字节Big-Endian */
    can_data[0] = (bits >> 56) & 0xFF;
    can_data[1] = (bits >> 48) & 0xFF;
    can_data[2] = (bits >> 40) & 0xFF;
    can_data[3] = (bits >> 32) & 0xFF;
    can_data[4] = (bits >> 24) & 0xFF;
    can_data[5] = (bits >> 16) & 0xFF;
    can_data[6] = (bits >> 8) & 0xFF;
    can_data[7] = bits & 0xFF;
}

void Motor_UnpackControlData(const uint8_t *can_data, MotorControlParam *param)
{
    uint64_t bits = 0;

    /* 从8字节Big-Endian解析 */
    bits |= ((uint64_t)can_data[0]) << 56;
    bits |= ((uint64_t)can_data[1]) << 48;
    bits |= ((uint64_t)can_data[2]) << 40;
    bits |= ((uint64_t)can_data[3]) << 32;
    bits |= ((uint64_t)can_data[4]) << 24;
    bits |= ((uint64_t)can_data[5]) << 16;
    bits |= ((uint64_t)can_data[6]) << 8;
    bits |= ((uint64_t)can_data[7]) << 0;

    /* 解析各字段 */
    /* KP参数 (Bit60-49, 12bit) */
    uint16_t kp_raw = (bits >> 49) & 0xFFFULL;
    param->kp = Motor_KP_RawToPhys(kp_raw);

    /* KD参数 (Bit48-40, 9bit) */
    uint16_t kd_raw = (bits >> 40) & 0x1FFULL;
    param->kd = Motor_KD_RawToPhys(kd_raw);

    /* 期望位置 (Bit39-24, 16bit) */
    uint16_t pos_raw = (bits >> 24) & 0xFFFFULL;
    param->position = Motor_Pos_RawToPhys(pos_raw);

    /* 期望速度 (Bit23-12, 12bit) */
    uint16_t vel_raw = (bits >> 12) & 0xFFFULL;
    param->velocity = Motor_Vel_RawToPhys(vel_raw);

    /* 前馈扭矩 (Bit11-0, 12bit) */
    uint16_t torque_raw = bits & 0xFFFULL;
    param->torque = Motor_Torque_RawToPhys(torque_raw);
}

/* ==================== 电机反馈数据解包函数 ==================== */
/* 反馈报文位域定义（64位，Big-Endian）：
 * Bit63-61: 报文类型(ack_status)，3bit，固定0x01
 * Bit60-56: 错误码，5bit
 * Bit55-40: 实际位置，16bit
 * Bit39-28: 实际速度，12bit
 * Bit27-16: 实际电流，12bit
 * Bit15-8:  温度，8bit
 * Bit7-0:   未使用（填0）
 */

void Motor_UnpackFeedbackData(const uint8_t *can_data, MotorFeedback *feedback)
{
    uint64_t bits = 0;

    /* 从8字节Big-Endian解析 */
    bits |= ((uint64_t)can_data[0]) << 56;
    bits |= ((uint64_t)can_data[1]) << 48;
    bits |= ((uint64_t)can_data[2]) << 40;
    bits |= ((uint64_t)can_data[3]) << 32;
    bits |= ((uint64_t)can_data[4]) << 24;
    bits |= ((uint64_t)can_data[5]) << 16;
    bits |= ((uint64_t)can_data[6]) << 8;
    bits |= ((uint64_t)can_data[7]) << 0;

    /* 解析各字段 */
    /* 报文类型 (Bit63-61, 3bit) - 验证是否为0x01 */
    uint8_t msg_type = (bits >> 61) & 0x07ULL;
	(void)msg_type;

    /* 错误码 (Bit60-56, 5bit) */
    feedback->error_code = (bits >> 56) & 0x1FULL;

    /* 实际位置 (Bit55-40, 16bit) */
    uint16_t pos_raw = (bits >> 40) & 0xFFFFULL;
    feedback->position = Motor_Pos_RawToPhys(pos_raw);

    /* 实际速度 (Bit39-28, 12bit) */
    uint16_t vel_raw = (bits >> 28) & 0xFFFULL;
    feedback->velocity = Motor_Vel_RawToPhys(vel_raw);

    /* 实际电流 (Bit27-16, 12bit): phys = raw / 4095 * 60 - 30 (A, 范围 -30~+30) */
    uint16_t current_raw = (bits >> 16) & 0xFFFULL;
    feedback->current = current_raw * 60.0f / 4095.0f - 30.0f;

    /* 温度 (Bit15-8, 8bit): ℃ = (原始值 - 50) / 2 */
    uint8_t temp_raw = (bits >> 8) & 0xFFULL;
    feedback->temperature = (temp_raw - 50) / 2.0f;
}

/* ==================== 电机ID辅助函数 ==================== */
uint32_t Motor_GetMasterID(uint8_t motor_id)
{
    return (uint32_t)(motor_id + MOTOR_MASTER_ID_BASE);
}

uint8_t Motor_IsMasterControlled(uint8_t motor_id)
{
    /* 主控FDCAN1控制电机ID: 0x01, 0x02, 0x03 */
    return (motor_id >= MOTOR_ID_1 && motor_id <= MOTOR_ID_3) ? 1 : 0;
}

/* ==================== 错误码字符串转换 ==================== */
const char* Motor_GetErrorString(uint8_t err_code)
{
//    static const char* error_strings[] = {
//        "正常",           // 0
//        "过热",           // 1
//        "过流",           // 2
//        "电压过高",       // 3
//        "电压过低",       // 4
//        //"编码器错误",     // 5
//        "刹车电压异常",   // 6
//        "DRV驱动错误"     // 7
//    };

    if (err_code > 7) {
        return "未知错误";
    }

   // return error_strings[err_code];
	return 0;
}
