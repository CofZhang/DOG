# 通信底层配置全面检查报告
**生成时间**: 2026-04-02
**涉及工程**: MT_H7 (STM32H723) / DOG (STM32H743) / DOG_G (STM32G474)

---

## 一、系统架构概览

```
MT_H7 (STM32H723)          DOG (STM32H743)           DOG_G (STM32G474)
  FDCAN1 @ 1MHz    <--->   FDCAN1 @ 506kHz    SPI1 Master -----> SPI1 Slave
  PD0(RX)/PD1(TX)          PD0(RX)/PD1(TX)    24MHz 16bit        16bit
                                               PA4/PB3/PA6/PA7    PA4/PA5/PA6/PA7
                                                                   FDCAN1/2/3 --> 电机4-12
```

---

## 二、FDCAN 配置详细分析

### 2.1 DOG_G (STM32G474) — FDCAN1/2/3

| 参数 | 配置值 | 问题 |
|------|--------|------|
| FDCAN 时钟源 | PLL → 170 MHz | ✓ |
| NominalPrescaler | 16 | ✓ |
| NominalTimeSeg1 | **1** | ❌ 低于最小值 2 |
| NominalTimeSeg2 | **1** | ❌ 低于最小值 2 |
| NominalSyncJumpWidth | 1 | ⚠️ 建议 ≥ 2 |
| 计算波特率 | **~3.54 MHz** | ❌ 超出 Classic CAN 上限 1 Mbps |
| RxFifo0ElmtsNbr | **0** (未配置) | ❌ 无接收缓冲，无法收帧 |
| FDCAN1 StdFiltersNbr | 1 | ✓ |
| FDCAN2 StdFiltersNbr | **0** | ❌ 所有帧被过滤器拒绝 |
| FDCAN3 StdFiltersNbr | **0** | ❌ 所有帧被过滤器拒绝 |
| FDCAN3 NVIC 中断 | **未配置** | ❌ FDCAN3 无法触发中断 |

**波特率计算验证:**
```
Baud = FDCAN_CLK / (Prescaler × (1 + TimeSeg1 + TimeSeg2))
     = 170,000,000 / (16 × (1 + 1 + 1))
     = 170,000,000 / 48
     = 3,541,666 Hz  ← 严重错误，Classic CAN 最大 1 Mbps
```

**HAL 最小值约束 (stm32g4xx_hal_fdcan.h):**
```c
IS_FDCAN_NOMINAL_TSEG1(TSEG1)  → TSEG1 >= 2
IS_FDCAN_NOMINAL_TSEG2(TSEG2)  → TSEG2 >= 2
```
当前 TimeSeg1=1, TimeSeg2=1 → `HAL_FDCAN_Init()` 将返回 `HAL_ERROR`，FDCAN 无法启动。

**修复建议 (目标 1 Mbps):**
```
FDCAN_CLK = 170 MHz
Prescaler=10, TimeSeg1=12, TimeSeg2=4, SJW=4
Baud = 170,000,000 / (10 × (1+12+4)) = 170,000,000 / 170 = 1,000,000 Hz ✓
采样点 = (1+12)/17 = 76.5%  (推荐 75%~80%)
```

---

### 2.2 DOG (STM32H743) — FDCAN1

| 参数 | 配置值 | 说明 |
|------|--------|------|
| FDCAN 时钟源 | PLL2 → 76 MHz | HSE=8M, PLL2M=1, PLL2N=19, PLL2R=2 |
| NominalPrescaler | 10 | ✓ |
| NominalTimeSeg1 | 10 | ✓ |
| NominalTimeSeg2 | 4 | ✓ |
| NominalSyncJumpWidth | 4 | ✓ |
| 计算波特率 | **~506.7 kHz** | ⚠️ 非标准，与电机 1 Mbps 不匹配 |
| RxFifo0ElmtsNbr | 16 | ✓ |
| TxFifoQueueElmtsNbr | 16 | ✓ |
| 引脚 | PD0(RX), PD1(TX) | ✓ |

**波特率计算验证:**
```
Baud = 76,000,000 / (10 × (1+10+4)) = 76,000,000 / 150 = 506,666 Hz
```
⚠️ 若 DOG 的 FDCAN 用于连接电机（1 Mbps），需修改时序参数。
若 DOG 的 FDCAN 用于连接 MT_H7，则需与 MT_H7 的 1 MHz 对齐。

---

### 2.3 MT_H7 (STM32H723) — FDCAN1

| 参数 | 配置值 | 说明 |
|------|--------|------|
| FDCAN 时钟源 | PLL → 120 MHz | ✓ |
| NominalPrescaler | 24 | ✓ |
| NominalTimeSeg1 | 2 | ✓ |
| NominalTimeSeg2 | 2 | ✓ |
| 计算波特率 | **1 MHz** | ✓ 标准 CAN 速率 |
| RxFifo0ElmtsNbr | 32 | ✓ |
| TxFifoQueueElmtsNbr | 32 | ✓ |
| 引脚 | PD0(RX), PD1(TX) | ✓ |

**波特率计算验证:**
```
Baud = 120,000,000 / (24 × (1+2+2)) = 120,000,000 / 120 = 1,000,000 Hz ✓
```

---

### 2.4 FDCAN 波特率汇总对比

| 控制器 | 波特率 | 状态 |
|--------|--------|------|
| MT_H7 FDCAN1 | 1,000,000 Hz | ✓ 标准 |
| DOG FDCAN1 | 506,666 Hz | ⚠️ 与 MT_H7 不一致 |
| DOG_G FDCAN1/2/3 | ~3,541,666 Hz | ❌ 超限且参数非法 |

---

## 三、SPI 配置详细分析

### 3.1 DOG (Master, STM32H743) ↔ DOG_G (Slave, STM32G474)

| 参数 | DOG (Master) | DOG_G (Slave) | 兼容性 |
|------|-------------|---------------|--------|
| 模式 | Master | Slave | ✓ |
| 数据位宽 | 16-bit | 16-bit | ✓ |
| CPOL | LOW (0) | LOW (0) | ✓ |
| CPHA | 1EDGE (0) | 1EDGE (0) | ✓ SPI Mode 0 |
| NSS | Hard Output | Hard Input | ✓ |
| 首位 | MSB | MSB | ✓ |
| 时钟频率 | 192MHz/8 = **24 MHz** | 从机跟随 | ✓ G474 SPI 最大 85 MHz |
| DMA 对齐 | **BYTE** | **BYTE** | ⚠️ 与 16-bit 不匹配 |
| NSS Pulse | ENABLE | DISABLE | ✓ 主机特性 |

**SPI 引脚对应 (跨板物理连接):**
| 信号 | DOG 引脚 | DOG_G 引脚 |
|------|---------|-----------|
| NSS | PA4 | PA4 |
| SCK | **PB3** | **PA5** |
| MISO | PA6 | PA6 |
| MOSI | PA7 | PA7 |

SCK 引脚名称不同（PB3 vs PA5），这是不同芯片上的引脚，PCB 布线连接即可，逻辑上无问题。

---

### 3.2 SPI DMA 对齐问题 (DOG 和 DOG_G 均存在)

**问题:** SPI 配置为 16-bit 数据宽度，但 DMA 配置为 BYTE 对齐：
```c
// DOG spi.c & DOG_G spi.c 均如此配置
hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // ⚠️
hdma_spi1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;  // ⚠️
```

**影响:** 16-bit SPI 使用 BYTE DMA 时，每次 DMA 传输只搬运 1 字节，但 SPI 期望 2 字节一帧，会导致数据错位或传输异常。

**修复:** 两端 DMA 均应改为 HALFWORD：
```c
hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
hdma_spi1_tx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
hdma_spi1_rx.Init.MemDataAlignment    = DMA_MDATAALIGN_HALFWORD;
```
需在 CubeMX 中修改后重新生成，或直接修改 spi.c 中的 MspInit。

---

## 四、问题汇总与优先级

### ❌ 严重问题（必须修复，否则无法运行）

| # | 问题 | 位置 | 修复方案 |
|---|------|------|---------|
| 1 | DOG_G FDCAN TimeSeg1/2=1 低于最小值，HAL_Init 失败 | DOG_G fdcan.c | CubeMX 重新配置时序参数 |
| 2 | DOG_G FDCAN 波特率 3.54 MHz 超出 Classic CAN 上限 | DOG_G fdcan.c | 目标 1 Mbps：Prescaler=10, Seg1=12, Seg2=4 |
| 3 | DOG_G 所有 FDCAN 无接收 FIFO (RxFifo0ElmtsNbr=0) | DOG_G .ioc | CubeMX 中为 FDCAN1/2/3 配置 RxFifo0ElmtsNbr≥8 |
| 4 | DOG_G FDCAN3 未配置 NVIC 中断 | DOG_G fdcan.c / .ioc | 在 .ioc 中启用 FDCAN3_IT0/IT1，重新生成 |

### ⚠️ 警告问题（影响稳定性）

| # | 问题 | 位置 | 修复方案 |
|---|------|------|---------|
| 5 | SPI DMA BYTE 对齐与 16-bit SPI 不匹配 | DOG & DOG_G spi.c | DMA 对齐改为 HALFWORD |
| 6 | DOG FDCAN1 波特率 506 kHz 与 MT_H7 的 1 MHz 不一致 | DOG fdcan.c | 确认 DOG FDCAN 连接对象，统一波特率 |
| 7 | DOG_G FDCAN2/3 StdFiltersNbr=0，所有帧被拒绝 | DOG_G .ioc | 为 FDCAN2/3 配置至少 1 个标准过滤器 |

### ✓ 已确认正确

| 项目 | 状态 |
|------|------|
| FDCAN3 引脚: PA15(TX), PA8(RX) | ✓ .ioc 与 fdcan.c 一致 |
| SPI Mode 0 (CPOL=0, CPHA=0) 主从一致 | ✓ |
| SPI 16-bit 数据宽度主从一致 | ✓ |
| SPI NSS Hard 主输出/从输入 | ✓ |
| MT_H7 FDCAN1 @ 1 MHz 参数合法 | ✓ |
| DOG SPI 时钟 24 MHz，G474 从机可承受 | ✓ |
| DOG_G SPI 从机模式配置 | ✓ |

---

## 五、DOG_G FDCAN 修复参数参考

目标波特率 **1 Mbps**，FDCAN 时钟 170 MHz：

```c
// fdcan.c — FDCAN1/2/3 均使用相同时序
hfdcan1.Init.NominalPrescaler    = 10;
hfdcan1.Init.NominalSyncJumpWidth = 4;
hfdcan1.Init.NominalTimeSeg1     = 12;   // 采样点 = (1+12)/17 = 76.5%
hfdcan1.Init.NominalTimeSeg2     = 4;
// 波特率 = 170,000,000 / (10 × 17) = 1,000,000 Hz ✓

// 接收 FIFO（必须配置）
hfdcan1.Init.RxFifo0ElmtsNbr    = 16;
hfdcan1.Init.RxFifo0ElmtSize    = FDCAN_DATA_BYTES_8;
hfdcan1.Init.StdFiltersNbr      = 1;    // FDCAN2/3 也需要设为 1

// 发送 FIFO
hfdcan1.Init.TxFifoQueueElmtsNbr = 16;
hfdcan1.Init.TxFifoQueueMode     = FDCAN_TX_FIFO_OPERATION;
hfdcan1.Init.TxElmtSize          = FDCAN_DATA_BYTES_8;
```

**建议在 CubeMX 中修改后重新生成代码，不要手动修改 USER CODE 区域外的参数。**
