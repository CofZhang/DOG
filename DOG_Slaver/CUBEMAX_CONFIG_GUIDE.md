# CubeMX配置修改指南

## 目的
修正从控制器STM32G474RET6的CubeMX配置，使其与系统要求完全匹配。

## 修改步骤

### 1. 打开CubeMX工程

1. 找到工程文件：`DOG_G.ioc`
2. 双击打开（或在STM32CubeMX中File → Open Project）

### 2. 修改SPI1配置（推荐）

#### 当前配置问题
- Data Size: 16 Bits（导致代码复杂度增加）

#### 修改步骤
1. 在左侧树形菜单中找到：`Connectivity` → `SPI1`
2. 在右侧`Parameter Settings`中找到`Data Size`
3. 将`16 Bits`改为`8 Bits`
4. 保持其他配置不变：
   - Mode: `Slave`
   - Hardware NSS Signal: `Enable`
   - Frame Format: `Motorola`
   - Data Size: `8 Bits` ← 修改这里
   - First Bit: `MSB First`
   - Clock Polarity (CPOL): `Low`
   - Clock Phase (CPHA): `1 Edge`

#### 修改后的代码影响
如果改为8位模式，需要修改`slave_controller.c`：
```c
// 修改前（16位模式）
uint16_t spi_length = sizeof(SPIPacket_t) / 2;
HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)buffer, spi_length);

// 修改后（8位模式）
HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)buffer, sizeof(SPIPacket_t));
```

### 3. 修改FDCAN波特率配置（必须）

#### 当前配置问题
- 波特率约3.54Mbps，不符合1Mbps要求

#### 修改步骤（FDCAN1）

1. 在左侧树形菜单中找到：`Connectivity` → `FDCAN1`
2. 点击`Parameter Settings`标签
3. 找到`Bit Timing Parameters`部分
4. 修改以下参数：

**Nominal Bit Timing（仲裁域）**：
```
Prescaler: 17          （原值：16）
Time Quanta in Bit Segment 1: 7    （原值：1）
Time Quanta in Bit Segment 2: 2    （原值：1）
Synchronization Jump Width: 2      （原值：1）
```

**计算验证**：
```
FDCAN时钟 = 170 MHz（STM32G474典型值）
波特率 = 170MHz / (Prescaler × (1 + Seg1 + Seg2))
      = 170MHz / (17 × (1 + 7 + 2))
      = 170MHz / 170
      = 1 Mbps ✓
```

5. 对FDCAN2和FDCAN3重复相同的配置

#### 详细配置界面

```
┌─────────────────────────────────────────────┐
│ FDCAN1 Configuration                        │
├─────────────────────────────────────────────┤
│ Mode and Configuration                      │
│   Mode: [Normal Mode]                       │
│   Frame Format: [Classic CAN]               │
│   Automatic Retransmission: [Disable]       │
│                                             │
│ Bit Timing Parameters                       │
│   Nominal (Arbitration Phase):             │
│     Prescaler: [17]          ← 修改         │
│     Time Seg1: [7]           ← 修改         │
│     Time Seg2: [2]           ← 修改         │
│     Sync Jump Width: [2]     ← 修改         │
│                                             │
│   Data Phase:                               │
│     Prescaler: [1]                          │
│     Time Seg1: [1]                          │
│     Time Seg2: [1]                          │
│     Sync Jump Width: [1]                    │
└─────────────────────────────────────────────┘
```

### 4. 调整中断优先级（可选优化）

#### 修改步骤

1. 点击顶部菜单：`System Core` → `NVIC`
2. 在`NVIC`标签页中找到以下中断
3. 修改优先级：

| 中断名称 | 当前优先级 | 推荐优先级 | 说明 |
|---------|-----------|-----------|------|
| FDCAN1 interrupt 0 | 0 | 0 | 最高优先级（实时性） |
| FDCAN1 interrupt 1 | 0 | 0 | 最高优先级 |
| FDCAN2 interrupt 0 | 0 | 0 | 最高优先级 |
| FDCAN2 interrupt 1 | 0 | 0 | 最高优先级 |
| FDCAN3 interrupt 0 | 0 | 0 | 最高优先级 |
| FDCAN3 interrupt 1 | 0 | 0 | 最高优先级 |
| DMA1 channel1 | 0 | 1 | 中等优先级（SPI RX） |
| DMA1 channel2 | 0 | 1 | 中等优先级（SPI TX） |
| SPI1 global | 0 | 2 | 较低优先级 |

**注意**：优先级数字越小，优先级越高。

### 5. 验证时钟配置

#### 检查步骤

1. 点击顶部菜单：`Clock Configuration`
2. 验证以下时钟：

```
┌─────────────────────────────────────────────┐
│ Clock Tree                                  │
├─────────────────────────────────────────────┤
│ System Clock (SYSCLK): 170 MHz             │
│ AHB Clock (HCLK): 170 MHz                  │
│ APB1 Clock: 170 MHz                        │
│ APB2 Clock: 170 MHz                        │
│ FDCAN Clock: 170 MHz      ← 确认这个       │
└─────────────────────────────────────────────┘
```

如果FDCAN时钟不是170MHz，需要调整波特率计算。

### 6. 生成代码

#### 生成步骤

1. 点击顶部菜单：`Project` → `Generate Code`
2. 或者按快捷键：`Alt + K`
3. 等待代码生成完成

#### 重要提示

⚠️ **代码生成前备份**：
```bash
# 备份当前代码
cp Core/Src/slave_controller.c Core/Src/slave_controller_backup.c
cp Core/Inc/slave_controller.h Core/Inc/slave_controller_backup.c
```

⚠️ **USER CODE区域保护**：
CubeMX只会重新生成外设初始化代码，不会覆盖`/* USER CODE BEGIN */`和`/* USER CODE END */`之间的代码。

### 7. 代码修改（如果改为8位SPI）

如果将SPI改为8位模式，需要修改`slave_controller.c`：

#### 修改位置1：初始化函数
```c
void SlaveController_Init(void) {
    // ...

    // ==================== SPI初始化 ====================
    // 修改前（16位模式）
    // uint16_t spi_length = sizeof(SPIPacket_t) / 2;
    // HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, spi_length);

    // 修改后（8位模式）
    HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, sizeof(SPIPacket_t));

    // ...
}
```

#### 修改位置2：主循环处理
```c
void SlaveController_Process(void) {
    // 检查SPI接收完成
    if (g_slaveCtrl.spiRxComplete) {
        // ...

        // 修改前（16位模式）
        // uint16_t spi_length = sizeof(SPIFeedbackPacket_t) / 2;
        // HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiFeedbackPacket, spi_length);

        // 修改后（8位模式）
        HAL_SPI_Transmit_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiFeedbackPacket, sizeof(SPIFeedbackPacket_t));
    }

    // 检查SPI发送完成
    if (g_slaveCtrl.spiTxComplete) {
        // ...

        // 修改前（16位模式）
        // uint16_t spi_length = sizeof(SPIPacket_t) / 2;
        // HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, spi_length);

        // 修改后（8位模式）
        HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)&g_slaveCtrl.spiRxPacket, sizeof(SPIPacket_t));
    }

    // ...
}
```

### 8. 编译和测试

#### 编译步骤

1. 打开STM32CubeIDE或Keil MDK
2. 清理工程：`Project` → `Clean`
3. 编译工程：`Project` → `Build All`
4. 检查编译输出，确保无错误

#### 测试步骤

1. **单独测试FDCAN**：
   - 使用CAN分析仪连接到FDCAN1
   - 发送测试帧，检查波特率是否为1Mbps
   - 验证电机响应

2. **测试SPI通信**：
   - 连接主控和从控的SPI接口
   - 使用逻辑分析仪监控SPI信号
   - 验证数据传输正确

3. **完整系统测试**：
   - 连接所有硬件
   - 运行上位机软件
   - 测试12个电机控制

## 配置对比表

| 配置项 | 修改前 | 修改后 | 影响 |
|-------|-------|-------|------|
| SPI Data Size | 16 Bits | 8 Bits | 简化代码，提高可维护性 |
| FDCAN Prescaler | 16 | 17 | 波特率从3.54Mbps改为1Mbps |
| FDCAN TimeSeg1 | 1 | 7 | 增加采样点位置精度 |
| FDCAN TimeSeg2 | 1 | 2 | 改善相位误差容限 |
| FDCAN SJW | 1 | 2 | 提高同步能力 |
| DMA Priority | 0 | 1 | 优化中断响应 |

## 常见问题

### Q1: 修改后编译报错？
**A**: 检查是否正确修改了slave_controller.c中的SPI传输长度。

### Q2: FDCAN通信失败？
**A**:
1. 使用CAN分析仪验证波特率
2. 检查CAN收发器硬件连接
3. 确认终端电阻（120Ω）

### Q3: SPI通信异常？
**A**:
1. 确认主从SPI配置一致（8位或16位）
2. 检查时钟极性和相位
3. 验证NSS信号

### Q4: 如何验证波特率配置正确？
**A**:
```c
// 在main.c中添加调试代码
uint32_t fdcan_clock = HAL_RCC_GetPCLK1Freq();
uint32_t prescaler = hfdcan1.Init.NominalPrescaler;
uint32_t time_quanta = 1 + hfdcan1.Init.NominalTimeSeg1 + hfdcan1.Init.NominalTimeSeg2;
uint32_t baudrate = fdcan_clock / (prescaler * time_quanta);
printf("FDCAN Baudrate: %lu bps\n", baudrate);
```

## 修改检查清单

完成以下检查后，在方框中打勾：

- [ ] 备份了原始代码
- [ ] 修改了SPI Data Size为8 Bits
- [ ] 修改了FDCAN1波特率参数
- [ ] 修改了FDCAN2波特率参数
- [ ] 修改了FDCAN3波特率参数
- [ ] 调整了中断优先级
- [ ] 生成了新代码
- [ ] 修改了slave_controller.c（如果改为8位SPI）
- [ ] 编译通过无错误
- [ ] 测试了FDCAN通信
- [ ] 测试了SPI通信
- [ ] 完整系统测试通过

## 技术支持

如果遇到问题，请参考：
- `CONFIG_CHECK_REPORT.md` - 详细的配置检查报告
- `SYSTEM_DOCUMENTATION.md` - 完整系统文档
- STM32G474参考手册 RM0440
- FDCAN应用笔记 AN5348

---

**最后更新**: 2026-03-31
**版本**: v1.0
