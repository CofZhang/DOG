/**
 * @file motor_calib.h
 * @brief 电机校准模块 - 管理仿真零点与实际标定位置之间的偏移量
 *
 * 工作原理：
 *   - Flash中存储12个电机的位置偏移量（仿真坐标系 → 实际坐标系）
 *   - 上电后收集10帧反馈取平均，记录初始位置
 *   - 若当前上电位置与上次保存的上电位置相差 > π，说明电机转过了整圈，
 *     自动补偿 ±2π 以消除多圈误差
 *   - 控制指令发出前，对目标位置叠加偏移量
 *
 * Flash布局（Bank B Sector 7，地址 0x081E0000，128KB）：
 *   Offset 0x00: uint32_t magic (0xCAFEBEEF)
 *   Offset 0x04: float    position_offset[12]  (48字节，仿真→实际偏移)
 *   Offset 0x34: float    last_boot_pos[12]     (48字节，上次上电位置)
 *   Offset 0x64: uint32_t crc32                 (4字节，对前100字节的CRC)
 */

#ifndef __MOTOR_CALIB_H
#define __MOTOR_CALIB_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include "motor.h"

/* ==================== Flash存储配置 ==================== */
#define CALIB_FLASH_ADDR        0x081E0000UL   /* Bank B Sector 7 起始地址 */
#define CALIB_FLASH_BANK        FLASH_BANK_2
#define CALIB_FLASH_SECTOR      FLASH_SECTOR_7
#define CALIB_MAGIC             0xCAFEBEEFUL

/* ==================== 校准参数配置 ==================== */
#define CALIB_BOOT_SAMPLE_CNT   10             /* 上电采样帧数 */
#define CALIB_MULTI_TURN_THRESH 3.14159265f    /* 多圈检测阈值（π rad） */
#define CALIB_MOTOR_CNT         12

/* 默认偏移量（仿真零点 → 实际标定位置，单位 rad，待用户填写实测值） */
#define CALIB_DEFAULT_OFFSET_0   0.0f
#define CALIB_DEFAULT_OFFSET_1   0.0f
#define CALIB_DEFAULT_OFFSET_2   0.0f
#define CALIB_DEFAULT_OFFSET_3   0.0f
#define CALIB_DEFAULT_OFFSET_4   0.0f
#define CALIB_DEFAULT_OFFSET_5   0.0f
#define CALIB_DEFAULT_OFFSET_6   0.0f
#define CALIB_DEFAULT_OFFSET_7   0.0f
#define CALIB_DEFAULT_OFFSET_8   0.0f
#define CALIB_DEFAULT_OFFSET_9   0.0f
#define CALIB_DEFAULT_OFFSET_10  0.0f
#define CALIB_DEFAULT_OFFSET_11  0.0f

/* ==================== 数据结构 ==================== */
typedef struct {
    uint32_t magic;                          /* 魔数，用于校验Flash数据有效性 */
    float    position_offset[CALIB_MOTOR_CNT]; /* 位置偏移量（rad），仿真→实际 */
    float    last_boot_pos[CALIB_MOTOR_CNT];   /* 上次上电时记录的初始位置（rad） */
    uint32_t crc32;                          /* 对前100字节的CRC32校验 */
} CalibData;

/* 校准状态 */
typedef enum {
    CALIB_STATE_COLLECTING = 0,  /* 正常上电：正在收集初始位置样本（多圈补偿用） */
    CALIB_STATE_CALIBRATING,     /* 校准模式：BIAO_Pin拉低，正在采集标定位置 */
    CALIB_STATE_READY,           /* 校准就绪，偏移量已加载可正常工作 */
    CALIB_STATE_CALIB_DONE,      /* 校准模式完成，偏移量已写入Flash，等待拨回开关 */
    CALIB_STATE_ERROR            /* Flash读写错误 */
} CalibState;

/* ==================== 函数声明 ==================== */

/**
 * @brief 初始化校准模块
 * @note 从Flash加载偏移量；若Flash数据无效则写入默认值
 *       调用后进入CALIB_STATE_COLLECTING状态，等待上电位置采样
 */
void Motor_Calib_Init(void);

/**
 * @brief 上电位置采样（在收到电机反馈时调用）
 * @param feedback 12个电机的反馈数据数组
 * @note 累积CALIB_BOOT_SAMPLE_CNT帧后自动完成采样，进入CALIB_STATE_READY
 *       同时执行多圈补偿并将新的上电位置写回Flash
 */
void Motor_Calib_FeedSample(const MotorFeedback feedback[CALIB_MOTOR_CNT]);

/**
 * @brief 将关节空间控制参数转换为电机空间控制参数
 * @param motor_idx 电机索引（0~11）
 * @param param     输入/输出参数，传入关节空间值，原地转换为电机空间值
 *
 * 转换公式：
 *   电机位置 = (关节位置 * 方向 + 偏移) * 减速比
 *   电机速度 = 关节速度 * 方向 * 减速比
 *   电机力矩 = 关节力矩 / 方向 / 减速比
 *   电机kp   = 关节kp   / (减速比^2)   （方向^2=1，约去）
 *   电机kd   = 关节kd   / (减速比^2)
 *
 * @note 仅在 CALIB_STATE_READY / CALIB_STATE_CALIB_DONE 状态下转换；
 *       否则原样返回（偏移和减速比均不生效）
 */
void Motor_Calib_ApplyTransform(uint8_t motor_idx, MotorControlParam *param);

/**
 * @brief 将新的偏移量写入Flash
 * @param offsets 12个电机的偏移量数组（rad）
 * @return 0=成功，非0=Flash写入失败
 */
int Motor_Calib_SaveOffsets(const float offsets[CALIB_MOTOR_CNT]);

/**
 * @brief 获取当前校准状态
 */
CalibState Motor_Calib_GetState(void);

/**
 * @brief 查询当前是否处于校准模式（BIAO_Pin拉低触发）
 * @return 1=校准模式中或已完成，0=正常模式
 */
uint8_t Motor_Calib_IsCalibMode(void);

/**
 * @brief 获取当前加载的偏移量（只读）
 * @param offsets 输出缓冲区，长度12
 */
void Motor_Calib_GetOffsets(float offsets[CALIB_MOTOR_CNT]);

#ifdef __cplusplus
}
#endif

#endif /* __MOTOR_CALIB_H */
