/**
 * @file protocol.c
 * @brief USB通信协议处理实现
 * @note 本文件实现USB数据包的解析、打包、校验等功能
 */

#include "usbcol.h"
#include "usbd_cdc_if.h"
#include "Led_indicator.h"
#include <string.h>
#include <stdio.h>

/* ==================== 私有变量 ==================== */
/* 接收缓冲区（注意：如需外部访问，请在头文件中声明extern） */
 uint8_t g_rx_buffer[USB_RX_BUFFER_SIZE];
static volatile uint16_t g_rx_write_idx = 0;
static volatile uint16_t g_rx_read_idx = 0; 

/* 协议状态 */
static volatile ProtocolState g_protocol_state = PROTOCOL_STATE_IDLE; 

/* 错误统计 */
static struct {
    uint32_t header_errors;      // 帧头错误
    uint32_t footer_errors;      // 帧尾错误
    uint32_t checksum_errors;    // 校验和错误
    uint32_t length_errors;      // 长度错误
    uint32_t total_packets;     // 总包数
    uint32_t valid_packets;      // 有效包数
} g_protocol_stats = {0};

/* USB CDC发送锁定标志 */
static volatile uint8_t g_usb_tx_busy = 0;

/* ==================== USB CDC回调变量引用 ==================== */
/* 声明外部CDC接收回调（由CubeMX生成） */
extern uint8_t CDC_Transmit_FS(uint8_t* Buf, uint16_t Len);

/* ==================== USB协议初始化 ==================== */
void Protocol_Init(void)
{
    /* 清空接收缓冲区 */
    memset(g_rx_buffer, 0, sizeof(g_rx_buffer));
    g_rx_write_idx = 0;
    g_rx_read_idx = 0;

    /* 重置协议状态 */
    g_protocol_state = PROTOCOL_STATE_IDLE;

    /* 重置错误统计 */
    memset(&g_protocol_stats, 0, sizeof(g_protocol_stats));
}

/* ==================== USB数据包解析 ==================== */
/**
 * @brief  解析USB控制数据包
 * @param  rx_data: 接收到的原始数据包缓冲区
 * @param  rx_len: 数据包长度
 * @param  out_pkg: 输出参数，解析后的12个电机控制参数数组
 * @param  out_sequence: 输出参数，包序列号
 * @retval ProtocolState: 解析状态（成功/错误/完成）
 *
 * @note 数据包格式: [帧头(1B)][序列号(1B)][长度(2B)][数据(108B)][校验和(1B)][帧尾(1B)]
 *       总计 114 字节 (USB_PKG_TOTAL_LEN)
 */
ProtocolState Protocol_ParseControlPkg(const uint8_t *rx_data, uint16_t rx_len,
                                       MotorControlParam out_pkg[], uint8_t *out_sequence)
{
    /* 第1步：检查最小长度，确保数据包完整 */
    if (rx_len < USB_PKG_TOTAL_LEN) {
        g_protocol_stats.length_errors++;
        return PROTOCOL_STATE_ERROR;
    }

    /* 第2步：检查帧头标识（通常为0xAA或特定magic byte） */
    if (rx_data[0] != USB_PKG_HEADER) {
        g_protocol_stats.header_errors++;
        return PROTOCOL_STATE_ERROR;
    }

    /* 第3步：检查帧尾标识，帧尾固定在 Byte 111 */
    if (rx_data[USB_PKG_FOOTER_OFFSET] != USB_PKG_FOOTER) {
        g_protocol_stats.footer_errors++;
        return PROTOCOL_STATE_ERROR;
    }

    /* 第4步：验证校验和（对 Byte 0~109 共110字节异或，结果存于 Byte 110） */
    uint8_t calc_checksum = Protocol_CalcChecksum(rx_data, USB_PKG_CHECKSUM_LEN);
    uint8_t pkt_checksum = rx_data[USB_PKG_CHECKSUM_OFFSET];

    if (calc_checksum != pkt_checksum) {
        g_protocol_stats.checksum_errors++;
        return PROTOCOL_STATE_ERROR;
    }

    /* 第5步：解析包头结构体，获取数据长度信息 */
    /* length字段为大端序，需要字节交换 */
    uint16_t data_length = (rx_data[2] << 8) | rx_data[3];


    /* 第6步：验证解析出的数据长度是否符合预期 */
    if (data_length != USB_PKG_DATA_LEN) {
        g_protocol_stats.length_errors++;
        return PROTOCOL_STATE_ERROR;
    }

    /* 第7步：提取序列号（用于数据包顺序追踪和丢包检测） */
    if (out_sequence != NULL) {
        *out_sequence = rx_data[4];
    }

    /* 第8步：定位电机数据区（跳过6字节包头）并解析12个电机控制数据 */
    const uint8_t *motor_data_ptr = rx_data + 6;  /* 跳过6字节包头 */

    for (int i = 0; i < USB_PKG_MOTOR_CNT; i++) {
        if (out_pkg != NULL) {
            /* 每个电机占8字节，调用Motor_UnpackControlData解包 */
            Motor_UnpackControlData(motor_data_ptr + i * USB_PKG_MOTOR_DATA_LEN, &out_pkg[i]);
        }
    }

    /* 第9步：更新协议统计计数器和状态 */
    g_protocol_stats.total_packets++;
    g_protocol_stats.valid_packets++;

    return PROTOCOL_STATE_COMPLETE;
}

uint8_t Protocol_CheckPacket(const uint8_t *rx_data, uint16_t rx_len)
{
    /* 检查最小长度 */
    if (rx_len < USB_PKG_TOTAL_LEN) {
        return 0;
    }

    /* 检查帧头和帧尾 */
    if (rx_data[0] != USB_PKG_HEADER) {
        return 0;
    }

    if (rx_data[USB_PKG_FOOTER_OFFSET] != USB_PKG_FOOTER) {
        return 0;
    }

    /* 验证校验和 */
    return Protocol_VerifyChecksum(rx_data, USB_PKG_TOTAL_LEN);
}

/* ==================== 校验和计算 ==================== */
uint8_t Protocol_CalcChecksum(const uint8_t *data, uint16_t len)
{
    uint8_t checksum = 0;

    for (uint16_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }

    return checksum;
}

uint8_t Protocol_VerifyChecksum(const uint8_t *data, uint16_t len)
{
    if (len < USB_PKG_FOOTER_OFFSET + 1) {
        return 0;
    }

    /* 对 Byte 0~109 做异或，与 Byte 110 比较 */
    uint8_t calc_checksum = Protocol_CalcChecksum(data, USB_PKG_CHECKSUM_LEN);
    return (calc_checksum == data[USB_PKG_CHECKSUM_OFFSET]) ? 1 : 0;
}

/* ==================== USB数据包打包 ==================== */
uint16_t Protocol_PackFeedbackPkg(const MotorFeedback motor_feedback[], uint8_t sequence, uint8_t *tx_data)
{
    if (tx_data == NULL) {
        return 0;
    }

    /* 清空发送缓冲区 */
    memset(tx_data, 0, USB_PKG_TOTAL_LEN);

    /* 打包包头 */
    tx_data[0] = USB_PKG_HEADER;           // 帧头
    tx_data[1] = CMD_TYPE_CONTROL;         // 命令类型（反馈）
    tx_data[2] = (USB_PKG_DATA_LEN >> 8) & 0xFF;  // 长度高字节
    tx_data[3] = USB_PKG_DATA_LEN & 0xFF;          // 长度低字节
    tx_data[4] = sequence;                 // 序列号
    tx_data[5] = 0x00;                      // 保留字节

    /* 打包12个电机的反馈数据 */
    uint8_t *feedback_ptr = tx_data + 6;

    for (int i = 0; i < USB_PKG_MOTOR_CNT; i++) {
        MotorFeedback fb = motor_feedback[i];

        /* Byte0: 报文类型(bit7-5)=0x01, 错误码(bit4-0) */
        feedback_ptr[i * 8 + 0] = (0x01 << 5) | (fb.error_code & 0x1F);

        /* Byte1~2: Position uint16 */
        uint16_t pos_raw = Motor_Pos_PhysToRaw(fb.position);
        feedback_ptr[i * 8 + 1] = (pos_raw >> 8) & 0xFF;
        feedback_ptr[i * 8 + 2] = pos_raw & 0xFF;

        /* Byte3 + Byte4高4位: Velocity uint12 */
        uint16_t vel_raw = Motor_Vel_PhysToRaw(fb.velocity);
        feedback_ptr[i * 8 + 3] = (vel_raw >> 4) & 0xFF;

        /* Byte4低4位 + Byte5: Current uint12, 范围 -60~+60A */
        uint16_t current_raw = (uint16_t)((fb.current + 60.0f) / 120.0f * 4095.0f + 0.5f);
        if (current_raw > 4095) current_raw = 4095;
        feedback_ptr[i * 8 + 4] = ((vel_raw & 0x0F) << 4) | ((current_raw >> 8) & 0x0F);
        feedback_ptr[i * 8 + 5] = current_raw & 0xFF;

        /* Byte6: MotorTemp, raw = 实际温度*2 + 50 */
        feedback_ptr[i * 8 + 6] = (uint8_t)(fb.temperature * 2.0f + 50.0f);

        /* Byte7: MOSTemp, raw = 实际温度*2 + 50 */
        feedback_ptr[i * 8 + 7] = (uint8_t)(fb.mos_temperature * 2.0f + 50.0f);
    }

    /* 计算校验和（Byte 0~109 异或，写入 Byte 110） */
    uint8_t checksum = Protocol_CalcChecksum(tx_data, USB_PKG_CHECKSUM_LEN);
    tx_data[USB_PKG_CHECKSUM_OFFSET] = checksum;

    /* 帧尾写入 Byte 111 */
    tx_data[USB_PKG_FOOTER_OFFSET] = USB_PKG_FOOTER;

    return USB_PKG_TOTAL_LEN;
}

uint16_t Protocol_PackControlPkg(const MotorControlParam motor_params[], uint8_t sequence, uint8_t *tx_data)
{
    if (tx_data == NULL) {
        return 0;
    }

    /* 清空发送缓冲区 */
    memset(tx_data, 0, USB_PKG_TOTAL_LEN);

    /* 打包包头 */
    tx_data[0] = USB_PKG_HEADER;           // 帧头
    tx_data[1] = CMD_TYPE_CONTROL;         // 命令类型
    tx_data[2] = (USB_PKG_DATA_LEN >> 8) & 0xFF;  // 长度高字节
    tx_data[3] = USB_PKG_DATA_LEN & 0xFF;          // 长度低字节
    tx_data[4] = sequence;                 // 序列号
    tx_data[5] = 0x00;                      // 保留字节

    /* 打包12个电机的控制数据 */
    uint8_t *motor_data_ptr = tx_data + 6;

    for (int i = 0; i < USB_PKG_MOTOR_CNT; i++) {
        Motor_PackControlData(&motor_params[i], motor_data_ptr + i * USB_PKG_MOTOR_DATA_LEN);
    }

    /* 计算校验和（Byte 0~109 异或，写入 Byte 110） */
    uint8_t checksum = Protocol_CalcChecksum(tx_data, USB_PKG_CHECKSUM_LEN);
    tx_data[USB_PKG_CHECKSUM_OFFSET] = checksum;

    /* 帧尾写入 Byte 111 */
    tx_data[USB_PKG_FOOTER_OFFSET] = USB_PKG_FOOTER;

    return USB_PKG_TOTAL_LEN;
}

/* ==================== USB数据发送 ==================== */
uint8_t Protocol_SendData(const uint8_t *data, uint16_t len)
{
    if (g_usb_tx_busy) {
        return 0;  // USB忙，返回失败
    }

    if (data == NULL || len == 0 || len > USB_TX_BUFFER_SIZE) {
        return 0;
    }

    g_usb_tx_busy = 1;

    /* 调用CDC发送函数 */
    if (CDC_Transmit_FS((uint8_t*)data, len) == USBD_OK) {
        return 1;
    }

    g_usb_tx_busy = 0;
    return 0;
}

uint8_t Protocol_SendFeedback(const MotorFeedback motor_feedback[], uint8_t sequence)
{
    static uint8_t tx_buffer[USB_TX_BUFFER_SIZE];

    uint16_t len = Protocol_PackFeedbackPkg(motor_feedback, sequence, tx_buffer);

    if (len > 0) {
        return Protocol_SendData(tx_buffer, len);
    }

    return 0;
}

/* ==================== USB发送完成回调 ==================== */
/**
 * @brief USB CDC发送完成回调
 * @note 此函数应在CDC发送完成中断或回调中调用
 */
void Protocol_USB_TxCpltCallback(void)
{
    g_usb_tx_busy = 0;
}

/* ==================== USB接收数据处理 ==================== */
/**
 * @brief 处理USB接收到的数据
 * @param data 接收数据缓冲区
 * @param len 接收数据长度
 * @return 处理结果
 *
 * @note 此函数应在CDC接收回调中调用
 */
uint16_t Protocol_ProcessRxData(const uint8_t *data, uint16_t len)
{
    uint16_t processed = 0;

    for (uint16_t i = 0; i < len; i++) {
        /* 计算缓冲区剩余空间 */
        uint16_t free_space = (g_rx_read_idx <= g_rx_write_idx) ?
                              (USB_RX_BUFFER_SIZE - g_rx_write_idx + g_rx_read_idx - 1) :
                              (g_rx_read_idx - g_rx_write_idx - 1);

        if (free_space > 0) {
            g_rx_buffer[g_rx_write_idx] = data[i];
            g_rx_write_idx = (g_rx_write_idx + 1) % USB_RX_BUFFER_SIZE;
            processed++;
        }
    }

    /* 有数据写入则触发LED2闪烁 */
    if (processed > 0) {
        LED_Indicator_Trigger(LED_IND_USB_RX);
    }

    return processed;
}

/**
 * @brief 检查是否有完整的包可读
 * @return 1=有完整包，0=无
 */
uint8_t Protocol_HasCompletePacket(void)
{
    /* 检查是否有至少最小长度的数据 */
    uint16_t data_len = (g_rx_write_idx >= g_rx_read_idx) ?
                        (g_rx_write_idx - g_rx_read_idx) :
                        (USB_RX_BUFFER_SIZE - g_rx_read_idx + g_rx_write_idx);

    if (data_len < USB_PKG_TOTAL_LEN) {
        return 0;
    }

    /* 检查帧头 */
    if (g_rx_buffer[g_rx_read_idx] != USB_PKG_HEADER) {
        /* 帧头不对，移动读指针 */
        g_rx_read_idx = (g_rx_read_idx + 1) % USB_RX_BUFFER_SIZE;
        g_protocol_stats.header_errors++;
        return 0;
    }

    return 1;
}

/**
 * @brief 读取并解析一个完整的包
 * @param out_pkg 输出电机控制参数数组
 * @param out_sequence 输出序列号
 * @return 解析结果状态
 */
/**
 * @brief  从环形缓冲区读取并解析一个完整的USB数据包
 * @param  out_pkg: 输出参数，解析后的12个电机控制参数数组
 * @param  out_sequence: 输出参数，包序列号
 * @retval ProtocolState: 读取状态
 *
 * @note 此函数执行三个操作：
 *       1. 检查环形缓冲区中是否有完整数据包
 *       2. 将整个数据包复制到临时缓冲区（避免解析时数据被覆盖）
 *       3. 调用Protocol_ParseControlPkg解析数据包内容
 */
ProtocolState Protocol_ReadPacket(MotorControlParam out_pkg[], uint8_t *out_sequence)
{
    /* 第1步：检查环形缓冲区中是否有完整的USB数据包可读 */
    if (!Protocol_HasCompletePacket()) {
        return PROTOCOL_STATE_IDLE;  /* 无完整数据包，返回空闲状态 */
    }

    /* 第2步：创建静态临时缓冲区，存储待解析的数据包 */
    /* 使用static避免每次调用分配栈空间，同时保证缓冲区持久化 */
    static uint8_t packet_buffer[USB_PKG_TOTAL_LEN];
    uint16_t idx = g_rx_read_idx;  /* 保存当前读指针位置 */

    /* 第3步：从环形缓冲区连续读取USB_PKG_TOTAL_LEN字节到临时缓冲区 */
    /* 使用环形索引遍历，自动处理缓冲区边界回绕 */
    for (int i = 0; i < USB_PKG_TOTAL_LEN; i++) {
        packet_buffer[i] = g_rx_buffer[idx];
        idx = (idx + 1) % USB_RX_BUFFER_SIZE;  /* 环形索引递增 */
    }

    /* 第4步：调用解析函数处理临时缓冲区中的数据包 */
    ProtocolState result = Protocol_ParseControlPkg(packet_buffer, USB_PKG_TOTAL_LEN, out_pkg, out_sequence);

    /* 第5步：根据解析结果更新读指针
     * 成功：跳过整包（164字节）
     * 失败：只跳过1字节，让下次重新从下一个字节寻找帧头0xAA */
    if (result == PROTOCOL_STATE_COMPLETE) {
        g_rx_read_idx = (g_rx_read_idx + USB_PKG_TOTAL_LEN) % USB_RX_BUFFER_SIZE;
    } else {
        g_rx_read_idx = (g_rx_read_idx + 1) % USB_RX_BUFFER_SIZE;
    }

    return result;
}

/* ==================== 协议状态管理 ==================== */
ProtocolState Protocol_GetState(void)
{
    return g_protocol_state;
}

void Protocol_Reset(void)
{
    g_rx_read_idx = 0;
    g_rx_write_idx = 0;
    g_protocol_state = PROTOCOL_STATE_IDLE;
}

/* ==================== 调试函数 ==================== */
void Protocol_DumpPacket(const uint8_t *data, uint16_t len)
{
    printf("[PROTOCOL] Packet dump (%d bytes):\r\n", len);

    for (uint16_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);

        if ((i + 1) % 16 == 0) {
            printf("\r\n");
        }
    }

    printf("\r\n");
}

const char* Protocol_GetErrorStats(void)
{
    static char stats_str[256];
    snprintf(stats_str, sizeof(stats_str),
             "Header Err:%u, Footer Err:%u, Checksum Err:%u, Length Err:%u, Total:%u, Valid:%u",
             g_protocol_stats.header_errors,
             g_protocol_stats.footer_errors,
             g_protocol_stats.checksum_errors,
             g_protocol_stats.length_errors,
             g_protocol_stats.total_packets,
             g_protocol_stats.valid_packets);
    return stats_str;
}

uint16_t Protocol_GetRxFreeSpace(void)
{
    uint16_t used = (g_rx_write_idx >= g_rx_read_idx) ?
                    (g_rx_write_idx - g_rx_read_idx) :
                    (USB_RX_BUFFER_SIZE - g_rx_read_idx + g_rx_write_idx);

    return USB_RX_BUFFER_SIZE - used - 1;
}

void Protocol_ClearRxBuffer(void)
{
    g_rx_read_idx = 0;
    g_rx_write_idx = 0;
}
