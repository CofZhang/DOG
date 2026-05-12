/**
 * @file motor_control.h
 * @brief 电机控制协议定义 - 包含电机CAN ID配置、物理值与原始值换算函数
 * @note 本文件定义电机控制相关的宏、结构和函数
 */

/* 确保没有被重复包含 */
#ifndef __MOTOR_CONTROL_H
#define __MOTOR_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

/* 引用CubeMX生成的头文件 */
#include "main.h"
#include "fdcan.h"
#include "spi.h"

/* ==================== 电机ID配置（全局宏定义）==================== */
/* 主控FDCAN1电机 */
#define MOTOR_ID_1       0x01
#define MOTOR_ID_2       0x02
#define MOTOR_ID_3       0x03

/* 从控FDCAN1电机 */
#define MOTOR_ID_4       0x04
#define MOTOR_ID_5       0x05
#define MOTOR_ID_6       0x06

/* 从控FDCAN2电机 */
#define MOTOR_ID_7       0x07
#define MOTOR_ID_8       0x08
#define MOTOR_ID_9       0x09

/* 从控FDCAN3电机 */
#define MOTOR_ID_10      0x0A
#define MOTOR_ID_11      0x0B
#define MOTOR_ID_12      0x0C

/* 全局指令ID（用于零点设置、ID修改等） */
#define CAN_ID_GLOBAL    0x7FF

/* CAN超时保护时间 (ms) */
#define CAN_TIMEOUT_MS   500

/* 主控FDCAN1的CAN ID基数 */
#define MOTOR_MASTER_ID_BASE  0x00

/* 从控FDCAN的CAN ID基数 */
#define MOTOR_SLAVE_ID_BASE   0x00

/* ==================== USB数据包定义 ==================== */
#define USB_PKG_HEADER       0xAA    // USB帧头标识
#define USB_PKG_FOOTER       0x55    // USB帧尾标识

#define USB_PKG_TOTAL_LEN    164     //! USB数据包总长度（含填充，底层分3次64+64+36传输）
#define USB_PKG_DATA_LEN     96      //! 电机控制数据总长度

#define USB_PKG_MOTOR_CNT    12      // 电机数量
#define USB_PKG_MOTOR_DATA_LEN  8    // 单个电机控制数据长度
#define USB_PKG_RESERVED_LEN 8       // 保留字节数

/* 帧结构固定偏移量（与 USB_PKG_TOTAL_LEN 无关）
 * Byte 0      : 帧头
 * Byte 1      : 命令类型
 * Byte 2~3    : 数据长度（大端）
 * Byte 4      : 序列号
 * Byte 5      : 保留
 * Byte 6~101  : 12个电机数据（96字节）
 * Byte 102~109: 保留（8字节）
 * Byte 110    : XOR校验和（对 Byte 0~109 异或）
 * Byte 111    : 帧尾 0x55
 */
#define USB_PKG_CHECKSUM_OFFSET  110   // 校验和字节位置
#define USB_PKG_FOOTER_OFFSET    111   // 帧尾字节位置
#define USB_PKG_CHECKSUM_LEN     110   // 参与校验和计算的字节数（Byte 0~109）

/* USB命令类型 */
#define CMD_TYPE_CONTROL     0x10    // 电机控制
#define CMD_TYPE_CONFIG      0x11    // 配置指令
#define CMD_TYPE_QUERY       0x12    // 查询指令

/* ==================== 电机控制数据8字节格式 ==================== */
/* 位域定义（64位，Big-Endian） */
#define MOTOR_MODE_FORCE_POSITION  0x00    // 力位混控模式

/* 电机控制参数物理值范围 (EC-A6408-P2-25型号) */
#define KP_MIN      0.0f
#define KP_MAX      500.0f
#define KD_MIN      0.0f
#define KD_MAX      5.0f
#define POS_MIN     -12.5f   // rad
#define POS_MAX     12.5f    // rad
#define VEL_MIN     -18.0f   // rad/s
#define VEL_MAX     18.0f    // rad/s
#define TORQUE_MIN  -30.0f   // Nm
#define TORQUE_MAX  30.0f    // Nm

/* ==================== 电机反馈数据定义 ==================== */
/* 错误码定义 */
typedef enum {
    MOTOR_ERR_OK = 0,           // 正常
    MOTOR_ERR_OVERHEAT = 1,     // 过热
    MOTOR_ERR_OVERCURRENT = 2,  // 过流
    MOTOR_ERR_OVERVOLTAGE = 3,  // 电压过高
    MOTOR_ERR_UNDERVOLTAGE = 4, // 电压过低
    MOTOR_ERR_ENCODER = 5,      // 编码器错误
    MOTOR_ERR_BRAKE = 6,        // 刹车电压异常
    MOTOR_ERR_DRV = 7           // DRV驱动错误
} MotorErrorCode;

/* ==================== 数据结构定义 ==================== */
/* 电机控制参数结构体（使用物理值） */
typedef struct {
    float kp;            // KP参数，物理值 0~500
    float kd;            // KD参数，物理值 0~5
    float position;      // 期望位置，rad -12.5~+12.5
    float velocity;      // 期望速度，rad/s -18~+18
    float torque;        // 前馈扭矩，Nm -30~+30
} MotorControlParam;

/* 电机反馈状态结构体 */
typedef struct {
    uint8_t error_code;   // 错误码
    float position;       // 实际位置，rad
    float velocity;       // 实际速度，rad/s
    float current;        // 实际电流，A (-30~+30)
    float temperature;    // 温度，℃
    uint32_t timestamp;   // 时间戳
} MotorFeedback;

/* USB控制数据包结构体 */
typedef struct {
    uint8_t header;               // 帧头 0xAA
    uint8_t cmd_type;             // 命令类型
    uint16_t length;              // 有效数据长度（大端序）
    uint8_t sequence;             // 序列号
    uint8_t reserved;              // 保留字节
    MotorControlParam motor[12];  // 12个电机控制参数
    uint8_t reserved2[8];         // 保留字节
    uint8_t checksum;             // XOR校验和
    uint8_t footer;               // 帧尾 0x55
} USB_ControlPkg;

/* USB反馈数据包结构体 */
typedef struct {
    uint8_t header;               // 帧头 0xAA
    uint8_t cmd_type;             // 命令类型
    uint16_t length;              // 有效数据长度（大端序）
    uint8_t sequence;             // 序列号
    uint8_t reserved;              // 保留字节
    MotorFeedback motor[12];      // 12个电机反馈状态
    uint8_t reserved2[8];         // 保留字节
    uint8_t checksum;             // XOR校验和
    uint8_t footer;               // 帧尾 0x55
} USB_FeedbackPkg;

/* SPI数据包结构体（主控→从控，传输9个电机数据） */
#define SPI_PKG_MOTOR_CNT     9       // 从控控制的电机数量
#define SPI_PKG_TOTAL_LEN     (6 + SPI_PKG_MOTOR_CNT * 8 + 2)  // 帧头+命令+数据+校验+帧尾

typedef struct {
    uint8_t header;               // 帧头 0xAA
    uint8_t cmd_type;             // 命令类型
    uint8_t motor_cnt;            // 电机数量
    uint8_t sequence;              // 序列号
    uint8_t reserved[2];          // 保留字节
    MotorControlParam motor[SPI_PKG_MOTOR_CNT];  // 9个电机控制参数
    uint8_t checksum;             // XOR校验和
    uint8_t footer;               // 帧尾 0x55
} SPI_SlavePkg;

/* FDCAN消息结构体 */
typedef struct {
    uint32_t std_id;               // 标准ID
    uint8_t data[8];               // 数据
    uint8_t dlc;                   // 数据长度
} FDCAN_Message;

/* ==================== 函数声明 ==================== */
/* 物理值与CAN原始值换算函数 */
/**
 * @brief KP物理值转CAN原始值
 * @param kp_phys 物理值 (0~500)
 * @return CAN原始值 (0~4095)
 */
uint16_t Motor_KP_PhysToRaw(float kp_phys);

/**
 * @brief CAN原始值转KP物理值
 * @param kp_raw CAN原始值 (0~4095)
 * @return 物理值 (0~500)
 */
float Motor_KP_RawToPhys(uint16_t kp_raw);

/**
 * @brief KD物理值转CAN原始值
 * @param kd_phys 物理值 (0~5)
 * @return CAN原始值 (0~511)
 */
uint16_t Motor_KD_PhysToRaw(float kd_phys);

/**
 * @brief CAN原始值转KD物理值
 * @param kd_raw CAN原始值 (0~511)
 * @return 物理值 (0~5)
 */
float Motor_KD_RawToPhys(uint16_t kd_raw);

/**
 * @brief 位置物理值转CAN原始值
 * @param pos_phys 物理值 (-12.5~+12.5 rad)
 * @return CAN原始值 (0~65535)
 */
uint16_t Motor_Pos_PhysToRaw(float pos_phys);

/**
 * @brief CAN原始值转位置物理值
 * @param pos_raw CAN原始值 (0~65535)
 * @return 物理值 (-12.5~+12.5 rad)
 */
float Motor_Pos_RawToPhys(uint16_t pos_raw);

/**
 * @brief 速度物理值转CAN原始值
 * @param vel_phys 物理值 (-18~+18 rad/s)
 * @return CAN原始值 (0~4095)
 */
uint16_t Motor_Vel_PhysToRaw(float vel_phys);

/**
 * @brief CAN原始值转速度物理值
 * @param vel_raw CAN原始值 (0~4095)
 * @return 物理值 (-18~+18 rad/s)
 */
float Motor_Vel_RawToPhys(uint16_t vel_raw);

/**
 * @brief 扭矩物理值转CAN原始值
 * @param torque_phys 物理值 (-30~+30 Nm)
 * @return CAN原始值 (0~4095)
 */
uint16_t Motor_Torque_PhysToRaw(float torque_phys);

/**
 * @brief CAN原始值转扭矩物理值
 * @param torque_raw CAN原始值 (0~4095)
 * @return 物理值 (-30~+30 Nm)
 */
float Motor_Torque_RawToPhys(uint16_t torque_raw);

/* 电机控制参数打包/解包函数 */
/**
 * @brief 将电机控制参数打包为8字节CAN数据
 * @param param 电机控制参数（物理值）
 * @param can_data 8字节CAN数据缓冲区
 */
void Motor_PackControlData(const MotorControlParam *param, uint8_t *can_data);

/**
 * @brief 从8字节CAN数据解析电机控制参数
 * @param can_data 8字节CAN数据
 * @param param 电机控制参数结构体（物理值）
 */
void Motor_UnpackControlData(const uint8_t *can_data, MotorControlParam *param);

/**
 * @brief 从8字节CAN数据解析电机反馈状态
 * @param can_data 8字节CAN数据
 * @param feedback 电机反馈状态结构体
 */
void Motor_UnpackFeedbackData(const uint8_t *can_data, MotorFeedback *feedback);

/* 电机ID辅助函数 */
/**
 * @brief 获取电机的CAN ID
 * @param motor_id 电机物理ID (0x01~0x0C)
 * @return CAN ID (等于 motor_id)
 */
uint32_t Motor_GetMasterID(uint8_t motor_id);

/**
 * @brief 判断电机是否由主控FDCAN1直接控制
 * @param motor_id 电机物理ID
 * @return 1=主控直控，0=从控控制
 */
uint8_t Motor_IsMasterControlled(uint8_t motor_id);

/* 错误码字符串转换 */
/**
 * @brief 获取错误码对应的字符串描述
 * @param err_code 错误码
 * @return 错误描述字符串
 */
const char* Motor_GetErrorString(uint8_t err_code);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CONTROL_H */
