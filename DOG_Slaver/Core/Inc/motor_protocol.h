/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    motor_protocol.h
  * @brief   电机控制协议定义（主从控制器共用）
  ******************************************************************************
  */
/* USER CODE END Header */

#ifndef __MOTOR_PROTOCOL_H__
#define __MOTOR_PROTOCOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

// ==================== 电机ID配置 ====================
#define MOTOR_ID_1       0x01
#define MOTOR_ID_2       0x02
#define MOTOR_ID_3       0x03

// 从控FDCAN1电机
#define MOTOR_ID_4       0x04
#define MOTOR_ID_5       0x05
#define MOTOR_ID_6       0x06

// 从控FDCAN2电机
#define MOTOR_ID_7       0x07
#define MOTOR_ID_8       0x08
#define MOTOR_ID_9       0x09

// 从控FDCAN3电机
#define MOTOR_ID_10      0x0A
#define MOTOR_ID_11      0x0B
#define MOTOR_ID_12      0x0C

// 全局指令ID（用于零点设置、ID修改等）
#define CAN_ID_GLOBAL    0x7FF

// CAN超时保护时间
#define CAN_TIMEOUT_MS   500

// ==================== USB数据包格式定义 ====================
#define USB_PACKET_SIZE         112
#define USB_HEADER              0xAA
#define USB_FOOTER              0x55

// 命令类型
#define CMD_TYPE_MOTOR_CTRL     0x10    // 电机控制
#define CMD_TYPE_CONFIG         0x11    // 配置命令
#define CMD_TYPE_QUERY          0x12    // 查询命令

// USB数据包结构
typedef struct {
    uint8_t header;                     // 帧头 0xAA
    uint8_t cmdType;                    // 命令类型
    uint16_t length;                    // 数据长度（大端序）
    uint8_t sequence;                   // 序列号
    uint8_t reserved;                   // 保留
    uint8_t motorData[12][8];           // 12个电机数据，每个8字节
    uint8_t reservedBytes[8];           // 保留字节
    uint8_t checksum;                   // 校验和
    uint8_t footer;                     // 帧尾 0x55
} __attribute__((packed)) USBPacket_t;

// ==================== SPI数据包格式定义 ====================
// 与主控 u_spi.c 保持一致：
// [header(1)][cmdType(1)][motorCnt(1)][sequence(1)][reserved(2)][9×8 motor data][checksum(1)][footer(1)]
// 总计 6 + 72 + 2 = 80 字节
#define SPI_PACKET_SIZE         80
#define SPI_HEADER              0xAA
#define SPI_FOOTER              0x55
#define SPI_CMD_MOTOR_CONTROL   0x10
#define SPI_MOTOR_CNT           9

// SPI数据包结构（主控→从控，80字节）
typedef struct {
    uint8_t header;                     // 帧头 0xAA
    uint8_t cmdType;                    // 命令类型 0x10
    uint8_t motorCnt;                   // 电机数量 9
    uint8_t sequence;                   // 序列号
    uint8_t reserved[2];                // 保留字节
    uint8_t motorData[9][8];            // 9个电机数据（电机4-12）
    uint8_t checksum;                   // XOR校验和（Byte 0~77 异或）
    uint8_t footer;                     // 帧尾 0x55
} __attribute__((packed)) SPIPacket_t;

// SPI反馈数据包结构（从控→主控，80字节）
typedef struct {
    uint8_t header;                     // 帧头 0xAA
    uint8_t status;                     // 状态字节
    uint8_t motorCnt;                   // 电机数量 9
    uint8_t sequence;                   // 序列号
    uint8_t reserved[2];                // 保留字节
    uint8_t motorFeedback[9][8];        // 9个电机反馈数据
    uint8_t checksum;                   // XOR校验和
    uint8_t footer;                     // 帧尾 0x55
} __attribute__((packed)) SPIFeedbackPacket_t;

// ==================== 电机控制数据结构 ====================
// 电机控制指令（力位混控模式，EC-A6408-P2-25）
typedef struct {
    float kp;           // KP参数 (0~500)
    float kd;           // KD参数 (0~5)
    float position;     // 期望位置 (-12.5~+12.5 rad)
    float velocity;     // 期望速度 (-18~+18 rad/s)
    float torque;       // 前馈扭矩 (-30~+30 Nm)
} MotorCommand_t;

// 电机反馈数据
typedef struct {
    uint8_t errorCode;  // 错误码
    float position;     // 实际位置 (rad)
    float velocity;     // 实际速度 (rad/s)
    float current;      // 实际电流 (A, -30~+30)
    float temperature;  // 温度 (℃)
} MotorFeedback_t;

// 电机错误码定义
typedef enum {
    MOTOR_ERROR_NONE = 0,           // 正常
    MOTOR_ERROR_OVERHEAT = 1,       // 过热
    MOTOR_ERROR_OVERCURRENT = 2,    // 过流
    MOTOR_ERROR_OVERVOLTAGE = 3,    // 电压过高
    MOTOR_ERROR_UNDERVOLTAGE = 4,   // 电压过低
    MOTOR_ERROR_ENCODER = 5,        // 编码器错误
    MOTOR_ERROR_BRAKE_VOLTAGE = 6,  // 刹车电压异常
    MOTOR_ERROR_DRV = 7             // DRV驱动错误
} MotorError_e;

// ==================== 物理值与CAN原始值换算函数 ====================
// 换算公式（官方 float_to_uint / uint_to_float，EC-A6408-P2-25型号）：
// raw = (phys - phys_min) / (phys_max - phys_min) * (2^bits - 1)
// phys = raw / (2^bits - 1) * (phys_max - phys_min) + phys_min

// KP物理值转CAN原始值 (0~500 -> 0~4095, 12bit)
static inline uint16_t KP_PhysicalToRaw(float kp) {
    if (kp < 0.0f) kp = 0.0f;
    if (kp > 500.0f) kp = 500.0f;
    return (uint16_t)(kp / 500.0f * 4095.0f + 0.5f);
}

// KD物理值转CAN原始值 (0~5 -> 0~511, 9bit)
static inline uint16_t KD_PhysicalToRaw(float kd) {
    if (kd < 0.0f) kd = 0.0f;
    if (kd > 5.0f) kd = 5.0f;
    uint16_t raw = (uint16_t)(kd / 5.0f * 511.0f + 0.5f);
    return (raw > 511) ? 511 : raw;
}

// 位置物理值转CAN原始值 (-12.5~+12.5rad -> 0~65535, 16bit)
static inline uint16_t Position_PhysicalToRaw(float pos) {
    if (pos < -12.5f) pos = -12.5f;
    if (pos > 12.5f) pos = 12.5f;
    return (uint16_t)((pos + 12.5f) / 25.0f * 65535.0f + 0.5f);
}

// 速度物理值转CAN原始值 (-18~+18rad/s -> 0~4095, 12bit)
static inline uint16_t Velocity_PhysicalToRaw(float vel) {
    if (vel < -18.0f) vel = -18.0f;
    if (vel > 18.0f) vel = 18.0f;
    uint16_t raw = (uint16_t)((vel + 18.0f) / 36.0f * 4095.0f + 0.5f);
    return (raw > 4095) ? 4095 : raw;
}

// 扭矩物理值转CAN原始值 (-30~+30Nm -> 0~4095, 12bit)
static inline uint16_t Torque_PhysicalToRaw(float torque) {
    if (torque < -30.0f) torque = -30.0f;
    if (torque > 30.0f) torque = 30.0f;
    uint16_t raw = (uint16_t)((torque + 30.0f) / 60.0f * 4095.0f + 0.5f);
    return (raw > 4095) ? 4095 : raw;
}

// CAN原始值转KP物理值 (0~4095 -> 0~500)
static inline float KP_RawToPhysical(uint16_t raw) {
    return (float)raw * 500.0f / 4095.0f;
}

// CAN原始值转KD物理值 (0~511 -> 0~5)
static inline float KD_RawToPhysical(uint16_t raw) {
    return (float)raw * 5.0f / 511.0f;
}

// CAN原始值转位置物理值 (0~65535 -> -12.5~+12.5rad)
static inline float Position_RawToPhysical(uint16_t raw) {
    return (float)raw / 65535.0f * 25.0f - 12.5f;
}

// CAN原始值转速度物理值 (0~4095 -> -18~+18rad/s)
static inline float Velocity_RawToPhysical(uint16_t raw) {
    return (float)raw / 4095.0f * 36.0f - 18.0f;
}

// CAN原始值转扭矩物理值 (0~4095 -> -30~+30Nm)
static inline float Torque_RawToPhysical(uint16_t raw) {
    return (float)raw / 4095.0f * 60.0f - 30.0f;
}

// CAN原始值转电流物理值 (0~4095 -> -30~+30A)
static inline float Current_RawToPhysical(uint16_t raw) {
    return (float)raw * 60.0f / 4095.0f - 30.0f;
}

// CAN原始值转温度物理值 (℃ = (raw - 50) / 2)
static inline float Temperature_RawToPhysical(uint8_t raw) {
    return ((float)raw - 50.0f) / 2.0f;
}

// ==================== 电机控制指令打包函数 ====================
// 将电机控制指令打包为8字节CAN数据
void MotorCommand_Pack(const MotorCommand_t* cmd, uint8_t* canData);

// 从8字节CAN数据解析电机控制指令
void MotorCommand_Unpack(const uint8_t* canData, MotorCommand_t* cmd);

// 从8字节CAN数据解析电机反馈
void MotorFeedback_Unpack(const uint8_t* canData, MotorFeedback_t* feedback);

// 计算校验和（XOR）
uint8_t CalculateChecksum(const uint8_t* data, uint16_t length);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_PROTOCOL_H__ */
