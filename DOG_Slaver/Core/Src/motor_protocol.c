/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    motor_protocol.c
  * @brief   电机控制协议实现
  ******************************************************************************
  */
/* USER CODE END Header */

#include "motor_protocol.h"
#include <string.h>

/**
 * @brief 将电机控制指令打包为8字节CAN数据
 * @param cmd 电机控制指令结构体
 * @param canData 输出的8字节CAN数据
 */
void MotorCommand_Pack(const MotorCommand_t* cmd, uint8_t* canData) {
    // 转换物理值为CAN原始值
    uint16_t kp_raw = KP_PhysicalToRaw(cmd->kp);
    uint16_t kd_raw = KD_PhysicalToRaw(cmd->kd);
    uint16_t pos_raw = Position_PhysicalToRaw(cmd->position);
    uint16_t vel_raw = Velocity_PhysicalToRaw(cmd->velocity);
    uint16_t torque_raw = Torque_PhysicalToRaw(cmd->torque);

    // 位域打包（64位，高位在前）
    // Bit63-61: 电机模式 (0x00 = 力位混控)
    // Bit60-49: KP (12bit)
    // Bit48-40: KD (9bit)
    // Bit39-24: 位置 (16bit)
    // Bit23-12: 速度 (12bit)
    // Bit11-0:  扭矩 (12bit)

    uint64_t data = 0;
    data |= ((uint64_t)0x00 & 0x07) << 61;          // 模式 (3bit)
    data |= ((uint64_t)kp_raw & 0xFFF) << 49;       // KP (12bit)
    data |= ((uint64_t)kd_raw & 0x1FF) << 40;       // KD (9bit)
    data |= ((uint64_t)pos_raw & 0xFFFF) << 24;     // 位置 (16bit)
    data |= ((uint64_t)vel_raw & 0xFFF) << 12;      // 速度 (12bit)
    data |= ((uint64_t)torque_raw & 0xFFF);         // 扭矩 (12bit)

    // 大端序输出
    canData[0] = (data >> 56) & 0xFF;
    canData[1] = (data >> 48) & 0xFF;
    canData[2] = (data >> 40) & 0xFF;
    canData[3] = (data >> 32) & 0xFF;
    canData[4] = (data >> 24) & 0xFF;
    canData[5] = (data >> 16) & 0xFF;
    canData[6] = (data >> 8) & 0xFF;
    canData[7] = data & 0xFF;
}

/**
 * @brief 从8字节CAN数据解析电机控制指令
 * @param canData 8字节CAN数据
 * @param cmd 输出的电机控制指令结构体
 */
void MotorCommand_Unpack(const uint8_t* canData, MotorCommand_t* cmd) {
    // 大端序解析
    uint64_t data = 0;
    data |= ((uint64_t)canData[0]) << 56;
    data |= ((uint64_t)canData[1]) << 48;
    data |= ((uint64_t)canData[2]) << 40;
    data |= ((uint64_t)canData[3]) << 32;
    data |= ((uint64_t)canData[4]) << 24;
    data |= ((uint64_t)canData[5]) << 16;
    data |= ((uint64_t)canData[6]) << 8;
    data |= ((uint64_t)canData[7]);

    // 提取各字段
    uint16_t kp_raw = (data >> 49) & 0xFFF;
    uint16_t kd_raw = (data >> 40) & 0x1FF;
    uint16_t pos_raw = (data >> 24) & 0xFFFF;
    uint16_t vel_raw = (data >> 12) & 0xFFF;
    uint16_t torque_raw = data & 0xFFF;

    // 转换为物理值
    cmd->kp = KP_RawToPhysical(kp_raw);
    cmd->kd = KD_RawToPhysical(kd_raw);
    cmd->position = Position_RawToPhysical(pos_raw);
    cmd->velocity = Velocity_RawToPhysical(vel_raw);
    cmd->torque = Torque_RawToPhysical(torque_raw);
}

/**
 * @brief 从8字节CAN数据解析电机反馈
 * @param canData 8字节CAN反馈数据
 * @param feedback 输出的电机反馈结构体
 */
void MotorFeedback_Unpack(const uint8_t* canData, MotorFeedback_t* feedback) {
    // 协议格式：
    // Byte0 bit7-5: 报文类型(3bit), bit4-0: 错误码(5bit)
    // Byte1~2: Position uint16
    // Byte3 + Byte4高4位: Velocity uint12
    // Byte4低4位 + Byte5: Current uint12 (-60~+60A)
    // Byte6: MotorTemp uint8
    // Byte7: MOSTemp uint8

    feedback->errorCode = canData[0] & 0x1F;

    uint16_t pos_raw = ((uint16_t)canData[1] << 8) | canData[2];
    feedback->position = Position_RawToPhysical(pos_raw);

    uint16_t vel_raw = ((uint16_t)canData[3] << 4) | (canData[4] >> 4);
    feedback->velocity = Velocity_RawToPhysical(vel_raw);

    uint16_t current_raw = (((uint16_t)canData[4] & 0x0F) << 8) | canData[5];
    feedback->current = current_raw * 120.0f / 4095.0f - 60.0f;

    feedback->temperature = (canData[6] - 50) / 2.0f;
}

/**
 * @brief 计算8位校验和（XOR）
 * @param data 数据指针
 * @param length 数据长度
 * @return 校验和
 */
uint8_t CalculateChecksum(const uint8_t* data, uint16_t length) {
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}
