# Linux 平台电机控制使用教程

## 概述

本教程说明如何在 Linux 系统上通过 USB 控制 12 个电机全部旋转 30 度。

## 环境准备

### 1. 安装 Python

大多数 Linux 发行版已预装 Python 3。如果没有，请安装：

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install python3 python3-pip
```

**Fedora/CentOS/RHEL:**
```bash
sudo dnf install python3 python3-pip
```

**Arch Linux:**
```bash
sudo pacman -S python python-pip
```

### 2. 安装依赖库

```bash
pip3 install pyserial
```

或使用系统包管理器：

**Ubuntu/Debian:**
```bash
sudo apt install python3-serial
```

## 使用步骤

### 1. 连接设备

将 USB 设备连接到电脑。

### 2. 检查串口权限

默认情况下，普通用户可能没有串口访问权限：

```bash
ls -l /dev/ttyACM*
# 或
ls -l /dev/ttyUSB*
```

如果权限不足，运行：

```bash
sudo chmod 666 /dev/ttyACM0
```

或永久添加用户到 dialout 组（推荐）：

```bash
sudo usermod -aG dialout $USER
```
*注意：需要重新登录才能生效*

### 3. 运行控制程序

在终端中进入 test 目录：

```bash
cd ~/DOG/test
```

运行控制程序：

```bash
python3 control_12motors_linux.py
```

### 4. 选择串口

程序会自动列出所有可用串口，选择对应设备的串口即可。

## 程序功能说明

程序执行以下三个步骤：

1. **第一步**：将所有电机设置到 0 度位置（基准位置）
2. **第二步**：将所有电机旋转到 30 度位置
3. **第三步**：将所有电机回到 0 度位置

## 自定义参数

如果需要修改控制参数，可以编辑 `control_12motors_linux.py` 文件：

- 修改 `kp` 参数（0~5，位置增益）
- 修改 `kd` 参数（0~50，微分增益）
- 修改目标角度
- 修改延时时间

## 文件说明

- `motor_protocol.py` - 电机协议实现（通用模块）
- `control_12motors_linux.py` - Linux 控制程序
- `README_LINUX.md` - 本教程

## 故障排除

### 问题：找不到串口

- 检查 USB 连接是否正常
- 运行 `dmesg | tail` 查看内核日志
- 检查设备是否被识别

### 问题：权限被拒绝

- 运行 `sudo chmod 666 /dev/ttyACM*`
- 或将用户添加到 dialout 组：`sudo usermod -aG dialout $USER`（需重新登录）

### 问题：电机无响应

- 检查电机电源是否正常
- 确认波特率设置正确（默认 115200）
- 检查 `dmesg` 输出查看是否有错误

