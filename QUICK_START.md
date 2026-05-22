# 快速入门指南

## 1. 硬件准备

### 1.1 所需硬件

- STM32H743VIT6开发板（主控制器）
- STM32G474RET6开发板（从控制器）
- USB3320 ULPI PHY芯片（用于USB HS）
- CAN收发器（如TJA1050）×4
- EC-A6408-P2-25电机×12
- Jetson开发板或PC（运行上位机软件）
- USB Type-C线缆
- 电源适配器

### 1.2 硬件连接

#### 主控制器连接

1. **USB HS连接**:
   - 连接USB3320 PHY芯片到STM32H743的ULPI接口
   - USB Type-C线连接到Jetson/PC

2. **FDCAN1连接**:
   - 连接CAN收发器到PD0(RX)和PD1(TX)
   - CAN_H和CAN_L连接到电机1、2、3

3. **SPI1连接**:
   - PA4(NSS) → 从控PA4
   - PB3(SCK) → 从控PA5
   - PA6(MISO) → 从控PA6
   - PA7(MOSI) → 从控PA7
   - GND → GND

#### 从控制器连接

1. **FDCAN1连接**:
   - 连接CAN收发器到PA11(RX)和PA12(TX)
   - CAN_H和CAN_L连接到电机4、5、6

2. **FDCAN2连接**:
   - 连接CAN收发器到PB12(RX)和PB13(TX)
   - CAN_H和CAN_L连接到电机7、8、9

3. **FDCAN3连接**:
   - 连接CAN收发器到PA8(RX)和PA15(TX)
   - CAN_H和CAN_L连接到电机10、11、12

4. **SPI1连接**: 见主控制器连接

**注意**: 每条CAN总线需要120Ω终端电阻。

## 2. 固件烧录

### 2.1 主控制器固件

1. 打开主控制器工程（`DOG`文件夹）
2. 使用STM32CubeIDE或Keil MDK编译
3. 通过ST-Link烧录到STM32H743VIT6
4. 复位MCU

### 2.2 从控制器固件

1. 打开从控制器工程（`DOG_G`文件夹）
2. 编译工程
3. 通过ST-Link烧录到STM32G474RET6
4. 复位MCU

### 2.3 验证烧录

- 主控制器LED应该闪烁（如果有）
- 从控制器应该进入SPI等待状态
- USB设备应该被识别为CDC虚拟串口

## 3. 上位机软件安装

### 3.1 Python环境

```bash
# 安装Python 3.8或更高版本
sudo apt-get install python3 python3-pip

# 安装pyserial库
pip3 install pyserial
```

### 3.2 复制Python文件

将以下文件复制到工作目录：
- `motor_controller.py` - 电机控制器SDK
- `motor_control_examples.py` - 示例程序

### 3.3 设置串口权限（Linux）

```bash
# 将用户添加到dialout组
sudo usermod -a -G dialout $USER

# 重新登录或执行
newgrp dialout
```

## 4. 第一次运行

### 4.1 查找USB设备

**Linux**:
```bash
# 查看USB CDC设备
ls /dev/ttyACM*

# 应该看到类似 /dev/ttyACM0
```

**Windows**:
```
打开设备管理器 → 端口(COM和LPT) → 查找"USB Serial Device (COMx)"
```

### 4.2 测试连接

```bash
# 运行基础测试
python3 motor_controller.py /dev/ttyACM0

# Windows
python motor_controller.py COM3
```

如果连接成功，应该看到：
```
已连接到 /dev/ttyACM0
开始电机控制测试...
按Ctrl+C停止
```

### 4.3 运行示例程序

```bash
# 正弦波控制测试
python3 motor_control_examples.py /dev/ttyACM0 1

# 阶跃响应测试
python3 motor_control_examples.py /dev/ttyACM0 2

# 单独电机控制
python3 motor_control_examples.py /dev/ttyACM0 3
```

## 5. 常见问题

### 5.1 USB设备无法识别

**问题**: 插入USB后没有识别到设备

**解决方法**:
1. 检查USB3320 PHY芯片供电（3.3V）
2. 检查ULPI接口连接
3. 确认主控固件已正确烧录
4. 尝试更换USB线缆
5. 查看设备管理器是否有未知设备

### 5.2 电机不响应

**问题**: 发送指令后电机没有动作

**解决方法**:
1. 检查电机供电（24V或48V）
2. 检查CAN总线连接（CAN_H, CAN_L）
3. 确认终端电阻（120Ω）已连接
4. 检查电机ID配置是否正确
5. 使用CAN分析仪查看总线数据

### 5.3 SPI通信异常

**问题**: 从控制器无法接收主控数据

**解决方法**:
1. 检查SPI连线（NSS, SCK, MISO, MOSI）
2. 确认主从时钟极性和相位一致
3. 检查GND连接
4. 使用逻辑分析仪查看SPI波形

### 5.4 数据包校验失败

**问题**: 上位机提示"校验和错误"

**解决方法**:
1. 检查USB线缆质量
2. 降低控制频率（从1kHz降到100Hz）
3. 检查固件版本是否匹配
4. 重新烧录固件

### 5.5 电机抖动

**问题**: 电机运行时抖动或不稳定

**解决方法**:
1. 降低KP参数（从2.0降到1.0）
2. 增加KD参数（从0.5增到1.0）
3. 检查电机编码器连接
4. 降低控制频率
5. 检查电源质量

## 6. 性能优化

### 6.1 提高控制频率

默认示例程序运行在100Hz，可以提高到1kHz：

```python
# 将 time.sleep(0.01) 改为
time.sleep(0.001)
```

**注意**: 高频率需要更强的CPU性能和稳定的USB连接。

### 6.2 减少延迟

1. 使用USB 3.0端口
2. 关闭不必要的后台程序
3. 使用实时操作系统（RTOS）
4. 优化固件中的中断优先级

### 6.3 提高稳定性

1. 添加数据包重传机制
2. 实现心跳检测
3. 增加错误恢复逻辑
4. 使用看门狗定时器

## 7. 进阶功能

### 7.1 修改电机ID

如果需要修改电机的CAN ID：

1. 编辑`motor_protocol.h`中的宏定义：
```c
#define MOTOR_ID_1       0x01  // 改为新的ID
```

2. 重新编译并烧录固件

3. 或者使用全局指令修改（CAN ID = 0x7FF）：
```python
# 发送ID修改指令（需要实现）
```

### 7.2 调整FDCAN波特率

如果需要更高的CAN波特率：

1. 使用STM32CubeMX重新配置FDCAN
2. 修改`fdcan.c`中的参数
3. 确保所有设备使用相同的波特率

### 7.3 添加日志记录

```python
import logging

logging.basicConfig(
    filename='motor_control.log',
    level=logging.INFO,
    format='%(asctime)s - %(message)s'
)

# 在控制循环中记录数据
logging.info(f"Motor1 Position: {feedbacks[0].position}")
```

### 7.4 实时数据可视化

```python
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# 创建实时绘图（需要额外实现）
```

## 8. 安全注意事项

### 8.1 电气安全

- 确保电源电压正确（MCU: 3.3V, 电机: 24V/48V）
- 使用合适的保险丝
- 避免短路
- 注意高压危险

### 8.2 机械安全

- 首次测试时使用小幅度运动
- 确保电机周围无障碍物
- 准备紧急停止按钮
- 佩戴防护装备

### 8.3 软件安全

- 始终实现超时保护
- 限制电机运动范围
- 监控电机温度和电流
- 实现错误恢复机制

## 9. 电机反馈数据说明

### 9.1 反馈数据结构

每个电机的反馈数据解析后存储在 `MotorFeedback` 结构体中，包含以下物理值：

| 字段 | 类型 | 单位 | 范围 | 说明 |
|------|------|------|------|------|
| `error_code` | uint8 | — | 0~7 | 错误码，0=正常 |
| `position` | float | rad | -12.5 ~ +12.5 | 实际位置 |
| `velocity` | float | rad/s | -18 ~ +18 | 实际速度 |
| `current` | float | A | -60 ~ +60 | 实际电流 |
| `temperature` | float | ℃ | — | 电机温度 |
| `mos_temperature` | float | ℃ | — | MOS管温度 |

错误码定义：

| 值 | 含义 |
|----|------|
| 0 | 无错误 |
| 1 | 电机过热 |
| 2 | 电机过流 |
| 3 | 母线过压 |
| 4 | 母线欠压 |
| 5 | 编码器错误 |
| 6 | 刹车电压过高 |
| 7 | DRV驱动错误 |

### 9.2 反馈数据流向

```
电机(CAN反馈帧)
    │
    ├─ 电机1-3 ──→ 主控 FDCAN1 接收中断
    │               └─ Motor_UnpackFeedbackData()
    │               └─ g_motor_feedback[0~2]（主控 fdcan_handler.c）
    │
    └─ 电机4-12 ─→ 从控 FDCAN1/2/3 接收中断
                    └─ memcpy 原始8字节 → motorFeedbackRaw[9][8]
                    └─ SPI 透传给主控（SPIFeedbackPacket_t）
                    └─ 主控 SPI_TxCpltCallback() 解析
                    └─ g_slave_feedback[0~8]（主控 u_spi.c）

最终合并：
    System_SendFeedback() 每10帧调用一次
    → g_motor_feedback[0~2]  （电机1-3，主控FDCAN直接收）
    → g_motor_feedback[3~11] （电机4-12，从控SPI回传）
    → Protocol_PackFeedbackPkg() 打包 → USB 发给上位机
```

### 9.3 Keil 调试观察反馈数据

**主控（STM32H743）调试时：**

在 Watch 窗口输入以下表达式可直接查看物理值：

```
system_control\g_motor_feedback
```

展开后可看到12个电机的完整反馈，例如：

```
g_motor_feedback[0].position        // 电机1 位置 (rad)
g_motor_feedback[0].velocity        // 电机1 速度 (rad/s)
g_motor_feedback[0].current         // 电机1 电流 (A)
g_motor_feedback[0].temperature     // 电机1 电机温度 (℃)
g_motor_feedback[0].mos_temperature // 电机1 MOS温度 (℃)
g_motor_feedback[0].error_code      // 电机1 错误码

g_motor_feedback[3].position        // 电机4 位置（从机回传）
...
g_motor_feedback[11].position       // 电机12 位置（从机回传）
```

> 注意：`g_motor_feedback` 是 `static` 变量，Watch 窗口需要加模块限定符 `system_control\g_motor_feedback`，或者在该变量所在文件（`system_control.c`）中Protocol_SendFeedback(g_motor_feedback, g_sequence); 147行打断点后直接输入变量名。

**从控（STM32G474）调试时：**

从控只保存原始8字节，不解析物理值：

```
slave_controller\g_slaveCtrl.motorFeedbackRaw
```

`motorFeedbackRaw[0]` 对应电机4，`motorFeedbackRaw[8]` 对应电机12，每个元素是8字节原始CAN数据，需要手动按协议解析。

**SPI 从机反馈缓存（主控侧）：**

```
u_spi\g_slave_feedback
```

这是主控解析完从机 SPI 反馈后的物理值缓存，索引0对应电机4，索引8对应电机12。

### 9.4 反馈更新频率

- 主控 FDCAN 反馈：实时更新（中断驱动）
- 从控 SPI 反馈：每次 SPI 全双工传输完成后更新，与控制指令同步
- USB 上报频率：每10次控制指令发送一次反馈（`g_feedback_counter >= 10`）

## 10. 电机校准系统

### 10.1 背景与原理

机器狗的12个电机均配备绝对编码器，存在两个关键位置：

- **仿真零点**：仿真环境中使用的参考位置（0 rad）
- **实际标定位置**：机器狗组装完成后，实体机器人的对应姿态位置

上位机始终工作在**关节空间**（仿真坐标系），固件在发送给电机前完成**关节空间 → 电机空间**的完整变换，变换公式如下：

```
电机位置 = (关节位置 × 旋转方向 + 关节偏移) × 减速比
电机速度 = 关节速度 × 旋转方向 × 减速比
电机力矩 = 关节力矩 / 旋转方向 / 减速比
电机 kp  = 关节 kp  / 减速比²
电机 kd  = 关节 kd  / 减速比²
```

其中：
- **关节偏移**：存储在 Flash 中，通过拨码开关校准流程写入（见 10.5 节）
- **旋转方向**：宏定义在 `motor.h`，`+1` 正转，`-1` 反转
- **减速比**：宏定义在 `motor.h`，电机转速 / 关节转速

### 10.2 相关文件

| 文件 | 说明 |
|------|------|
| `DOG_Master/Core/Inc/motor.h` | 旋转方向宏（`MOTOR_DIR_x`）和减速比宏（`MOTOR_RATIO_x`） |
| `DOG_Master/Core/Inc/motor_calib.h` | 校准模块头文件，含数据结构和接口声明 |
| `DOG_Master/Core/Src/motor_calib.c` | 校准模块实现，含 Flash 读写、采样、关节→电机空间变换 |

### 10.3 Flash 存储布局

校准数据存储在 **Bank B Sector 7**（地址 `0x081E0000`，128KB），远离程序代码区，烧录固件不会覆盖校准数据。

```
偏移    大小    内容
0x00    4字节   Magic Number (0xCAFEBEEF)，用于判断数据是否有效
0x04    48字节  position_offset[12]，12个电机的位置偏移量（float，rad）
0x34    48字节  last_boot_pos[12]，上次上电时记录的初始位置（用于多圈补偿）
0x64    4字节   CRC32 校验（对前100字节计算）
```

### 10.4 校准模式触发

主控板上的 **BIAO_Pin（GPIOE Pin5）** 为校准触发引脚：

- **高电平（默认）**：正常工作模式
- **低电平（拨码开关拨下）**：进入校准模式

上电时固件检测该引脚状态，决定进入哪个流程。**校准只需做一次**，完成后拨回高电平，此后每次上电自动加载已保存的偏移量。

### 10.5 校准操作步骤（出厂一次性操作）

> **前提**：机器狗已完成机械组装，电机已上电并能正常通信。

**第一步：摆好标定姿态**

将机器狗摆放到实际环境中的标准站立姿态（与仿真中零点姿态对应的实体姿态）。确保姿态稳定，不要有外力干扰。

**第二步：拨下校准开关**

将 BIAO_Pin 对应的拨码开关拨到低电平位置。

**第三步：上电**

给主控板上电。固件启动后检测到 BIAO_Pin 为低，自动进入 `CALIB_STATE_CALIBRATING` 状态。

**第四步：等待校准完成**

固件会主动向所有12个电机发送零扭矩控制帧触发反馈，收集 **10帧** 取平均值，约需 **200ms**（每轮间隔20ms，共10轮）。此过程完全独立于上位机，无需 USB 连接。采集完成后自动将偏移量写入 Flash，状态变为 `CALIB_STATE_CALIB_DONE`。

可通过 Keil Watch 窗口观察校准状态：
```
motor_calib\g_calib_state
```
值为 `3`（CALIB_DONE）时表示校准完成。

也可观察写入的偏移量：
```
motor_calib\g_calib.position_offset
```

**第五步：拨回开关**

将拨码开关拨回高电平。**此后无需再操作此开关。**

**第六步：重新上电验证**

重新上电，固件进入正常模式，从 Flash 加载偏移量，收集10帧反馈后进入 `CALIB_STATE_READY`，偏移量自动生效。

### 10.6 正常上电流程（每次上电自动执行）

```
上电
  │
  ├─ 从 Flash 读取校准数据（magic + 偏移量 + 上次上电位置）
  │   └─ 若 Flash 无效（首次烧录）→ 写入默认偏移量（全0）
  │
  ├─ 检测 BIAO_Pin
  │   ├─ 低电平 → CALIBRATING（校准模式，见10.5节）
  │   └─ 高电平 → COLLECTING（正常上电，继续下面流程）
  │
  ├─ System_BootCalibSample()：主动发零扭矩控制帧给全部12个电机
  │   ├─ 电机1-3：直接写 FDCAN FIFO，每轮间隔 20ms
  │   ├─ 电机4-12：通过 SPI 发给从控，从控转发 FDCAN
  │   ├─ FDCAN 接收中断自动将反馈存入缓存
  │   ├─ 每轮读取缓存，喂给采样器
  │   ├─ 重复直到收满 10 帧（约 200ms）
  │   └─ 超时保护：5秒内无反馈则跳过（电机未连接时不卡死）
  │
  ├─ 计算各电机上电平均位置
  │
  ├─ 多圈补偿检测
  │   └─ 若当前上电位置与上次记录相差 > π rad
  │       → 说明电机在上次校准后转过了整圈
  │       → 偏移量自动 ±2π 补偿
  │
  ├─ 将本次上电位置写回 Flash（更新 last_boot_pos）
  │
  └─ 进入 READY 状态，偏移量开始生效，主循环启动
```

> 注意：整个采样过程在 `System_Control_Init()` 内阻塞完成，主循环 `while(1)` 在采样结束后才开始运行。上位机无需提前连接。

### 10.7 多圈补偿说明

绝对编码器的量程为 ±12.5 rad（约 ±2 圈），当电机在两次上电之间转过了整圈，编码器读数会发生跳变（例如从 +11 rad 跳到 -11 rad）。

固件通过比较本次上电位置与上次记录的上电位置来检测这种情况：

| 差值 | 判断 | 补偿动作 |
|------|------|----------|
| `diff > π` | 电机正向转了整圈 | `offset -= 2π` |
| `diff < -π` | 电机反向转了整圈 | `offset += 2π` |
| `\|diff\| ≤ π` | 正常范围 | 不补偿 |

### 10.8 校准状态说明

| 状态值 | 枚举名 | 含义 |
|--------|--------|------|
| 0 | `CALIB_STATE_COLLECTING` | 正常上电，正在收集初始位置样本 |
| 1 | `CALIB_STATE_CALIBRATING` | 校准模式，正在采集标定位置 |
| 2 | `CALIB_STATE_READY` | 就绪，偏移量已加载，正常工作 |
| 3 | `CALIB_STATE_CALIB_DONE` | 校准完成，偏移量已写入 Flash |
| 4 | `CALIB_STATE_ERROR` | Flash 读写错误 |

> 注意：`COLLECTING` 和 `CALIBRATING` 状态下，`Motor_Calib_ApplyOffset()` 不生效（原样透传指令位置），避免在偏移量确认前影响电机运动。

### 10.9 重新校准

如果机器狗重新组装或偏移量需要更新，重复 **10.5节** 的操作即可。新的偏移量会覆盖 Flash 中的旧数据。

### 10.10 常见问题

**Q：校准后重新上电，电机位置有偏差？**

检查校准时机器狗姿态是否与仿真零点姿态严格对应。可通过 Keil 查看 `motor_calib\g_calib.position_offset` 确认各电机偏移量是否合理（应在 ±几 rad 范围内，不应接近 ±12.5 rad）。

**Q：`g_calib_state` 一直停在 COLLECTING/CALIBRATING，不进入 READY？**

固件会主动发控制帧触发反馈，若5秒内仍无法收满10帧，会超时跳出采样循环（状态停留在 COLLECTING/CALIBRATING，不会进入 READY）。检查：
1. 电机是否正常上电并通信（CAN 总线是否正常，终端电阻是否接好）
2. 主控 FDCAN 和从控 SPI 是否正常工作
3. 查看 `motor_calib\g_boot_sample_cnt` 是否在递增（若为0说明完全没收到反馈）
4. 查看 `motor_calib\g_boot_pos_cnt` 数组，确认哪些电机有有效反馈（值>0）

**Q：`g_calib_state` 变为 ERROR（值为4）？**

Flash 写入失败。检查：
1. 是否在 Keil 调试模式下（调试模式下 Flash 操作可能受限）
2. 确认 `CALIB_FLASH_ADDR`（`0x081E0000`）未被链接脚本占用
3. 检查 Flash 是否有写保护

**Q：想手动设置偏移量而不用拨码开关？**

可调用 `Motor_Calib_SaveOffsets(offsets)` 直接写入，或在 Keil 调试时通过内存窗口修改 Flash 数据。

### 10.11 旋转方向与减速比配置

旋转方向和减速比均以宏定义形式集中在 `DOG_Master/Core/Inc/motor.h`，修改后重新编译烧录即可生效，无需重新校准。

#### 旋转方向（`MOTOR_DIR_x`）

```c
/* motor.h */
#define MOTOR_DIR_1    1    /* +1=正转，-1=反转 */
#define MOTOR_DIR_2    1
// ...
#define MOTOR_DIR_12   1
```

判断方法：给某个关节发一个小正值位置指令，观察电机实际转动方向是否与期望一致。若相反，将对应宏改为 `-1`。

#### 减速比（`MOTOR_RATIO_x`）

```c
/* motor.h */
#define MOTOR_RATIO_1    1.0f   /* 电机转速 / 关节转速 */
#define MOTOR_RATIO_2    1.0f
// ...
#define MOTOR_RATIO_12   1.0f
```

填入实际减速比后，固件会自动完成所有5个参数（位置、速度、力矩、kp、kd）的缩放，上位机无需关心减速比。

#### 变换在代码中的位置

变换由 `Motor_Calib_ApplyTransform(motor_idx, &param)` 完成，在控制指令打包前调用：
- 电机1-3：`system_control.c` → `System_ProcessMotorControl()`
- 电机4-12：`u_spi.c` → `SPI_PackMotorData()`

从机（STM32G474）不参与任何变换，直接转发主机已处理好的数据。

### 10.12 电机掉线保护

#### 触发条件

每次 `System_SendFeedback()` 调用时（每10帧控制指令触发一次），固件检查12个电机的反馈时间戳是否有更新。若某电机的时间戳连续 **20次** 未更新，则判定该电机掉线。

以1kHz控制频率为例，掉线判定时间约为：
```
20次 × 10帧 × 1ms = 200ms
```

#### 报警行为

- 任意一个电机掉线 → **蜂鸣器以0.5s为周期间歇鸣叫**（响0.5s → 停0.5s → 循环）
- 所有掉线电机恢复通信 → 蜂鸣器自动停止

掉线和恢复均为自动检测，无需人工干预。

#### 调试查询

在 Keil Watch 窗口可查看各电机掉线状态：

```
system_control\g_motor_offline        // 掉线标志数组，1=掉线，0=正常
system_control\g_motor_offline_cnt    // 各电机连续无反馈计数（0~20）
```

也可在代码中调用：
```c
System_IsMotorOffline(motor_idx);  // 0~11，返回1表示该电机掉线
```

#### 相关参数

掉线阈值定义在 `system_control.c`：
```c
#define MOTOR_OFFLINE_THRESHOLD  20   /* 连续失败次数，可按需调整 */
```

报警闪烁间隔固定为 500ms，由 `Tim_IsElapsed()` 驱动，不阻塞主循环。

## 11. 蜂鸣器说明

### 11.1 硬件配置

蜂鸣器由 TIM17 CH1（PB9）PWM 驱动，相关参数：

| 参数 | 值 |
|------|----|
| TIM17 时钟源 | APB2（120MHz） |
| Prescaler | 239（计数时钟 = 120MHz ÷ 240 = **500kHz**） |
| 默认报警频率 | 1000Hz（`BEEP_On()` 调用 `Buzzer_Beep(1000)`） |

### 11.2 蜂鸣器接口

定义在 `DOG_Master/Core/Src/led.c`，声明在 `led.h`：

```c
void BEEP_On(void);              // 以1000Hz鸣叫
void BEEP_Off(void);             // 停止
void Buzzer_Beep(uint16_t freq); // 自定义频率（Hz），0=停止
```

### 11.3 常见问题

**Q：蜂鸣器频率不对？**

检查 `Buzzer_Beep()` 中的 `tmr_clk` 是否与实际计数时钟一致：

```c
uint32_t tmr_clk = 500000;  // 500kHz = 120MHz / (Prescaler+1)
```

若修改了 TIM17 的 Prescaler，需同步更新此值，否则实际频率会偏差。

### 11.1 文档

- `SYSTEM_DOCUMENTATION.md` - 完整系统文档
- `README_SLAVE.md` - 从控制器说明
- 代码注释

### 11.2 调试工具

- STM32CubeIDE调试器
- 逻辑分析仪（SPI/CAN）
- CAN分析仪
- USB分析仪

### 11.3 社区资源

- STM32官方论坛
- GitHub Issues
- 电机厂商技术支持

## 12. 非阻塞延迟工具（tim_delay）

### 12.1 背景

主循环中禁止使用 `HAL_Delay()`，否则会阻塞 USB 接收、SPI 发送等所有任务。`tim_delay` 模块提供两种延迟方式：

| 函数 | 用途 |
|------|------|
| `Tim_Delay(ms)` | 阻塞式，仅用于初始化阶段 |
| `Tim_IsElapsed(&t, ms)` | 非阻塞式，主循环中定时触发 |
| `Tim_GetMs()` | 获取当前毫秒时间戳 |

底层使用 SysTick 驱动的 `HAL_GetTick()`（1ms 精度），不占用额外硬件定时器。

### 12.2 主循环用法

```c
#include "tim_delay.h"

// while(1) 中
static uint32_t t1 = 0;
static uint32_t t2 = 0;

if (Tim_IsElapsed(&t1, 10)) {
    // 每10ms执行一次
}

if (Tim_IsElapsed(&t2, 500)) {
    // 每500ms执行一次
}
```

`Tim_IsElapsed()` 内部使用无符号减法，`HAL_GetTick()` 约49.7天溢出回绕时也能正确处理。

## 13. 指示灯说明

### 13.1 LED功能总览

主控板共8个LED，功能如下：

| LED | 引脚 | 功能 | 行为 |
|-----|------|------|------|
| LED1 | PB6 | 系统心跳 | 1Hz 闪烁，系统正常运行时持续闪烁 |
| LED2 | PB7 | USB数据接收 | 每收到一包上位机数据亮 50ms |
| LED3 | PB8 | CAN发送 | 每向电机1-3发送成功一次亮 30ms |
| LED4 | PE0 | SPI发送 | 每向从控发送成功一次亮 30ms |
| LED5 | PE1 | 电机1-3掉线 | 电机1、2、3中任意一个掉线时常亮 |
| LED6 | PE2 | 电机4-6掉线 | 电机4、5、6中任意一个掉线时常亮 |
| LED7 | PE3 | 电机7-9掉线 | 电机7、8、9中任意一个掉线时常亮 |
| LED8 | PE4 | 电机10-12掉线 | 电机10、11、12中任意一个掉线时常亮 |

### 13.2 上电自检流程

系统上电后会依次点亮 LED1→LED2→...→LED8（每个亮200ms），然后蜂鸣器短鸣一声，最后所有LED同时亮500ms后熄灭。若某个LED在自检中不亮，说明该LED或其GPIO存在硬件问题。

### 13.3 正常工作状态

系统正常运行时，预期观察到：

- **LED1**：稳定1Hz闪烁
- **LED2**：上位机发送控制指令时快速闪烁（频率取决于上位机发包频率）
- **LED3**：与LED2同步闪烁（CAN发送与USB接收联动）
- **LED4**：与LED2同步闪烁（SPI发送与USB接收联动）
- **LED5~LED8**：全部熄灭

### 13.4 故障诊断

| 现象 | 可能原因 |
|------|----------|
| LED1不闪烁 | 主循环卡死，检查是否有阻塞调用（如 `HAL_Delay`） |
| LED2不亮 | USB未连接，或上位机未发送数据 |
| LED3亮但LED2不亮 | 不可能，LED3由USB数据触发CAN发送驱动 |
| LED4不亮但LED2亮 | SPI通信异常，检查主从控连线 |
| LED5~LED8任意常亮 | 对应组电机掉线，同时蜂鸣器间歇报警 |
| LED5~LED8全部常亮 | 所有电机掉线，检查CAN总线和电机供电 |

### 13.5 相关代码位置

| 功能 | 文件 |
|------|------|
| LED驱动（GPIO控制） | `DOG_Master/Core/Src/led.c` |
| LED指示逻辑（闪烁处理） | `DOG_Master/Core/Src/Led_indicator.c` |
| USB接收触发LED2 | `DOG_Master/Core/Src/usbcol.c` → `Protocol_ProcessRxData()` |
| CAN发送触发LED3 | `DOG_Master/Core/Src/system_control.c` → `HAL_TIM_PeriodElapsedCallback()` |
| SPI发送触发LED4 | `DOG_Master/Core/Src/system_control.c` → `System_ProcessMotorControl()` |
| 掉线检测控制LED5~LED8 | `DOG_Master/Core/Src/system_control.c` → `System_CheckMotorOffline()` |

## 14. 技术支持

### 14.1 文档

- `SYSTEM_DOCUMENTATION.md` - 完整系统文档
- `README_SLAVE.md` - 从控制器说明
- 代码注释

### 14.2 调试工具

- STM32CubeIDE调试器
- 逻辑分析仪（SPI/CAN）
- CAN分析仪
- USB分析仪

### 14.3 社区资源

- STM32官方论坛
- GitHub Issues
- 电机厂商技术支持

## 15. 下一步

完成基础测试后，可以：

1. 实现更复杂的运动控制算法
2. 添加传感器反馈（IMU、力传感器）
3. 集成到机器人控制系统
4. 开发图形化控制界面
5. 实现轨迹规划和插补

---

**祝你使用愉快！**

如有问题，请参考完整技术文档或联系技术支持。
