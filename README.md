<div align="center">

# DOG — 12-Axis Motor Control Gateway

**USB HS → 4× FDCAN 高性能电机控制网关系统**

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![GitHub stars](https://img.shields.io/github/stars/CofZhang/DOG?style=social)](https://github.com/CofZhang/DOG/stargazers)
[![GitHub forks](https://img.shields.io/github/forks/CofZhang/DOG?style=social)](https://github.com/CofZhang/DOG/network/members)
![GitHub Views](https://komarev.com/ghpvc/?username=CofZhang&label=Profile%20Views&color=blue&style=flat)
[![GitHub last commit](https://img.shields.io/github/last-commit/CofZhang/DOG)](https://github.com/CofZhang/DOG/commits/master)
[![Platform](https://img.shields.io/badge/Platform-STM32H743%20%7C%20STM32G474-blue)](https://www.st.com/)
[![Motor](https://img.shields.io/badge/Motor-EC--A6408--P2--25-green)](https://github.com/CofZhang/DOG)

</div>

---

## 简介

DOG 是一个面向四足机器人的**高性能电机控制网关系统**，实现了 USB FS → 4路 FDCAN 的实时控制链路，可同时驱动 **12 个关节电机**，控制频率达 **1KHz**。

系统采用主从双 MCU 架构：
- **主控 STM32H743**：接收上位机 USB 指令，直接控制 3 个电机，并通过 SPI 将剩余 9 个电机的指令转发给从机
- **从控 STM32G474**：接收 SPI 数据，通过 3 路 FDCAN 独立控制 9 个电机

---

## 系统架构

```
┌─────────────────┐
│  上位机 / Jetson │
│  (Python SDK)   │
└────────┬────────┘
         │ USB FS CDC  12Mbps  164字节数据包
         ↓
┌──────────────────────────────────────────┐
│          主控  STM32H743VIT6             │
│                                          │
│   USB CDC 接收/发送  ──→  数据包解析     │
│                    ├──→ FDCAN1 → 电机 1,2,3  │
│                    └──→ SPI1 Master ↓    │
└──────────────────────┬───────────────────┘
                       │ SPI  80字节数据包
┌──────────────────────┴───────────────────┐
│          从控  STM32G474RET6             │
│                                          │
│   SPI1 Slave 接收  ──→  数据分发         │
│                    ├──→ FDCAN1 → 电机 4,5,6  │
│                    ├──→ FDCAN2 → 电机 7,8,9  │
│                    └──→ FDCAN3 → 电机 10,11,12 │
└──────────────────────────────────────────┘
```

---

## 特性

- **12路同步控制**：4路 FDCAN 总线，每路最多 3 个电机，全部同步下发
- **1kHz 控制频率**：USB FS 12Mbps，端到端延迟约 0.31ms
- **力位混控模式**：支持位置、速度、前馈扭矩同时下发（MIT 协议）
- **实时反馈**：位置、速度、电流、线圈温度、MOS 温度全量回传
- **安全保护**：帧头帧尾校验 + XOR 校验和 + 超时自动停机
- **Python SDK**：开箱即用的上位机数据包构建工具

---

## 硬件配置

| 组件 | 型号 | 说明 |
|------|------|------|
| 主控 MCU | STM32H743VIT6 | USB HS + FDCAN1 + SPI Master |
| 从控 MCU | STM32G474RET6 | SPI Slave + FDCAN1/2/3 |
| 电机 | EC-A6408-P2-25 | 12个，MIT 协议 |
| 上位机 | Jetson / PC | Python 3.x |

### 电机 ID 映射

| 电机 | CAN ID | 控制器 | FDCAN 总线 |
|------|--------|--------|-----------|
| 1 | 0x01 | 主控 | FDCAN1 |
| 2 | 0x02 | 主控 | FDCAN1 |
| 3 | 0x03 | 主控 | FDCAN1 |
| 4 | 0x04 | 从控 | FDCAN1 |
| 5 | 0x05 | 从控 | FDCAN1 |
| 6 | 0x06 | 从控 | FDCAN1 |
| 7 | 0x07 | 从控 | FDCAN2 |
| 8 | 0x08 | 从控 | FDCAN2 |
| 9 | 0x09 | 从控 | FDCAN2 |
| 10 | 0x0A | 从控 | FDCAN3 |
| 11 | 0x0B | 从控 | FDCAN3 |
| 12 | 0x0C | 从控 | FDCAN3 |

---

## 通信协议

### USB 数据包（164 字节）

| 字节 | 字段 | 值 |
|------|------|----|
| 0 | 帧头 | 0xAA |
| 1 | 命令类型 | 0x10 = 电机控制 |
| 2~3 | 长度 | 大端序 |
| 4 | 序列号 | 滚动计数 |
| 5 | 保留 | 0x00 |
| 6~101 | 电机数据 | 12 × 8 字节 |
| 102~109 | 保留 | 扩展用 |
| 110 | 校验和 | XOR(Byte 0~109) |
| 111 | 帧尾 | 0x55 |

### 电机控制指令（8 字节，MIT 协议）

| 位域 | 位数 | 物理范围 | 换算 |
|------|------|---------|------|
| Bit63-61 | 3bit | 模式 0x00 | — |
| Bit60-49 | 12bit | KP 0~500 | raw / 8.19 |
| Bit48-40 | 9bit | KD 0~5 | raw × 0.00978 |
| Bit39-24 | 16bit | 位置 ±12.5 rad | (raw/65535)×25 - 12.5 |
| Bit23-12 | 12bit | 速度 ±18 rad/s | (raw/4095)×36 - 18 |
| Bit11-0 | 12bit | 扭矩 ±30 Nm | (raw/4095)×60 - 30 |

### 电机反馈数据（8 字节）

| 位域 | 位数 | 说明 | 换算 |
|------|------|------|------|
| Bit63-61 | 3bit | 报文类型 0x01 | — |
| Bit60-56 | 5bit | 错误码 | 见下表 |
| Bit55-40 | 16bit | 实际位置 | (raw/65535)×25 - 12.5 rad |
| Bit39-28 | 12bit | 实际速度 | (raw/4095)×36 - 18 rad/s |
| Bit27-16 | 12bit | 实际电流 | raw×60/4095 - 30 A |
| Bit15-8 | 8bit | 线圈温度 | (raw-50)/2 ℃ |
| Bit7-0 | 8bit | MOS 温度 | (raw-50)/2 ℃ |

**错误码**：0=正常，1=过热，2=过流，3=过压，4=欠压，5=编码器错误，6=刹车异常，7=DRV 驱动错误

---

## 快速开始

### 1. 编译固件

用 **Keil MDK** 分别打开并编译：

```
DOG_Master/MDK-ARM/DOG.uvprojx      # 主控固件
DOG_Slaver/MDK-ARM/DOG_G.uvprojx    # 从控固件
```

### 2. 烧录

通过 ST-Link 分别烧录主控和从控。

### 3. 上位机控制

```python
from 电机控制例程.usb_packet_builder import build_usb_packet

# 定义12个电机参数 [kp, kd, position, velocity, torque]
motors = [
    [10.0, 0.5, 0.0, 0.0, 0.0],  # 电机1
    # ... 共12个
]

packet = build_usb_packet(motors, sequence=1)  # 返回164字节
# 通过 USB CDC 虚拟串口发送 packet
```

### 4. 特殊指令

```
# 设置当前位置为零点（CAN ID = 0x7FF）
数据: 0x00 [电机ID] 0x00 0x03

# 配置超时时间（500ms）
数据: 0xC0 0x0B 0x01 0xF4

# 关闭超时保护
数据: 0xC0 0x0B 0x00 0x00
```

---

## 项目结构

```
DOG/
├── DOG_Master/              # 主控固件 (STM32H743)
│   └── Core/Src/
│       ├── main.c           # 主程序
│       ├── fdcan_handler.c  # FDCAN 收发
│       ├── motor.c          # 电机协议编解码
│       └── usbcol.c         # USB CDC 数据处理
├── DOG_Slaver/              # 从控固件 (STM32G474)
│   └── Core/Src/
│       ├── main.c           # 主程序
│       └── slave_controller.c  # SPI接收 + FDCAN分发
└── 电机控制例程/
    └── usb_packet_builder.py   # Python 上位机工具
```

---

## 性能指标

| 指标 | 数值 |
|------|------|
| 控制频率 | 1kHz |
| USB 带宽 | 480Mbps (HS) |
| 端到端延迟 | ~0.31ms |
| FDCAN 波特率 | 1Mbps |
| SPI 时钟 | ≥10MHz |
| 电机数量 | 12 |

---

## 常见问题


**FDCAN 无法通信**：确认波特率一致、CAN 收发器正常、终端电阻 120Ω 已接。

**SPI 通信异常**：检查时钟极性/相位、NSS 信号、DMA 对齐（Half Word）。

**电机不响应**：确认电机 ID、FDCAN 过滤器配置、电机供电和使能状态。KD 不能为 0。

---

## 版本历史

- **v1.0** (2026-05): 完成主从控制器架构，实现 USB FS + SPI + 4路 FDCAN 全链路控制

---

## 参考资料

- STM32H743 参考手册 RM0433
- STM32G474 参考手册 RM0440
- FDCAN 应用笔记 AN5348
- EC-A6408-P2-25 电机用户手册

---

## License

本项目基于 [MIT License](LICENSE) 开源。

```
MIT License

Copyright (c) 2026 Zhang

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
