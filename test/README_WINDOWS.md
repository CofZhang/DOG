# Windows 平台电机控制使用教程

## 概述

本教程说明如何在 Windows 系统上通过 USB 控制 12 个电机全部旋转 30 度。

## 环境准备

### 1. 安装 Python

从 https://www.python.org/downloads/ 下载并安装 Python 3.8 或更高版本。

安装时请勾选 "Add Python to PATH" 选项。

### 2. 安装依赖库

打开命令提示符（CMD）或 PowerShell，运行以下命令：

```bash
pip install pyserial
```

## 使用步骤

### 1. 连接设备

将 USB 设备连接到电脑。

### 2. 运行控制程序

在命令提示符中进入 test 目录：

```bash
cd c:\Users\roman\Desktop\DOG\test
```

运行控制程序：

```bash
python control_12motors_windows.py
```

### 3. 选择串口

程序会自动列出所有可用串口，选择对应设备的串口即可。

## 程序功能说明

程序执行以下三个步骤：

1. **第一步**：将所有电机设置到 0 度位置（基准位置）
2. **第二步**：将所有电机旋转到 30 度位置
3. **第三步**：将所有电机回到 0 度位置

## 自定义参数

如果需要修改控制参数，可以编辑 `control_12motors_windows.py` 文件：

- 修改 `kp` 参数（0~5，位置增益）
- 修改 `kd` 参数（0~50，微分增益）
- 修改目标角度
- 修改延时时间

## 文件说明

- `motor_protocol.py` - 电机协议实现（通用模块）
- `control_12motors_windows.py` - Windows 控制程序
- `README_WINDOWS.md` - 本教程

## 故障排除

### 问题：找不到串口

- 检查 USB 连接是否正常
- 确认设备驱动已正确安装
- 尝试更换 USB 端口

### 问题：电机无响应

- 检查电机电源是否正常
- 确认波特率设置正确（默认 115200）
- 查看设备管理器中的端口状态

