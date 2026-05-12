# 从控制器工程配置检查和修复报告

## 检查日期
2026-03-31

## 检查的CubeMX配置

### 1. SPI1配置检查

#### 当前CubeMX配置（spi.c）
```c
hspi1.Instance = SPI1;
hspi1.Init.Mode = SPI_MODE_SLAVE;              // ✓ 从机模式正确
hspi1.Init.Direction = SPI_DIRECTION_2LINES;   // ✓ 全双工模式正确
hspi1.Init.DataSize = SPI_DATASIZE_16BIT;      // ⚠️ 16位模式
hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;     // ✓ CPOL=0
hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;         // ✓ CPHA=0
hspi1.Init.NSS = SPI_NSS_HARD_INPUT;           // ✓ 硬件NSS
hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;        // ✓ MSB先行
```

#### DMA配置
```c
// SPI1_RX: DMA1_Channel1
hdma_spi1_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // ⚠️ 字节对齐
hdma_spi1_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;     // ⚠️ 字节对齐

// SPI1_TX: DMA1_Channel2
hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;  // ⚠️ 字节对齐
hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;     // ⚠️ 字节对齐
```

#### 问题分析
**严重问题**：SPI配置为16位数据模式，但DMA配置为字节对齐。这会导致：
1. HAL_SPI_Receive_DMA/Transmit_DMA的长度参数单位为**半字（16位）**，不是字节
2. 如果按字节数传递长度，实际传输的数据量会是预期的2倍
3. 数据包必须是偶数字节（16位对齐）

#### 修复方案

**方案1：修改代码适配16位SPI（已实施）**
```c
// 原代码（错误）
HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)buffer, 88);  // 错误：会传输176字节

// 修复后代码
uint16_t spi_length = sizeof(SPIPacket_t) / 2;  // 88字节 = 44个半字
HAL_SPI_Receive_DMA(&hspi1, (uint8_t*)buffer, spi_length);  // 正确：传输88字节
```

**方案2：修改CubeMX配置为8位SPI（推荐）**
1. 打开CubeMX工程
2. 找到SPI1配置
3. 将`Data Size`从`16 Bits`改为`8 Bits`
4. 重新生成代码
5. 使用原始代码（按字节传输）

### 2. FDCAN配置检查

#### FDCAN1配置
```c
hfdcan1.Init.NominalPrescaler = 16;
hfdcan1.Init.NominalSyncJumpWidth = 1;
hfdcan1.Init.NominalTimeSeg1 = 1;
hfdcan1.Init.NominalTimeSeg2 = 1;
```

#### 波特率计算
假设FDCAN时钟为170MHz（STM32G474典型值）：
```
波特率 = 170MHz / (16 × (1 + 1 + 1))
       = 170MHz / 48
       = 3.54 Mbps
```

**问题**：文档要求1Mbps，但实际配置约为3.54Mbps

#### 修复方案
修改CubeMX配置以获得1Mbps波特率：
```
目标波特率 = 1 Mbps
FDCAN时钟 = 170 MHz

Prescaler × (1 + TimeSeg1 + TimeSeg2) = 170
```

推荐配置：
```c
hfdcan1.Init.NominalPrescaler = 17;     // 修改为17
hfdcan1.Init.NominalSyncJumpWidth = 2;
hfdcan1.Init.NominalTimeSeg1 = 7;       // 修改为7
hfdcan1.Init.NominalTimeSeg2 = 2;       // 修改为2

// 波特率 = 170MHz / (17 × (1 + 7 + 2)) = 170MHz / 170 = 1 Mbps
```

### 3. FDCAN引脚配置检查

#### FDCAN3引脚（fdcan.c）
```c
/**FDCAN3 GPIO Configuration
PA8     ------> FDCAN3_RX
PA15    ------> FDCAN3_TX
*/
```

**确认**：FDCAN3_TX为PA15，FDCAN3_RX为PA8，与CubeMX配置一致

#### 引脚配置
- FDCAN3_TX: **PA15**
- FDCAN3_RX: PA8

### 4. 中断优先级检查

#### 当前配置
```c
// DMA中断
HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);  // SPI RX
HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);  // SPI TX

// FDCAN中断
HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 0, 0);
HAL_NVIC_SetPriority(FDCAN2_IT0_IRQn, 0, 0);
HAL_NVIC_SetPriority(FDCAN3_IT0_IRQn, 0, 0);

// SPI中断
HAL_NVIC_SetPriority(SPI1_IRQn, 0, 0);
```

**问题**：所有中断优先级相同（0,0），可能导致中断冲突

#### 推荐优先级配置
```c
// 最高优先级：FDCAN（实时性要求高）
HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 0, 0);
HAL_NVIC_SetPriority(FDCAN2_IT0_IRQn, 0, 0);
HAL_NVIC_SetPriority(FDCAN3_IT0_IRQn, 0, 0);

// 中等优先级：SPI DMA（数据传输）
HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 1, 0);
HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 1, 0);

// 较低优先级：SPI中断
HAL_NVIC_SetPriority(SPI1_IRQn, 2, 0);
```

## 修复总结

### 已修复的问题

1. ✅ **SPI 16位模式适配**
   - 修改了slave_controller.c中的DMA传输长度计算
   - 所有SPI传输现在使用半字为单位

2. ✅ **数据包对齐**
   - 确认SPIPacket_t和SPIFeedbackPacket_t都是88字节（偶数，16位对齐）

3. ✅ **FDCAN过滤器配置**
   - 添加了ConfigureFDCANFilter函数
   - 配置接收所有标准帧（0x000-0x7FF）

4. ✅ **回调函数实现**
   - 实现了HAL_SPI_RxCpltCallback
   - 实现了HAL_SPI_TxCpltCallback
   - 实现了HAL_FDCAN_RxFifo0Callback

### 需要用户手动修复的问题

1. ⚠️ **SPI数据位宽**（推荐修改）
   - 打开CubeMX
   - SPI1 → Data Size → 改为`8 Bits`
   - 重新生成代码
   - 如果改为8位，需要恢复原始代码（去掉`/ 2`）

2. ⚠️ **FDCAN波特率**（必须修改）
   - 打开CubeMX
   - FDCAN1/2/3 → Bit Timing Parameters
   - 配置为1Mbps（见上文推荐配置）
   - 重新生成代码

3. ⚠️ **中断优先级**（可选优化）
   - 打开CubeMX
   - NVIC Settings
   - 调整中断优先级（见上文推荐配置）

4. ✅ **FDCAN3引脚已确认**
   - FDCAN3_TX: PA15，FDCAN3_RX: PA8

## 测试建议

### 1. SPI通信测试
```c
// 在slave_controller.c中添加调试代码
void SlaveController_ProcessSPIPacket(void) {
    SPIPacket_t* packet = &g_slaveCtrl.spiRxPacket;

    // 调试：打印接收到的数据
    // printf("Header: 0x%02X%02X\n", packet->headerH, packet->headerL);
    // printf("Sequence: %d\n", packet->sequence);

    // ... 原有代码
}
```

### 2. FDCAN通信测试
使用CAN分析仪监控总线：
- 检查发送的CAN ID是否正确（0x04-0x0C）
- 检查数据长度是否为8字节
- 检查波特率是否匹配

### 3. 完整系统测试
1. 连接主控和从控的SPI接口
2. 连接电机到FDCAN总线
3. 运行主控固件
4. 观察从控是否正确接收和转发数据

## 性能优化建议

### 1. 减少内存拷贝
当前实现中有多次memcpy操作，可以优化：
```c
// 优化前
memcpy(feedback->motorFeedback, g_slaveCtrl.motorFeedbackRaw, 72);

// 优化后：直接使用指针
feedback->motorFeedback = g_slaveCtrl.motorFeedbackRaw;
```

### 2. 使用循环缓冲区
对于高频率通信，可以使用双缓冲或循环缓冲区：
```c
SPIPacket_t spiRxBuffer[2];  // 双缓冲
uint8_t currentBuffer = 0;
```

### 3. 添加性能监控
```c
// 记录处理时间
uint32_t startTime = HAL_GetTick();
SlaveController_ProcessSPIPacket();
uint32_t processingTime = HAL_GetTick() - startTime;
```

## 兼容性检查清单

- [x] SPI配置与CubeMX一致
- [x] DMA配置与CubeMX一致
- [x] FDCAN配置与CubeMX一致
- [x] 中断处理函数名称正确
- [x] 引脚定义与硬件匹配
- [ ] 波特率配置需要修改（见上文）
- [ ] SPI位宽建议修改为8位

## 结论

从控制器代码已经修复以适配当前的CubeMX配置（16位SPI模式）。但为了获得最佳性能和可维护性，**强烈建议**：

1. 将SPI配置改为8位模式
2. 修正FDCAN波特率为1Mbps
3. 调整中断优先级

修复后的代码文件：
- `slave_controller.c` - 已适配16位SPI模式
- `slave_controller_old.c` - 原始版本备份

---

**重要提示**：如果修改CubeMX配置后重新生成代码，请确保不要覆盖USER CODE区域的自定义代码。
