/**
 * @file u_spi.c
 * @brief SPI通信模块实现 - 主控向从控传输电机控制数据
 * @note 主控通过SPI将电机4-12的控制数据传输给从控STM32G474
 */

#include "u_spi.h"
#include "usbcol.h"
#include "motor_calib.h"
#include <string.h>

/* ==================== 私有变量 ==================== */
/* SPI发送缓冲区 */
static uint8_t g_spi_tx_buffer[SPI_PKG_TOTAL_LEN];

/* SPI接收缓冲区（全双工：接收从控的反馈数据） */
static uint8_t g_spi_rx_buffer[SPI_PKG_TOTAL_LEN];

/* SPI传输状态 */
static volatile SPI_TransferState g_spi_state = SPI_STATE_IDLE;

/* 从机9个电机反馈缓存（电机4-12，索引0-8） */
static MotorFeedback g_slave_feedback[SPI_PKG_MOTOR_CNT];

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
        /* 从motor_params[3]开始取数据（电机4），应用关节→电机空间变换后打包 */
        MotorControlParam param = motor_params[i + 3];
        Motor_Calib_ApplyTransform(i + 3, &param);
        Motor_PackControlData(&param, motor_data_ptr + i * 8);
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

    /* 通过DMA全双工收发（主控 TX = 控制指令，RX = 从控反馈） */
    /* 8位模式：传输长度 = 字节数 */
    uint16_t transfer_len = len;

    if (HAL_SPI_TransmitReceive_DMA(&hspi1, g_spi_tx_buffer, g_spi_rx_buffer, transfer_len) != HAL_OK) {
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
 * @brief SPI传输完成回调，解析从机反馈包
 */
void SPI_TxCpltCallback(void)
{
    g_spi_state = SPI_STATE_IDLE;

    /* 验证从机反馈包帧头和帧尾 */
    if (g_spi_rx_buffer[0] != SPI_PKG_HEADER ||
        g_spi_rx_buffer[SPI_PKG_TOTAL_LEN - 1] != SPI_PKG_FOOTER) {
        return;
    }

    /* 验证校验和（Byte 0 ~ SPI_PKG_TOTAL_LEN-3 异或，与 SPI_PKG_TOTAL_LEN-2 比较） */
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < SPI_PKG_TOTAL_LEN - 2; i++) {
        checksum ^= g_spi_rx_buffer[i];
    }
    if (checksum != g_spi_rx_buffer[SPI_PKG_TOTAL_LEN - 2]) {
        return;
    }

    /* 解析9个电机反馈数据（从 Byte6 开始，每个电机8字节） */
    const uint8_t *motor_data_ptr = g_spi_rx_buffer + SPI_PKG_HEADER_LEN;
    for (int i = 0; i < SPI_PKG_MOTOR_CNT; i++) {
        Motor_UnpackFeedbackData(motor_data_ptr + i * 8, &g_slave_feedback[i]);
    }
}

/**
 * @brief 获取从机电机反馈数据
 * @param feedback_out 输出数组，9个电机（电机4-12）
 */
void SPI_GetSlaveFeedback(MotorFeedback feedback_out[SPI_PKG_MOTOR_CNT])
{
    memcpy(feedback_out, g_slave_feedback, sizeof(g_slave_feedback));
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
 * @brief HAL SPI发送+接收完成回调（全双工 DMA 传输完成后调用）
 */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
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
