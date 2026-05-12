#ifndef MOTOR_PROTOCOL_H
#define MOTOR_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 常量定义 ==================== */
#define USB_PKG_HEADER           0xAA
#define USB_PKG_FOOTER           0x55
#define USB_PKG_TOTAL_LEN        164
#define USB_PKG_HEADER_LEN       6
#define USB_PKG_DATA_LEN         96
#define USB_PKG_MOTOR_CNT        12
#define USB_PKG_MOTOR_DATA_LEN   8
#define USB_PKG_RESERVED2_LEN    8
#define USB_PKG_CHECKSUM_POS     110
#define USB_PKG_FOOTER_POS       111

#define CMD_TYPE_CONTROL         0x10
#define CMD_TYPE_CONFIG          0x11
#define CMD_TYPE_QUERY           0x12

#define MOTOR_MODE_FORCE_POSITION  0x00

#define KP_MIN      0.0f
#define KP_MAX      5.0f
#define KD_MIN      0.0f
#define KD_MAX      50.0f
#define POS_MIN     -60.0f
#define POS_MAX     60.0f
#define VEL_MIN     -60.0f
#define VEL_MAX     60.0f
#define TORQUE_MIN  -60.0f
#define TORQUE_MAX  60.0f

/* ==================== 数据结构定义 ==================== */
typedef struct {
    float kp;
    float kd;
    float position;
    float velocity;
    float torque;
} MotorControlParam;

typedef struct {
    uint8_t error_code;
    float position;
    float velocity;
    float current;
    float coil_temp;
    float mos_temp;
} MotorFeedback;

typedef struct {
    uint8_t header;
    uint8_t cmd_type;
    uint16_t length;
    uint8_t sequence;
    uint8_t reserved;
    MotorControlParam motor[12];
    uint8_t reserved2[8];
    uint8_t checksum;
    uint8_t footer;
} USBControlPkg;

/* ==================== 函数声明 ==================== */
uint16_t motor_kp_phys_to_raw(float kp_phys);
float motor_kp_raw_to_phys(uint16_t kp_raw);
uint16_t motor_kd_phys_to_raw(float kd_phys);
float motor_kd_raw_to_phys(uint16_t kd_raw);
uint16_t motor_pos_phys_to_raw(float pos_phys);
float motor_pos_raw_to_phys(uint16_t pos_raw);
uint16_t motor_vel_phys_to_raw(float vel_phys);
float motor_vel_raw_to_phys(uint16_t vel_raw);
uint16_t motor_torque_phys_to_raw(float torque_phys);
float motor_torque_raw_to_phys(uint16_t torque_raw);

float deg_to_rad(float deg);
float rad_to_deg(float rad);

void motor_pack_control_data(const MotorControlParam *param, uint8_t *can_data);
void motor_unpack_feedback_data(const uint8_t *can_data, MotorFeedback *feedback);

void usb_control_pkg_init(USBControlPkg *pkg);
void usb_control_pkg_set_motor_position_deg(USBControlPkg *pkg, int motor_index, float deg, float kp, float kd);
void usb_control_pkg_set_all_motors_position_deg(USBControlPkg *pkg, float deg, float kp, float kd);
void usb_control_pkg_pack(USBControlPkg *pkg, uint8_t *data);

int feedback_pkg_parse(const uint8_t *data, MotorFeedback feedbacks[12]);

#ifdef __cplusplus
}
#endif

#endif
