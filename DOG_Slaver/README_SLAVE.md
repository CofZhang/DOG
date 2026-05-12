# STM32G474RET6 从控制器工程说明

## 系统概述

本工程为STM32G474RET6从控制器固件，配合STM32H743VIT6主控制器，实现USB HS转四路FDCAN电机控制网关系统。

### 系统架构

```
Jetson上位机
    ↓ USB HS CDC
主控STM32H743VIT6
    ├─ FDCAN1 → 电机1, 2, 3
    └─ SPI1 → 从控STM32G474RET6
              ├─ FDCAN1 → 电机4, 5, 6
              ├─ FDCAN2 → 电机7, 8, 9
              └─ FDCAN3 → 电机10, 11, 12
```

## 硬件配置

### 引脚定义

#### FDCAN接口
- **FDCAN1**: PA12(TX), PA11(RX) → 控制电机4, 5, 6
- **FDCAN2**: PB13(TX), PB12(RX) → 控制电机7, 8, 9
- **FDCAN3**: PA15(TX), PA8(RX) → 控制电机10, 11, 12

#### SPI接口（从机模式）
- **SPI1**: PA4(NSS), PA5(SCK), PA6(MISO), PA7(MOSI)
- **DMA配置**:
  - RX: DMA1_Channel1
  - TX: DMA1_Channel2

### FDCAN波特率配置

**当前配置**: 1Mbps（仲裁域和数据域）

**修改方法**（在`fdcan.c`中）:

```c
// 位于 MX_FDCAN1_Init(), MX_FDCAN2_Init(), MX_FDCAN3_Init()
hfdcan1.Init.NominalPrescaler = 16;     // 修改此值调整波特率
hfdcan1.Init.NominalSyncJumpWidth = 1;
hfdcan1.Init.NominalTimeSeg1 = 1;
hfdcan1.Init.NominalTimeSeg2 = 1;
```

**波特率计算公式**:
```
波特率 = FDCAN时钟 / (Prescaler × (1 + TimeSeg1 + TimeSeg2))
```

对于STM32G474，FDCAN时钟通常为170MHz，当前配置：
```
1Mbps = 170MHz / (16 × (1 + 1 + 1)) = 170MHz / 48 ≈ 3.54Mbps
```

**注意**: 需要根据实际FDCAN时钟重新计算。建议使用STM32CubeMX重新配置。

## 电机ID配置

电机ID在`motor_protocol.h`中定义，方便统一修改：

```c
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
```

## SPI通信协议

### SPI数据包格式（主控→从控，88字节）

| 字节位置 | 字段名称 | 描述 |
|---------|---------|------|
| 0 | headerH | 帧头高字节 0xAA |
| 1 | headerL | 帧头低字节 0x55 |
| 2 | cmdType | 命令类型 |
| 3 | sequence | 序列号 |
| 4-75 | motorData[9][8] | 9个电机控制数据（每个8字节） |
| 76-83 | reserved | 保留字节 |
| 84 | checksumH | 校验和高字节 |
| 85 | checksumL | 校验和低字节 |
| 86 | footerH | 帧尾高字节 0x55 |
| 87 | footerL | 帧尾低字节 0xAA |

### SPI反馈数据包格式（从控→主控，88字节）

格式与控制包相同，但`motorData`字段为电机反馈数据。

## 电机控制协议

### 控制指令格式（8字节，力位混控模式）

| 位域 | 位数 | 描述 | 物理范围 |
|-----|------|------|---------|
| Bit63-61 | 3bit | 电机模式（固定0x00） | - |
| Bit60-49 | 12bit | KP参数 | 0~5 |
| Bit48-40 | 9bit | KD参数 | 0~50 |
| Bit39-24 | 16bit | 期望位置 | -60~+60 rad |
| Bit23-12 | 12bit | 期望速度 | -60~+60 rad/s |
| Bit11-0 | 12bit | 前馈扭矩 | -60~+60 Nm |

### 反馈数据格式（8字节）

| 位域 | 位数 | 描述 |
|-----|------|------|
| Bit63-61 | 3bit | 报文类型（固定0x01） |
| Bit60-56 | 5bit | 错误码 |
| Bit55-40 | 16bit | 实际位置 |
| Bit39-28 | 12bit | 实际速度 |
| Bit27-16 | 12bit | 实际电流 |
| Bit15-8 | 8bit | 线圈温度 |
| Bit7-0 | 8bit | MOS温度 |

### 错误码定义

| 错误码 | 描述 |
|-------|------|
| 0 | 正常 |
| 1 | 过热 |
| 2 | 过流 |
| 3 | 电压过高 |
| 4 | 电压过低 |
| 5 | 编码器错误 |
| 6 | 刹车电压异常 |
| 7 | DRV驱动错误 |

## 代码结构

### 核心文件

1. **motor_protocol.h/c**: 电机协议定义和数据转换函数（主从共用）
2. **slave_controller.h/c**: 从控制器主逻辑
3. **main.c**: 主程序入口

### 主要函数

#### 初始化
```c
void SlaveController_Init(void);
```
- 配置FDCAN过滤器
- 启动FDCAN1/2/3
- 启动SPI DMA接收

#### 主循环处理
```c
void SlaveController_Process(void);
```
- 处理SPI接收的数据包
- 发送电机控制指令到FDCAN
- 准备并发送SPI反馈数据包
- 超时保护（1秒无数据则停止电机）

#### 回调函数
```c
void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs);
```

## 工作流程

### 数据流向

1. **控制流程**:
   ```
   主控SPI发送 → 从控SPI接收中断 → 解析数据包 → 发送到FDCAN1/2/3 → 电机执行
   ```

2. **反馈流程**:
   ```
   电机反馈 → FDCAN接收中断 → 保存反馈数据 → 打包SPI反馈包 → SPI发送到主控
   ```

### 时序说明

1. 从控启动后，SPI进入DMA接收等待状态
2. 主控通过SPI发送88字节控制数据包
3. 从控SPI接收完成中断触发，设置`spiRxComplete`标志
4. 主循环检测到标志，解析数据包并发送到FDCAN
5. 从控准备反馈数据包，通过SPI DMA发送
6. SPI发送完成后，重新启动SPI接收

## 安全机制

### 超时保护

- 如果超过1秒未收到主控数据，自动发送零扭矩指令停止所有电机
- 防止通信中断导致电机失控

### 数据校验

- SPI数据包包含16位XOR校验和
- 帧头帧尾验证，确保数据完整性

## 编译和烧录

### 开发环境

- STM32CubeIDE 或 Keil MDK
- STM32CubeMX（用于外设配置）

### 编译步骤

1. 打开工程文件（.ioc或.uvprojx）
2. 编译工程
3. 通过ST-Link或J-Link烧录到STM32G474RET6

### 调试建议

1. 使用逻辑分析仪监控SPI通信
2. 使用CAN分析仪监控FDCAN总线
3. 通过串口输出调试信息（需添加UART支持）

## 常见问题

### Q1: FDCAN无法通信？
**A**: 检查波特率配置是否与电机匹配，确认CAN收发器硬件连接正常。

### Q2: SPI通信异常？
**A**: 确认主从时钟极性和相位配置一致，检查NSS信号是否正常。

### Q3: 电机不响应？
**A**: 检查电机ID配置是否正确，确认FDCAN过滤器配置。

### Q4: 如何修改电机ID？
**A**: 修改`motor_protocol.h`中的`MOTOR_ID_x`宏定义，重新编译烧录。

## 版本历史

- **v1.0** (2026-03-31): 初始版本
  - 实现SPI从机通信
  - 实现三路FDCAN电机控制
  - 实现电机反馈数据采集

## 技术支持

如有问题，请参考：
- STM32G474参考手册
- FDCAN应用笔记AN5348
- 电机控制器用户手册

---

**注意**: 本固件需配合主控制器STM32H743VIT6使用，单独运行无法工作。
