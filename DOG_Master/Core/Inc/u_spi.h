/**
 * @file u_spi.h
 * @brief SPI通信模块 - 主控向从控传输电机控制数据
 * @note 主控通过SPI将电机4-12的控制数据传输给从控STM32G474
 */

#ifndef __U_SPI_H
#define __U_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "spi.h"
#include "motor.h"

/* ==================== SPI数据包定义 ==================== */
#define SPI_PKG_HEADER       0xAA    // SPI帧头标识
#define SPI_PKG_FOOTER       0x55    // SPI帧尾标识
#define SPI_PKG_MOTOR_CNT    9       // 从控控制的电机数量（电机4-12）
#define SPI_PKG_HEADER_LEN   6       // 包头长度
#define SPI_PKG_DATA_LEN     (SPI_PKG_MOTOR_CNT * 8)  // 数据长度：9个电机×8字节

/* SPI命令类型 */
#define SPI_CMD_MOTOR_CONTROL  0x10  // 电机控制命令

/* SPI传输状态 */
typedef enum {
    SPI_STATE_IDLE = 0,       // 空闲
    SPI_STATE_BUSY,           // 传输中
    SPI_STATE_COMPLETE,       // 传输完成
    SPI_STATE_ERROR           // 传输错误
} SPI_TransferState;

/* ==================== 函数声明 ==================== */
void SPI_Protocol_Init(void);
uint8_t SPI_SendMotorControl(const MotorControlParam motor_params[12], uint8_t sequence);
uint16_t SPI_PackMotorData(const MotorControlParam motor_params[12], uint8_t sequence, uint8_t *tx_buffer);
SPI_TransferState SPI_GetTransferState(void);
void SPI_TxCpltCallback(void);
void SPI_ErrorCallback(void);

/**
 * @brief 获取从机9个电机的反馈数据（电机4-12）
 * @param feedback_out 输出数组，长度为 SPI_PKG_MOTOR_CNT
 */
void SPI_GetSlaveFeedback(MotorFeedback feedback_out[SPI_PKG_MOTOR_CNT]);

#ifdef __cplusplus
}
#endif

#endif // __U_SPI_H
