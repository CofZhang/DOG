/**
 * @file u_spi.c
 * @brief SPI通信模块实现 - 主控向从控传输电机控制数据
 * @note 主控通过SPI将电机4-12的控制数据传输给从控STM32G474
 */

#include "u_spi.h"
#include "usbcol.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* SPI发送缓冲区 */
static uint8_t g_spi_tx_buffer[SPI_PKG_TOTAL_LEN];

/* SPI传输状态 */
static volatile SPI_TransferState g_spi_state = SPI_STATE_IDLE;

/* 外部SPI句柄（在spi.c中定义） */
extern SPI_HandleTypeDef hspi1;

/* ==================== SPI协议初始化 ==================== */
void SPI_Protocol_Init(void)
{
    /* 清空发送缓冲区 */
    memset(g_spi_tx_buffer, 0, sizeof(g_spi_tx_buffer));

    /* 重置传输状态 */
    g_spi_state = SPI_STATE_IDLE;
}

/* ==================== SPI数据包打包 ==================== */
uint16_t SPI_PackMotorData(const MotorControlParam motor_params[12], uint8_t sequence, uint8_t *tx_buffer)
{
    if (tx_buffer == NULL || motor_params == NULL) {
        return 0;
    }

    /* 清空缓冲区 */
    memset(tx_buffer, 0, SPI_PKG_TOTAL_LEN);

    /* 打包包头 */
    tx_buffer[0] = SPI_PKG_HEADER;           // 帧头 0xAA
    tx_buffer[1] = SPI_CMD_MOTOR_CONTROL;    // 命令类型
    tx_buffer[2] = SPI_PKG_MOTOR_CNT;        // 电机数量：9
    tx_buffer[3] = sequence;                  // 序列号
    tx_buffer[4] = 0x00;                      // 保留字节
    tx_buffer[5] = 0x00;                      // 保留字节

    /* 打包9个电机的控制数据（电机4-12，对应索引3-11） */
    uint8_t *motor_data_ptr = tx_buffer + SPI_PKG_HEADER_LEN;

    for (int i = 0; i < SPI_PKG_MOTOR_CNT; i++) {
        /* 从motor_params[3]开始取数据（电机4），打包到SPI缓冲区 */
        Motor_PackControlData(&motor_params[i + 3], motor_data_ptr + i * 8);
    }

    /* 计算校验和（从字节0到数据结束，异或校验） */
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < SPI_PKG_TOTAL_LEN - 2; i++) {
        checksum ^= tx_buffer[i];
    }
    tx_buffer[SPI_PKG_TOTAL_LEN - 2] = checksum;

    /* 帧尾 */
    tx_buffer[SPI_PKG_TOTAL_LEN - 1] = SPI_PKG_FOOTER;

    return SPI_PKG_TOTAL_LEN;
}

/* ==================== SPI数据发送 ==================== */
uint8_t SPI_SendMotorControl(const MotorControlParam motor_params[12], uint8_t sequence)
{
    /* 检查SPI是否空闲 */
    if (g_spi_state == SPI_STATE_BUSY) {
        return 0;  // SPI忙，返回失败
    }

    if (motor_params == NULL) {
        return 0;
    }

    /* 打包数据 */
    uint16_t len = SPI_PackMotorData(motor_params, sequence, g_spi_tx_buffer);
    if (len == 0) {
        return 0;
    }

    /* 设置为忙状态 */
    g_spi_state = SPI_STATE_BUSY;

    /* 通过DMA发送数据 */
    /* ⚠️ 注意：如果SPI配置为16位模式，len参数应该是半字数量 */
    /* 当前len=80字节，在16位模式下应该传输 len/2 = 40个半字 */
    uint16_t transfer_len = len / 2;  // 16位模式：字节数 / 2 = 半字数

    if (HAL_SPI_Transmit_DMA(&hspi1, g_spi_tx_buffer, transfer_len) != HAL_OK) {
        g_spi_state = SPI_STATE_ERROR;
        return 0;
    }

    return 1;
}

/* ==================== SPI状态管理 ==================== */
SPI_TransferState SPI_GetTransferState(void)
{
    return g_spi_state;
}

/* ==================== SPI回调函数 ==================== */
/**
 * @brief SPI传输完成回调
 * @note 此函数应在HAL_SPI_TxCpltCallback中调用
 */
void SPI_TxCpltCallback(void)
{
    g_spi_state = SPI_STATE_IDLE;
}

/**
 * @brief SPI传输错误回调
 * @note 此函数应在HAL_SPI_ErrorCallback中调用
 */
void SPI_ErrorCallback(void)
{
    g_spi_state = SPI_STATE_ERROR;
}

/* ==================== HAL回调函数重定向 ==================== */
/**
 * @brief HAL SPI传输完成回调
 * @note 此函数会被HAL库自动调用
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        SPI_TxCpltCallback();
    }
}

/**
 * @brief HAL SPI错误回调
 * @note 此函数会被HAL库自动调用
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1) {
        SPI_ErrorCallback();
    }
}
