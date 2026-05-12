# DOG 机器人通讯协议文档

> 适用固件版本：STM32H7xx，USB-FS CDC 模式
> 电机型号：EC-A6408-P2-25（12轴）

---

## 一、系统整体数据流程图

```
┌─────────────────────────────────────────────────────────────────┐
│                        上位机（Windows / Linux）                  │
│                                                                   │
│   write(fd, control_pkg, 164)      read(fd, feedback_pkg, 164)   │
└──────────────┬──────────────────────────────┬────────────────────┘
               │ USB-FS CDC（虚拟串口）         │
               │ 底层自动拆包：64+64+36 字节    │ 底层自动拆包：64+64+36 字节
               ▼                              ▲
┌─────────────────────────────────────────────────────────────────┐
│                     STM32H7xx 下位机                              │
│                                                                   │
│  OTG_FS_IRQHandler                                                │
│       │                                                           │
│       ▼                                                           │
│  HAL_PCD_IRQHandler                                               │
│       │                                                           │
│       ▼                                                           │
│  CDC_Receive_FS(Buf, Len)   ← 每次最多 64 字节，触发 3 次         │
│       │                                                           │
│       ▼                                                           │
│  Protocol_ProcessRxData()                                         │
│       │  逐字节写入环形缓冲区                                       │
│       ▼                                                           │
│  g_rx_buffer[512]  （环形缓冲区）                                  │
│       │                                                           │
│       │  主循环轮询                                                │
│       ▼                                                           │
│  Protocol_HasCompletePacket()  ← 检查 ≥164 字节 且首字节 = 0xAA  │
│       │ 满足条件                                                   │
│       ▼                                                           │
│  Protocol_ReadPacket()                                            │
│       │  解析成功：读指针 +164                                     │
│       │  解析失败：读指针 +1（重新寻找帧头）                        │
│       ▼                                                           │
│  g_motor_params[12]                                               │
│       │                                                           │
│       ├──[0..2]──▶ Motor_PackControlData()                        │
│       │                  │                                        │
│       │                  ▼                                        │
│       │           HAL_FDCAN_AddMessageToTxFifoQ()                 │
│       │                  │                                        │
│       │                  ▼                                        │
│       │           FDCAN1 总线 ──▶ 电机 1、2、3                    │
│       │                                                           │
│       └──[3..11]─▶ SPI_PackMotorData()                           │
│                          │                                        │
│                          ▼                                        │
│                   HAL_SPI_Transmit_DMA()                          │
│                          │                                        │
│                          ▼                                        │
│                   SPI 总线 ──▶ 从控 STM32G474 ──▶ 电机 4~12      │
│                                                                   │
│  ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  │
│                                                                   │
│  电机 1~3 反馈                                                     │
│       │                                                           │
│       ▼                                                           │
│  FDCAN1 总线 ──▶ FDCAN1_IT0_IRQHandler                           │
│                          │                                        │
│                          ▼                                        │
│                  HAL_FDCAN_IRQHandler                             │
│                          │                                        │
│                          ▼                                        │
│                  HAL_FDCAN_RxFifo0Callback()                      │
│                          │  Motor_UnpackFeedbackData()            │
│                          ▼                                        │
│                  g_motor_feedback[12]                             │
│                          │                                        │
│                          │  每 10 次控制周期触发一次               │
│                          ▼                                        │
│                  Protocol_SendFeedback()                          │
│                          │                                        │
│                          ▼                                        │
│                  CDC_Transmit_FS(feedback_pkg, 164)               │
│                          │  底层自动拆包发送                       │
└──────────────────────────┼─────────────────────────────────────-─┘
                           │
                           ▼ USB-FS CDC
                      上位机接收反馈
```

---

## 二、USB 通讯参数

| 参数 | 值 |
|------|----|
| USB 模式 | USB-FS（Full Speed，12 Mbps） |
| USB 类 | CDC（虚拟串口，Virtual COM Port） |
| 单次 OUT 事务最大载荷 | 64 字节 |
| 应用层数据包大小 | **164 字节**（底层自动拆为 64+64+36） |
| 上位机接口 | Windows: COM 口 / Linux: /dev/ttyACM0 |
| 波特率设置 | 任意（CDC 虚拟串口，波特率不影响实际速率） |

---

## 三、上位机 → 下位机：控制数据包格式

### 3.1 包结构总览（164 字节）

```
Byte  0      : 帧头        0xAA
Byte  1      : 命令类型    0x10（电机控制）
Byte  2      : 数据长度高字节  0x00
Byte  3      : 数据长度低字节  0x60（= 96，12个电机×8字节）
Byte  4      : 序列号      0x00~0xFF（每包递增，用于丢包检测）
Byte  5      : 保留        0x00
Bytes 6~101  : 电机控制数据  96 字节（12个电机，每个8字节）
Bytes 102~109: 保留        8 字节，填 0x00
Byte  110    : XOR 校验和  对 Byte 0~109 做异或
Byte  111    : 帧尾        0x55
```

> **注意**：`USB_PKG_TOTAL_LEN = 164`，但实际有效字节布局为上表所示 112 字节，
> 其余字节为保留填充。上位机发送时需按 164 字节总长度填充。

### 3.2 单个电机控制数据（8 字节，Big-Endian）

```
Bit 63~61 : 电机模式    3 bit   固定 0x00（力位混控）
Bit 60~49 : KP 参数     12 bit  原始值范围 0~4095（物理值 0~500）
Bit 48~40 : KD 参数     9 bit   原始值范围 0~511（物理值 0~5）
Bit 39~24 : 期望位置    16 bit  原始值范围 0~65535（物理值 -12.5~+12.5 rad）
Bit 23~12 : 期望速度    12 bit  原始值范围 0~4095（物理值 -18~+18 rad/s）
Bit 11~0  : 前馈扭矩    12 bit  原始值范围 0~4095（物理值 -30~+30 Nm）
```

### 3.3 物理值 ↔ 原始值换算

| 参数 | 物理值范围 | 原始值范围 | 换算公式（物理→原始） | 换算公式（原始→物理） |
|------|-----------|-----------|---------------------|---------------------|
| KP | 0 ~ 500 | 0 ~ 4095 | `raw = phys / 500 * 4095` | `phys = raw * 500 / 4095` |
| KD | 0 ~ 5 | 0 ~ 511 | `raw = phys / 5 * 511` | `phys = raw * 5 / 511` |
| 位置 | -12.5 ~ +12.5 rad | 0 ~ 65535 | `raw = (phys + 12.5) / 25 * 65535` | `phys = raw * 25 / 65535 - 12.5` |
| 速度 | -18 ~ +18 rad/s | 0 ~ 4095 | `raw = (phys + 18) / 36 * 4095` | `phys = raw * 36 / 4095 - 18` |
| 扭矩 | -30 ~ +30 Nm | 0 ~ 4095 | `raw = (phys + 30) / 60 * 4095` | `phys = raw * 60 / 4095 - 30` |

### 3.4 电机编号与数据偏移

| 电机编号 | 数组索引 | 数据起始字节（包内偏移） | 控制路径 |
|---------|---------|----------------------|---------|
| 电机 1 | [0] | Byte 6 | 主控 FDCAN1（CAN ID 0x11） |
| 电机 2 | [1] | Byte 14 | 主控 FDCAN1（CAN ID 0x12） |
| 电机 3 | [2] | Byte 22 | 主控 FDCAN1（CAN ID 0x13） |
| 电机 4 | [3] | Byte 30 | SPI → 从控（CAN ID 0x14） |
| 电机 5 | [4] | Byte 38 | SPI → 从控（CAN ID 0x15） |
| 电机 6 | [5] | Byte 46 | SPI → 从控（CAN ID 0x16） |
| 电机 7 | [6] | Byte 54 | SPI → 从控（CAN ID 0x17） |
| 电机 8 | [7] | Byte 62 | SPI → 从控（CAN ID 0x18） |
| 电机 9 | [8] | Byte 70 | SPI → 从控（CAN ID 0x19） |
| 电机 10 | [9] | Byte 78 | SPI → 从控（CAN ID 0x1A） |
| 电机 11 | [10] | Byte 86 | SPI → 从控（CAN ID 0x1B） |
| 电机 12 | [11] | Byte 94 | SPI → 从控（CAN ID 0x1C） |

### 3.5 XOR 校验和计算

对 Byte 0 到 Byte 109（共 110 字节）逐字节异或：

```python
checksum = 0
for i in range(110):
    checksum ^= data[i]
data[110] = checksum
```

---

## 四、下位机 → 上位机：反馈数据包格式

### 4.1 包结构总览（164 字节）

```
Byte  0      : 帧头        0xAA
Byte  1      : 命令类型    0x10
Byte  2      : 数据长度高字节  0x00
Byte  3      : 数据长度低字节  0x60（= 96）
Byte  4      : 序列号      与对应控制包序列号一致
Byte  5      : 保留        0x00
Bytes 6~101  : 电机反馈数据  96 字节（12个电机，每个8字节）
Bytes 102~109: 保留        8 字节
Byte  110    : XOR 校验和
Byte  111    : 帧尾        0x55
```

> 反馈包每 10 次控制周期发送一次。

### 4.2 单个电机反馈数据（8 字节，Big-Endian）

```
Bit 63~61 : 报文类型    3 bit   固定 0x01
Bit 60~56 : 错误码      5 bit   见错误码表
Bit 55~40 : 实际位置    16 bit  原始值范围 0~65535
Bit 39~28 : 实际速度    12 bit  原始值范围 0~4095
Bit 27~16 : 实际电流    12 bit  原始值范围 0~4095
Bit 15~8  : 温度        8 bit   原始值范围 0~255
Bit 7~0   : 未使用      8 bit   填 0x00
```

### 4.3 反馈物理值换算

| 参数 | 换算公式（原始→物理） |
|------|---------------------|
| 位置 | `phys = raw * 25 / 65535 - 12.5`（rad） |
| 速度 | `phys = raw * 36 / 4095 - 18`（rad/s） |
| 电流 | `phys = raw * 60 / 4095 - 30`（A，范围 -30~+30） |
| 温度 | `phys = (raw - 50) / 2`（℃） |

### 4.4 错误码表

| 错误码 | 含义 |
|--------|------|
| 0x00 | 正常 |
| 0x01 | 过热 |
| 0x02 | 过流 |
| 0x03 | 电压过高 |
| 0x04 | 电压过低 |
| 0x05 | 编码器错误 |
| 0x06 | 刹车电压异常 |
| 0x07 | DRV 驱动错误 |

---

## 五、上位机示例代码

### Python（Linux /dev/ttyACM0）

```python
import serial
import struct

def pack_motor_control(motors, sequence):
    """
    motors: list of 12 dicts, each with keys: kp, kd, position, velocity, torque
    sequence: uint8
    returns: 164-byte bytes object
    """
    def to_raw_kp(v):   return min(4095, int(v / 500 * 4095 + 0.5))
    def to_raw_kd(v):   return min(511,  int(v / 5 * 511 + 0.5))
    def to_raw_pos(v):  return min(65535, int((v + 12.5) / 25 * 65535 + 0.5))
    def to_raw_vel(v):  return min(4095, int((v + 18) / 36 * 4095 + 0.5))
    def to_raw_trq(v):  return min(4095, int((v + 30) / 60 * 4095 + 0.5))

    data = bytearray(164)
    data[0] = 0xAA          # 帧头
    data[1] = 0x10          # 命令类型
    data[2] = 0x00          # 数据长度高字节
    data[3] = 0x60          # 数据长度低字节 (96)
    data[4] = sequence & 0xFF
    data[5] = 0x00

    for i, m in enumerate(motors):
        bits  = (0x00 & 0x07) << 61
        bits |= (to_raw_kp(m['kp'])       & 0xFFF) << 49
        bits |= (to_raw_kd(m['kd'])       & 0x1FF) << 40
        bits |= (to_raw_pos(m['position'])& 0xFFFF)<< 24
        bits |= (to_raw_vel(m['velocity'])& 0xFFF) << 12
        bits |= (to_raw_trq(m['torque'])  & 0xFFF) << 0
        packed = struct.pack('>Q', bits)  # Big-Endian uint64
        data[6 + i*8 : 6 + i*8 + 8] = packed

    # XOR 校验
    checksum = 0
    for b in data[:110]:
        checksum ^= b
    data[110] = checksum
    data[111] = 0x55        # 帧尾

    return bytes(data)


def unpack_feedback(data):
    """
    data: 164-byte bytes object
    returns: list of 12 dicts with motor feedback
    """
    def to_pos(r):  return r * 25 / 65535 - 12.5
    def to_vel(r):  return r * 36 / 4095 - 18
    def to_cur(r):  return r * 60 / 4095 - 30
    def to_tmp(r):  return (r - 50) / 2

    motors = []
    for i in range(12):
        bits = struct.unpack_from('>Q', data, 6 + i*8)[0]
        motors.append({
            'error_code': (bits >> 56) & 0x1F,
            'position':   to_pos((bits >> 40) & 0xFFFF),
            'velocity':   to_vel((bits >> 28) & 0xFFF),
            'current':    to_cur((bits >> 16) & 0xFFF),
            'temperature': to_tmp((bits >>  8) & 0xFF),
        })
    return motors


# 使用示例
ser = serial.Serial('/dev/ttyACM0', baudrate=115200, timeout=1)

motors = [{'kp': 1.0, 'kd': 0.5, 'position': 0.0, 'velocity': 0.0, 'torque': 0.0}] * 12
seq = 0

pkg = pack_motor_control(motors, seq)
ser.write(pkg)
seq = (seq + 1) & 0xFF

# 接收反馈（每10次控制发一次）
raw = ser.read(164)
if len(raw) == 164 and raw[0] == 0xAA and raw[111] == 0x55:
    feedback = unpack_feedback(raw)
    print(f"Motor 1 position: {feedback[0]['position']:.3f} rad")
```

---

## 六、接收容错机制

下位机环形缓冲区大小为 512 字节，解析逻辑如下：

```
收到数据 → 写入环形缓冲区
                │
                ▼
        缓冲区 ≥ 164 字节？
        当前读指针 = 0xAA？
                │ 是
                ▼
        复制 164 字节到临时缓冲区
                │
                ▼
        校验帧尾、校验和、数据长度
                │
        ┌───────┴───────┐
        │ 成功           │ 失败
        ▼               ▼
   读指针 +164      读指针 +1
   解析电机数据     重新寻找下一个 0xAA
```
