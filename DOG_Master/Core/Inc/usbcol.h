#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor.h"

/* ==================== USB协议处理状态 ==================== */
typedef enum {
    PROTOCOL_STATE_IDLE = 0,          // 空闲状态，等待帧头
    PROTOCOL_STATE_RECEIVING,          // 正在接收数据
    PROTOCOL_STATE_PARSING,           // 正在解析数据
    PROTOCOL_STATE_COMPLETE,          // 数据接收完成
    PROTOCOL_STATE_ERROR              // 协议错误
} ProtocolState;

/* ==================== USB接收缓冲区 ==================== */
#define USB_RX_BUFFER_SIZE   512      // 接收缓冲区大小（需容纳至少3个完整包：164×3=492）
#define USB_TX_BUFFER_SIZE   256      // 发送缓冲区大小

/* USB数据包包头结构 */
typedef struct {
    uint8_t header;       // 帧头 0xAA
    uint8_t cmd_type;     // 命令类型
    uint16_t length;     // 有效数据长度（大端序）
    uint8_t sequence;     // 序列号
    uint8_t reserved;     // 保留字节
} __attribute__((packed)) USB_PkgHeader;

/* ==================== 函数声明 ==================== */
/* USB协议初始化 */
/**
 * @brief 初始化USB协议处理模块
 */
void Protocol_Init(void);

/* USB数据包解析 */
/**
 * @brief 解析USB控制数据包
 * @param rx_data 接收到的原始数据
 * @param rx_len 接收数据长度
 * @param out_pkg 输出解析后的控制参数数组
 * @param out_sequence 输出序列号
 * @return 解析结果状态
 */
ProtocolState Protocol_ParseControlPkg(const uint8_t *rx_data, uint16_t rx_len,
                                       MotorControlParam out_pkg[], uint8_t *out_sequence);

/**
 * @brief 检查USB数据包格式
 * @param rx_data 接收到的原始数据
 * @param rx_len 接收数据长度
 * @return 1=有效包，0=无效包
 */
uint8_t Protocol_CheckPacket(const uint8_t *rx_data, uint16_t rx_len);

/**
 * @brief 计算XOR校验和
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 校验和
 */
uint8_t Protocol_CalcChecksum(const uint8_t *data, uint16_t len);

/**
 * @brief 验证XOR校验和
 * @param data 数据缓冲区（包含校验和）
 * @param len 数据长度（包含校验和字节）
 * @return 1=校验通过，0=校验失败
 */
uint8_t Protocol_VerifyChecksum(const uint8_t *data, uint16_t len);

/* USB数据包打包 */
/**
 * @brief 打包USB反馈数据包
 * @param motor_feedback 12个电机的反馈状态
 * @param sequence 序列号
 * @param tx_data 输出数据缓冲区
 * @return 打包后的数据长度
 */
uint16_t Protocol_PackFeedbackPkg(const MotorFeedback motor_feedback[], uint8_t sequence, uint8_t *tx_data);

/**
 * @brief 打包单电机控制数据到USB包格式
 * @param motor_params 电机控制参数数组
 * @param sequence 序列号
 * @param tx_data 输出数据缓冲区
 * @return 打包后的数据长度
 */
uint16_t Protocol_PackControlPkg(const MotorControlParam motor_params[], uint8_t sequence, uint8_t *tx_data);

/* USB数据发送 */
/**
 * @brief 通过USB CDC发送数据
 * @param data 数据缓冲区
 * @param len 数据长度
 * @return 发送状态
 */
uint8_t Protocol_SendData(const uint8_t *data, uint16_t len);

/**
 * @brief 发送电机反馈数据包
 * @param motor_feedback 电机反馈状态数组
 * @param sequence 序列号
 * @return 发送状态
 */
uint8_t Protocol_SendFeedback(const MotorFeedback motor_feedback[], uint8_t sequence);

/* 协议状态管理 */
/**
 * @brief 获取当前协议处理状态
 * @return 当前状态
 */
ProtocolState Protocol_GetState(void);

/**
 * @brief 重置协议处理状态
 */
void Protocol_Reset(void);

/* 调试函数 */
/**
 * @brief 打印数据包内容（用于调试）
 * @param data 数据缓冲区
 * @param len 数据长度
 */
void Protocol_DumpPacket(const uint8_t *data, uint16_t len);

/**
 * @brief 获取协议错误计数字符串
 * @return 错误统计字符串
 */
const char* Protocol_GetErrorStats(void);

/* 缓冲区管理 */
/**
 * @brief 获取接收缓冲区剩余空间
 * @return 剩余空间字节数
 */
uint16_t Protocol_GetRxFreeSpace(void);

/**
 * @brief 清空接收缓冲区
 */
void Protocol_ClearRxBuffer(void);

/* ==================== 额外全局函数声明 ==================== */
/**
 * @brief USB CDC发送完成回调
 */
void Protocol_USB_TxCpltCallback(void);

/**
 * @brief 处理USB接收到的数据
 * @param data 接收数据缓冲区
 * @param len 接收数据长度
 * @return 实际处理的字节数
 */
uint16_t Protocol_ProcessRxData(const uint8_t *data, uint16_t len);

/**
 * @brief 检查是否有完整的包可读
 * @return 1=有完整包，0=无
 */
uint8_t Protocol_HasCompletePacket(void);

/**
 * @brief 读取并解析一个完整的包
 * @param out_pkg 输出电机控制参数数组
 * @param out_sequence 输出序列号
 * @return 解析结果状态
 */
ProtocolState Protocol_ReadPacket(MotorControlParam out_pkg[], uint8_t *out_sequence);

#ifdef __cplusplus
}
#endif

#endif /* __PROTOCOL_H */
