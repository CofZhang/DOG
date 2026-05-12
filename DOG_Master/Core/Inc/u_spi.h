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
//#define SPI_PKG_TOTAL_LEN    (SPI_PKG_HEADER_LEN + SPI_PKG_DATA_LEN + 2)  // 总长度：包头+数据+校验+帧尾

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

/**
 * @brief 初始化SPI通信模块
 */
void SPI_Protocol_Init(void);

/**
 * @brief 打包并通过SPI发送电机控制数据到从控
 * @param motor_params 12个电机的控制参数数组
 * @param sequence 序列号
 * @return 1=成功，0=失败
 * @note 只会发送电机4-12（索引3-11）的数据给从控
 */
uint8_t SPI_SendMotorControl(const MotorControlParam motor_params[12], uint8_t sequence);

/**
 * @brief 打包SPI数据包（电机4-12的控制数据）
 * @param motor_params 12个电机的控制参数数组
 * @param sequence 序列号
 * @param tx_buffer 输出缓冲区
 * @return 打包后的数据长度
 */
uint16_t SPI_PackMotorData(const MotorControlParam motor_params[12], uint8_t sequence, uint8_t *tx_buffer);

/**
 * @brief 获取SPI传输状态
 * @return 当前传输状态
 */
SPI_TransferState SPI_GetTransferState(void);

/**
 * @brief SPI传输完成回调（在DMA传输完成中断中调用）
 */
void SPI_TxCpltCallback(void);

/**
 * @brief SPI传输错误回调（在DMA传输错误中断中调用）
 */
void SPI_ErrorCallback(void);

#ifdef __cplusplus
}
#endif

#endif // __U_SPI_H
