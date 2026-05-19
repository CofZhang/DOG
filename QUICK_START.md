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

## 10. 技术支持

### 10.1 文档

- `SYSTEM_DOCUMENTATION.md` - 完整系统文档
- `README_SLAVE.md` - 从控制器说明
- 代码注释

### 10.2 调试工具

- STM32CubeIDE调试器
- 逻辑分析仪（SPI/CAN）
- CAN分析仪
- USB分析仪

### 10.3 社区资源

- STM32官方论坛
- GitHub Issues
- 电机厂商技术支持

## 11. 下一步

完成基础测试后，可以：

1. 实现更复杂的运动控制算法
2. 添加传感器反馈（IMU、力传感器）
3. 集成到机器人控制系统
4. 开发图形化控制界面
5. 实现轨迹规划和插补

---

**祝你使用愉快！**

如有问题，请参考完整技术文档或联系技术支持。
