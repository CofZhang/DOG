/**
 * @file motor_calib.c
 * @brief 电机校准模块实现
 */

#include "motor_calib.h"
#include <string.h>
#include <math.h>

/* ==================== 私有变量 ==================== */
static CalibData  g_calib;                              /* 当前校准数据（RAM镜像） */
static CalibState g_calib_state = CALIB_STATE_COLLECTING;

/* 上电采样累加器 */
static float    g_boot_pos_sum[CALIB_MOTOR_CNT];
static uint8_t  g_boot_sample_cnt = 0;
static uint8_t  g_boot_pos_cnt[CALIB_MOTOR_CNT];   /* 各电机有效采样次数 */

/* 电机旋转方向表（与 motor.h 中 MOTOR_DIR_x 宏对应，索引0=电机1） */
static const int8_t g_motor_dir[CALIB_MOTOR_CNT] = {
    MOTOR_DIR_1,  MOTOR_DIR_2,  MOTOR_DIR_3,
    MOTOR_DIR_4,  MOTOR_DIR_5,  MOTOR_DIR_6,
    MOTOR_DIR_7,  MOTOR_DIR_8,  MOTOR_DIR_9,
    MOTOR_DIR_10, MOTOR_DIR_11, MOTOR_DIR_12
};

/* 电机减速比表（与 motor.h 中 MOTOR_RATIO_x 宏对应，索引0=电机1） */
static const float g_motor_ratio[CALIB_MOTOR_CNT] = {
    MOTOR_RATIO_1,  MOTOR_RATIO_2,  MOTOR_RATIO_3,
    MOTOR_RATIO_4,  MOTOR_RATIO_5,  MOTOR_RATIO_6,
    MOTOR_RATIO_7,  MOTOR_RATIO_8,  MOTOR_RATIO_9,
    MOTOR_RATIO_10, MOTOR_RATIO_11, MOTOR_RATIO_12
};

/* ==================== 私有函数声明 ==================== */
static uint32_t Calib_CRC32(const uint8_t *data, uint32_t len);
static int      Calib_FlashWrite(const CalibData *calib);
static int      Calib_FlashRead(CalibData *calib);
static void     Calib_LoadDefaults(CalibData *calib);
static void     Calib_FinalizeBoot(void);
static void     Calib_FinalizeCalib(void);

/* ==================== CRC32（IEEE 802.3多项式） ==================== */
static uint32_t Calib_CRC32(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320UL : (crc >> 1);
        }
    }
    return crc ^ 0xFFFFFFFFUL;
}

/* ==================== Flash读取 ==================== */
static int Calib_FlashRead(CalibData *calib)
{
    /* 直接从Flash地址读取（STM32H7 Flash可直接按字节访问） */
    const uint8_t *src = (const uint8_t *)CALIB_FLASH_ADDR;
    memcpy(calib, src, sizeof(CalibData));

    /* 校验魔数 */
    if (calib->magic != CALIB_MAGIC) {
        return -1;
    }

    /* 校验CRC（对magic + offset + last_boot_pos共100字节） */
    uint32_t crc = Calib_CRC32(src, sizeof(CalibData) - sizeof(uint32_t));
    if (crc != calib->crc32) {
        return -2;
    }

    return 0;
}

/* ==================== Flash写入 ==================== */
static int Calib_FlashWrite(const CalibData *calib)
{
    HAL_StatusTypeDef status;

    /* 解锁Flash */
    status = HAL_FLASH_Unlock();
    if (status != HAL_OK) return -1;

    /* 擦除Sector */
    FLASH_EraseInitTypeDef erase_init;
    erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
    erase_init.Banks        = CALIB_FLASH_BANK;
    erase_init.Sector       = CALIB_FLASH_SECTOR;
    erase_init.NbSectors    = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;

    uint32_t sector_error = 0;
    status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
    if (status != HAL_OK) {
        HAL_FLASH_Lock();
        return -2;
    }

    /* STM32H743 Flash编程单位为256位（32字节），需将数据对齐到32字节后逐块写入 */
    /* CalibData 大小约104字节，需要4个256位块（128字节），不足部分用0xFF填充 */
    #define CALIB_FLASH_WORD_SIZE  32U   /* 256位 = 32字节 */
    uint32_t blocks = (sizeof(CalibData) + CALIB_FLASH_WORD_SIZE - 1) / CALIB_FLASH_WORD_SIZE;

    /* 对齐缓冲区（32字节对齐，不足部分填0xFF）
     * 使用 static 确保编译器将其放在 BSS 段，支持32字节对齐（栈上不支持超过8字节对齐） */
    static __attribute__((aligned(32))) uint8_t aligned_buf[CALIB_FLASH_WORD_SIZE * 4];
    memset(aligned_buf, 0xFF, sizeof(aligned_buf));
    memcpy(aligned_buf, calib, sizeof(CalibData));

    uint32_t addr = CALIB_FLASH_ADDR;
    for (uint32_t b = 0; b < blocks; b++) {
        /* HAL_FLASH_Program 的 Address 参数为目标Flash地址，
         * DataAddress 参数为源数据地址（256位块首地址） */
        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                                   addr,
                                   (uint32_t)(aligned_buf + b * CALIB_FLASH_WORD_SIZE));
        if (status != HAL_OK) {
            HAL_FLASH_Lock();
            return -3;
        }
        addr += CALIB_FLASH_WORD_SIZE;
    }

    HAL_FLASH_Lock();
    return 0;
}

/* ==================== 加载默认偏移量 ==================== */
static void Calib_LoadDefaults(CalibData *calib)
{
    calib->magic = CALIB_MAGIC;

    const float defaults[CALIB_MOTOR_CNT] = {
        CALIB_DEFAULT_OFFSET_0,  CALIB_DEFAULT_OFFSET_1,
        CALIB_DEFAULT_OFFSET_2,  CALIB_DEFAULT_OFFSET_3,
        CALIB_DEFAULT_OFFSET_4,  CALIB_DEFAULT_OFFSET_5,
        CALIB_DEFAULT_OFFSET_6,  CALIB_DEFAULT_OFFSET_7,
        CALIB_DEFAULT_OFFSET_8,  CALIB_DEFAULT_OFFSET_9,
        CALIB_DEFAULT_OFFSET_10, CALIB_DEFAULT_OFFSET_11
    };
    memcpy(calib->position_offset, defaults, sizeof(defaults));
    memset(calib->last_boot_pos, 0, sizeof(calib->last_boot_pos));

    calib->crc32 = Calib_CRC32((const uint8_t *)calib,
                                sizeof(CalibData) - sizeof(uint32_t));
}

/* ==================== 完成上电采样，执行多圈补偿 ==================== */
static void Calib_FinalizeBoot(void)
{
    float current_boot_pos[CALIB_MOTOR_CNT];

    /* 计算各电机上电平均位置 */
    for (int i = 0; i < CALIB_MOTOR_CNT; i++) {
        if (g_boot_pos_cnt[i] > 0) {
            current_boot_pos[i] = g_boot_pos_sum[i] / g_boot_pos_cnt[i];
        } else {
            /* 该电机无反馈，保留上次记录值，不做补偿 */
            current_boot_pos[i] = g_calib.last_boot_pos[i];
        }
    }

    /* 多圈补偿：若当前上电位置与上次保存的上电位置相差 > π，
     * 说明电机在上次校准后转过了整圈，需要对偏移量补偿 ±2π */
    for (int i = 0; i < CALIB_MOTOR_CNT; i++) {
        if (!g_boot_pos_cnt[i]) continue;

        float diff = current_boot_pos[i] - g_calib.last_boot_pos[i];

        if (diff > CALIB_MULTI_TURN_THRESH) {
            /* 当前位置比上次大超过π：电机正向转了整圈，偏移量需减去2π */
            g_calib.position_offset[i] -= 2.0f * 3.14159265f;
        } else if (diff < -CALIB_MULTI_TURN_THRESH) {
            /* 当前位置比上次小超过π：电机反向转了整圈，偏移量需加上2π */
            g_calib.position_offset[i] += 2.0f * 3.14159265f;
        }
    }

    /* 更新Flash中的上电位置记录 */
    memcpy(g_calib.last_boot_pos, current_boot_pos, sizeof(current_boot_pos));
    g_calib.crc32 = Calib_CRC32((const uint8_t *)&g_calib,
                                  sizeof(CalibData) - sizeof(uint32_t));

    if (Calib_FlashWrite(&g_calib) != 0) {
        g_calib_state = CALIB_STATE_ERROR;
        return;
    }

    g_calib_state = CALIB_STATE_READY;
}

/* ==================== 完成校准模式采样，计算并保存偏移量 ==================== */
static void Calib_FinalizeCalib(void)
{
    /* 计算各电机标定位置平均值，作为新的偏移量（仿真零点为0，偏移量 = 标定位置 - 0） */
    for (int i = 0; i < CALIB_MOTOR_CNT; i++) {
        if (g_boot_pos_cnt[i] > 0) {
            g_calib.position_offset[i] = g_boot_pos_sum[i] / g_boot_pos_cnt[i];
        }
        /* 若该电机无反馈，保留原偏移量不变 */
    }

    /* 将当前标定位置同时记录为上电位置基准，避免下次正常上电触发多圈补偿 */
    for (int i = 0; i < CALIB_MOTOR_CNT; i++) {
        if (g_boot_pos_cnt[i] > 0) {
            g_calib.last_boot_pos[i] = g_boot_pos_sum[i] / g_boot_pos_cnt[i];
        }
    }

    g_calib.crc32 = Calib_CRC32((const uint8_t *)&g_calib,
                                  sizeof(CalibData) - sizeof(uint32_t));

    if (Calib_FlashWrite(&g_calib) != 0) {
        g_calib_state = CALIB_STATE_ERROR;
        return;
    }

    g_calib_state = CALIB_STATE_CALIB_DONE;
}

/* ==================== 公开接口实现 ==================== */

void Motor_Calib_Init(void)
{
    memset(g_boot_pos_sum, 0, sizeof(g_boot_pos_sum));
    memset(g_boot_pos_cnt, 0, sizeof(g_boot_pos_cnt));
    g_boot_sample_cnt = 0;

    /* 尝试从Flash加载校准数据 */
    if (Calib_FlashRead(&g_calib) != 0) {
        /* Flash数据无效，写入默认值 */
        Calib_LoadDefaults(&g_calib);
        Calib_FlashWrite(&g_calib);
    }

    /* 检测BIAO_Pin：低电平 = 进入校准模式，高电平 = 正常上电 */
    if (HAL_GPIO_ReadPin(BIAO_GPIO_Port, BIAO_Pin) == GPIO_PIN_RESET) {
        g_calib_state = CALIB_STATE_CALIBRATING;
    } else {
        g_calib_state = CALIB_STATE_COLLECTING;
    }
}

void Motor_Calib_FeedSample(const MotorFeedback feedback[CALIB_MOTOR_CNT])
{
    /* 仅在采集状态下处理 */
    if (g_calib_state != CALIB_STATE_COLLECTING &&
        g_calib_state != CALIB_STATE_CALIBRATING) {
        return;
    }

    /* 累加各电机位置 */
    for (int i = 0; i < CALIB_MOTOR_CNT; i++) {
        if (feedback[i].timestamp != 0) {
            g_boot_pos_sum[i] += feedback[i].position;
            g_boot_pos_cnt[i]++;
        }
    }

    g_boot_sample_cnt++;

    if (g_boot_sample_cnt >= CALIB_BOOT_SAMPLE_CNT) {
        if (g_calib_state == CALIB_STATE_CALIBRATING) {
            /* 校准模式：将当前位置作为偏移量写入Flash */
            Calib_FinalizeCalib();
        } else {
            /* 正常上电：执行多圈补偿并更新上电位置记录 */
            Calib_FinalizeBoot();
        }
    }
}

void Motor_Calib_ApplyTransform(uint8_t motor_idx, MotorControlParam *param)
{
    if (motor_idx >= CALIB_MOTOR_CNT || param == NULL) return;
    if (g_calib_state != CALIB_STATE_READY &&
        g_calib_state != CALIB_STATE_CALIB_DONE) {
        return;
    }

    float dir   = (float)g_motor_dir[motor_idx];   /* +1 或 -1 */
    float ratio = g_motor_ratio[motor_idx];
    float ratio2 = ratio * ratio;                   /* 减速比平方，dir^2=1 约去 */

    /* 电机位置 = (关节位置 * 方向 + 偏移) * 减速比 */
    float pos = (param->position * dir + g_calib.position_offset[motor_idx]) * ratio;
    if (pos < POS_MIN) pos = POS_MIN;
    if (pos > POS_MAX) pos = POS_MAX;
    param->position = pos;

    /* 电机速度 = 关节速度 * 方向 * 减速比 */
    float vel = param->velocity * dir * ratio;
    if (vel < VEL_MIN) vel = VEL_MIN;
    if (vel > VEL_MAX) vel = VEL_MAX;
    param->velocity = vel;

    /* 电机力矩 = 关节力矩 / 方向 / 减速比 */
    float torque = param->torque / dir / ratio;
    if (torque < TORQUE_MIN) torque = TORQUE_MIN;
    if (torque > TORQUE_MAX) torque = TORQUE_MAX;
    param->torque = torque;

    /* 电机kp = 关节kp / 减速比^2 */
    float kp = param->kp / ratio2;
    if (kp < KP_MIN) kp = KP_MIN;
    if (kp > KP_MAX) kp = KP_MAX;
    param->kp = kp;

    /* 电机kd = 关节kd / 减速比^2 */
    float kd = param->kd / ratio2;
    if (kd < KD_MIN) kd = KD_MIN;
    if (kd > KD_MAX) kd = KD_MAX;
    param->kd = kd;
}

int Motor_Calib_SaveOffsets(const float offsets[CALIB_MOTOR_CNT])
{
    memcpy(g_calib.position_offset, offsets,
           CALIB_MOTOR_CNT * sizeof(float));
    g_calib.crc32 = Calib_CRC32((const uint8_t *)&g_calib,
                                  sizeof(CalibData) - sizeof(uint32_t));
    return Calib_FlashWrite(&g_calib);
}

CalibState Motor_Calib_GetState(void)
{
    return g_calib_state;
}

uint8_t Motor_Calib_IsCalibMode(void)
{
    return (g_calib_state == CALIB_STATE_CALIBRATING ||
            g_calib_state == CALIB_STATE_CALIB_DONE) ? 1 : 0;
}

void Motor_Calib_GetOffsets(float offsets[CALIB_MOTOR_CNT])
{
    memcpy(offsets, g_calib.position_offset, CALIB_MOTOR_CNT * sizeof(float));
}
