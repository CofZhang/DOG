#include "motor_protocol.h"
#include <math.h>
#include <string.h>

/* ==================== 辅助函数 ==================== */
static float clamp(float value, float min_val, float max_val)
{
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/* ==================== 物理值与CAN原始值换算 ==================== */
uint16_t motor_kp_phys_to_raw(float kp_phys)
{
    kp_phys = clamp(kp_phys, KP_MIN, KP_MAX);
    uint16_t raw = (uint16_t)(kp_phys * 819.0f + 0.5f);
    if (raw > 4095) raw = 4095;
    return raw & 0xFFF;
}

float motor_kp_raw_to_phys(uint16_t kp_raw)
{
    kp_raw = kp_raw & 0xFFF;
    return kp_raw / 819.0f;
}

uint16_t motor_kd_phys_to_raw(float kd_phys)
{
    kd_phys = clamp(kd_phys, KD_MIN, KD_MAX);
    uint16_t raw = (uint16_t)(kd_phys / 0.0978f + 0.5f);
    if (raw > 511) raw = 511;
    return raw & 0x1FF;
}

float motor_kd_raw_to_phys(uint16_t kd_raw)
{
    kd_raw = kd_raw & 0x1FF;
    return kd_raw * 0.0978f;
}

uint16_t motor_pos_phys_to_raw(float pos_phys)
{
    pos_phys = clamp(pos_phys, POS_MIN, POS_MAX);
    uint16_t raw = (uint16_t)((pos_phys + 60.0f) * 546.125f + 0.5f);
    if (raw > 65535) raw = 65535;
    return raw & 0xFFFF;
}

float motor_pos_raw_to_phys(uint16_t pos_raw)
{
    pos_raw = pos_raw & 0xFFFF;
    return pos_raw / 546.125f - 60.0f;
}

uint16_t motor_vel_phys_to_raw(float vel_phys)
{
    vel_phys = clamp(vel_phys, VEL_MIN, VEL_MAX);
    uint16_t raw = (uint16_t)((vel_phys + 60.0f) * 34.125f + 0.5f);
    if (raw > 4095) raw = 4095;
    return raw & 0xFFF;
}

float motor_vel_raw_to_phys(uint16_t vel_raw)
{
    vel_raw = vel_raw & 0xFFF;
    return vel_raw / 34.125f - 60.0f;
}

uint16_t motor_torque_phys_to_raw(float torque_phys)
{
    torque_phys = clamp(torque_phys, TORQUE_MIN, TORQUE_MAX);
    uint16_t raw = (uint16_t)((torque_phys + 60.0f) * 34.125f + 0.5f);
    if (raw > 4095) raw = 4095;
    return raw & 0xFFF;
}

float motor_torque_raw_to_phys(uint16_t torque_raw)
{
    torque_raw = torque_raw & 0xFFF;
    return torque_raw / 34.125f - 60.0f;
}

float deg_to_rad(float deg)
{
    return deg * 3.141592653589793f / 180.0f;
}

float rad_to_deg(float rad)
{
    return rad * 180.0f / 3.141592653589793f;
}

/* ==================== 电机控制数据打包（8字节Big-Endian）==================== */
void motor_pack_control_data(const MotorControlParam *param, uint8_t *can_data)
{
    uint64_t bits = 0;
    
    bits |= ((uint64_t)(MOTOR_MODE_FORCE_POSITION & 0x07) << 61);
    
    uint16_t kp_raw = motor_kp_phys_to_raw(param->kp);
    bits |= ((uint64_t)(kp_raw & 0xFFF) << 49);
    
    uint16_t kd_raw = motor_kd_phys_to_raw(param->kd);
    bits |= ((uint64_t)(kd_raw & 0x1FF) << 40);
    
    uint16_t pos_raw = motor_pos_phys_to_raw(param->position);
    bits |= ((uint64_t)(pos_raw & 0xFFFF) << 24);
    
    uint16_t vel_raw = motor_vel_phys_to_raw(param->velocity);
    bits |= ((uint64_t)(vel_raw & 0xFFF) << 12);
    
    uint16_t torque_raw = motor_torque_phys_to_raw(param->torque);
    bits |= ((uint64_t)(torque_raw & 0xFFF) << 0);
    
    can_data[0] = (uint8_t)((bits >> 56) & 0xFF);
    can_data[1] = (uint8_t)((bits >> 48) & 0xFF);
    can_data[2] = (uint8_t)((bits >> 40) & 0xFF);
    can_data[3] = (uint8_t)((bits >> 32) & 0xFF);
    can_data[4] = (uint8_t)((bits >> 24) & 0xFF);
    can_data[5] = (uint8_t)((bits >> 16) & 0xFF);
    can_data[6] = (uint8_t)((bits >> 8) & 0xFF);
    can_data[7] = (uint8_t)(bits & 0xFF);
}

/* ==================== 电机反馈数据解包 ==================== */
void motor_unpack_feedback_data(const uint8_t *can_data, MotorFeedback *feedback)
{
    uint64_t bits = 0;
    bits |= ((uint64_t)can_data[0]) << 56;
    bits |= ((uint64_t)can_data[1]) << 48;
    bits |= ((uint64_t)can_data[2]) << 40;
    bits |= ((uint64_t)can_data[3]) << 32;
    bits |= ((uint64_t)can_data[4]) << 24;
    bits |= ((uint64_t)can_data[5]) << 16;
    bits |= ((uint64_t)can_data[6]) << 8;
    bits |= ((uint64_t)can_data[7]) << 0;
    
    feedback->error_code = (bits >> 56) & 0x1F;
    
    uint16_t pos_raw = (bits >> 40) & 0xFFFF;
    feedback->position = motor_pos_raw_to_phys(pos_raw);
    
    uint16_t vel_raw = (bits >> 28) & 0xFFF;
    feedback->velocity = motor_vel_raw_to_phys(vel_raw);
    
    uint16_t current_raw = (bits >> 16) & 0xFFF;
    feedback->current = (current_raw / 4095.0f) * 124.0f - 62.0f;
    
    uint8_t coil_temp_raw = (bits >> 8) & 0xFF;
    feedback->coil_temp = (coil_temp_raw - 50) / 2.0f;
    
    uint8_t mos_temp_raw = bits & 0xFF;
    feedback->mos_temp = (mos_temp_raw - 50) / 2.0f;
}

/* ==================== USB控制数据包 ==================== */
void usb_control_pkg_init(USBControlPkg *pkg)
{
    pkg->header = USB_PKG_HEADER;
    pkg->cmd_type = CMD_TYPE_CONTROL;
    pkg->length = USB_PKG_DATA_LEN;
    pkg->sequence = 0;
    pkg->reserved = 0;
    
    for (int i = 0; i < 12; i++) {
        pkg->motor[i].kp = 1.0f;
        pkg->motor[i].kd = 10.0f;
        pkg->motor[i].position = 0.0f;
        pkg->motor[i].velocity = 0.0f;
        pkg->motor[i].torque = 0.0f;
    }
    
    memset(pkg->reserved2, 0, 8);
    pkg->checksum = 0;
    pkg->footer = USB_PKG_FOOTER;
}

void usb_control_pkg_set_motor_position_deg(USBControlPkg *pkg, int motor_index, float deg, float kp, float kd)
{
    if (motor_index >= 0 && motor_index < 12) {
        pkg->motor[motor_index].kp = kp;
        pkg->motor[motor_index].kd = kd;
        pkg->motor[motor_index].position = deg_to_rad(deg);
        pkg->motor[motor_index].velocity = 0.0f;
        pkg->motor[motor_index].torque = 0.0f;
    }
}

void usb_control_pkg_set_all_motors_position_deg(USBControlPkg *pkg, float deg, float kp, float kd)
{
    for (int i = 0; i < 12; i++) {
        usb_control_pkg_set_motor_position_deg(pkg, i, deg, kp, kd);
    }
}

void usb_control_pkg_pack(USBControlPkg *pkg, uint8_t *data)
{
    memset(data, 0, USB_PKG_TOTAL_LEN);
    
    data[0] = pkg->header;
    data[1] = pkg->cmd_type;
    data[2] = (uint8_t)((pkg->length >> 8) & 0xFF);
    data[3] = (uint8_t)(pkg->length & 0xFF);
    data[4] = pkg->sequence;
    data[5] = pkg->reserved;
    
    for (int i = 0; i < 12; i++) {
        uint8_t motor_data[8];
        motor_pack_control_data(&pkg->motor[i], motor_data);
        memcpy(&data[6 + i * 8], motor_data, 8);
    }
    
    for (int i = 0; i < 8; i++) {
        data[102 + i] = pkg->reserved2[i];
    }
    
    uint8_t checksum = 0;
    for (int i = 0; i < 110; i++) {
        checksum ^= data[i];
    }
    pkg->checksum = checksum;
    data[110] = checksum;
    data[111] = pkg->footer;
}

/* ==================== 反馈数据包解析 ==================== */
int feedback_pkg_parse(const uint8_t *data, MotorFeedback feedbacks[12])
{
    if (data[0] != USB_PKG_HEADER || data[111] != USB_PKG_FOOTER) {
        return -1;
    }
    
    for (int i = 0; i < 12; i++) {
        motor_unpack_feedback_data(&data[6 + i * 8], &feedbacks[i]);
    }
    
    return 0;
}
