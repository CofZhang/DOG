# C 语言版本使用说明

## 概述

C 语言版本提供跨平台支持，可在 Windows 和 Linux 上编译运行。

## 编译

### Windows (MinGW 或 MSVC)

**使用 MinGW:**
```bash
gcc motor_protocol.c control_12motors.c -o control_12motors.exe -lm
```

**使用 MSVC (Visual Studio):**
```cmd
cl motor_protocol.c control_12motors.c
```

### Linux
```bash
gcc motor_protocol.c control_12motors.c -o control_12motors -lm
```

## 运行

### Windows
```cmd
control_12motors.exe COM3
```

### Linux
```bash
./control_12motors /dev/ttyACM0
```

如果不指定串口，程序会使用默认串口。

## 使用 Makefile

项目提供了简单的 Makefile：

```bash
# Linux / macOS
make

# Windows (MinGW)
mingw32-make

# 清理
make clean
```

## 文件说明

- `motor_protocol.h` - C 语言协议头文件
- `motor_protocol.c` - C 语言协议实现
- `control_12motors.c` - 主程序
- `Makefile` - 编译配置
- `README_C.md` - 本说明

## 自定义

编辑 `control_12motors.c` 文件可以：
- 修改默认串口
- 修改 `kp` 和 `kd` 参数
- 修改目标角度
- 修改延时时间

