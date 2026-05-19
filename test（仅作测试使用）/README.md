# 12 电机控制程序

通过 USB-CDC 控制 12 个电机全部旋转 30 度。

> 基于 PROTOCOL.md 通讯协议文档实现

## 目录结构

```
test/
├── README.md                     # 总说明文档
├── README_WINDOWS.md             # Windows 平台详细教程
├── README_LINUX.md               # Linux 平台详细教程
├── README_C.md                   # C 语言版本说明
├── motor_protocol.py             # Python 协议实现
├── control_12motors_windows.py   # Windows Python 版本
├── control_12motors_linux.py     # Linux Python 版本
├── motor_protocol.h              # C 语言协议头文件
├── motor_protocol.c              # C 语言协议实现
├── control_12motors.c            # C 语言主程序
└── Makefile                      # C 语言编译配置
```

## 协议说明

### USB 通讯参数

| 参数 | 值 |
|------|------|
| USB 模式 | USB-FS（Full Speed，12 Mbps） |
| USB 类 | CDC（虚拟串口） |
| 单次 OUT 事务最大载荷 | 64 字节 |
| 应用层数据包大小 | 164 字节（底层自动拆为 64+64+36） |
| 上位机接口 | Windows: COM 口 / Linux: /dev/ttyACM0 |
| 波特率设置 | 任意（CDC 虚拟串口不影响实际速率） |

### 控制数据包格式（164 字节）

```
Byte  0       : 帧头        0xAA
Byte  1       : 命令类型    0x10（电机控制）
Byte  2~3     : 数据长度    0x0060（= 96）
Byte  4       : 序列号      0x00~0xFF
Byte  5       : 保留        0x00
Bytes 6~101   : 电机数据    96 字节（12个电机 × 8字节）
Bytes 102~109 : 保留        8 字节
Byte  110     : XOR 校验和  对 Byte 0~109 做异或
Byte  111     : 帧尾        0x55
```

### 单个电机控制数据（8 字节，Big-Endian）

```
Bit 63~61 : 电机模式    3 bit   固定 0x00（力位混控）
Bit 60~49 : KP 参数     12 bit  0~4095
Bit 48~40 : KD 参数     9 bit   0~511
Bit 39~24 : 期望位置    16 bit  0~65535
Bit 23~12 : 期望速度    12 bit  0~4095
Bit 11~0  : 前馈扭矩    12 bit  0~4095
```

## 快速开始

### Python 版本（推荐）

#### Windows
```bash
pip install pyserial
cd c:\Users\roman\Desktop\DOG\test
python control_12motors_windows.py
```

#### Linux
```bash
pip3 install pyserial
cd ~/DOG/test
python3 control_12motors_linux.py
```

### C 语言版本

#### 编译
```bash
# Linux / macOS
make

# Windows (MinGW)
mingw32-make

# 或使用 gcc
gcc motor_protocol.c control_12motors.c -o control_12motors -lm
```

#### 运行
```bash
# Linux
./control_12motors /dev/ttyACM0

# Windows
control_12motors.exe COM3
```

## 功能说明

程序执行以下三个步骤：

1. **第一步**：将所有电机设置到 0 度位置（基准位置）
2. **第二步**：将所有电机旋转到 30 度位置
3. **第三步**：将所有电机回到 0 度位置

## 物理值换算

| 参数 | 物理值范围 | 原始值范围 | 换算公式（物理→原始） |
|------|-----------|-----------|---------------------|
| KP | 0 ~ 5 | 0 ~ 4095 | `raw = phys × 819` |
| KD | 0 ~ 50 | 0 ~ 511 | `raw = phys ÷ 0.0978` |
| 位置 | -60 ~ +60 rad | 0 ~ 65535 | `raw = (phys + 60) × 546.125` |
| 速度 | -60 ~ +60 rad/s | 0 ~ 4095 | `raw = (phys + 60) × 34.125` |
| 扭矩 | -60 ~ +60 Nm | 0 ~ 4095 | `raw = (phys + 60) × 34.125` |

## 详细文档

- [Windows 平台详细教程](./README_WINDOWS.md)
- [Linux 平台详细教程](./README_LINUX.md)
- [C 语言版本说明](./README_C.md)
