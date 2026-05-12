#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
电机控制器Python SDK
用于通过USB CDC控制12路电机
"""

import serial
import struct
import time
from typing import List, Tuple
from dataclasses import dataclass


@dataclass
class MotorCommand:
    """电机控制指令"""
    kp: float = 0.0         # KP参数 (0~5)
    kd: float = 0.0         # KD参数 (0~50)
    position: float = 0.0   # 期望位置 (-60~+60 rad)
    velocity: float = 0.0   # 期望速度 (-60~+60 rad/s)
    torque: float = 0.0     # 前馈扭矩 (-60~+60 Nm)


@dataclass
class MotorFeedback:
    """电机反馈数据"""
    error_code: int = 0     # 错误码
    position: float = 0.0   # 实际位置 (rad)
    velocity: float = 0.0   # 实际速度 (rad/s)
    current: float = 0.0    # 实际电流 (A)
    coil_temp: int = 0      # 线圈温度 (℃)
    mos_temp: int = 0       # MOS温度 (℃)


class MotorController:
    """电机控制器类"""

    # 协议常量
    USB_PACKET_SIZE = 112
    HEADER = 0xAA
    FOOTER = 0x55
    CMD_TYPE_MOTOR_CTRL = 0x10

    # 错误码定义
    ERROR_CODES = {
        0: "正常",
        1: "过热",
        2: "过流",
        3: "电压过高",
        4: "电压过低",
        5: "编码器错误",
        6: "刹车电压异常",
        7: "DRV驱动错误"
    }

    def __init__(self, port: str, baudrate: int = 115200):
        """
        初始化电机控制器

        Args:
            port: 串口设备名（如 '/dev/ttyACM0' 或 'COM3'）
            baudrate: 波特率（USB CDC通常忽略此参数）
        """
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.sequence = 0

    def connect(self):
        """连接到设备"""
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0
            )
            print(f"已连接到 {self.port}")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False

    def disconnect(self):
        """断开连接"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            print("已断开连接")

    @staticmethod
    def _clamp(value: float, min_val: float, max_val: float) -> float:
        """限制数值范围"""
        return max(min_val, min(max_val, value))

    @staticmethod
    def _pack_motor_command(cmd: MotorCommand) -> bytes:
        """
        打包电机控制指令为8字节CAN数据

        Args:
            cmd: 电机控制指令

        Returns:
            8字节CAN数据
        """
        # 限制参数范围
        kp = MotorController._clamp(cmd.kp, 0.0, 5.0)
        kd = MotorController._clamp(cmd.kd, 0.0, 50.0)
        pos = MotorController._clamp(cmd.position, -60.0, 60.0)
        vel = MotorController._clamp(cmd.velocity, -60.0, 60.0)
        torque = MotorController._clamp(cmd.torque, -60.0, 60.0)

        # 转换为CAN原始值
        kp_raw = int(kp * 819.0)
        kd_raw = int(kd / 0.0978)
        pos_raw = int((pos + 60.0) * 546.125)
        vel_raw = int((vel + 60.0) * 34.125)
        torque_raw = int((torque + 60.0) * 34.125)

        # 位域打包
        data = 0
        data |= (0x00 & 0x07) << 61          # 模式 (3bit)
        data |= (kp_raw & 0xFFF) << 49       # KP (12bit)
        data |= (kd_raw & 0x1FF) << 40       # KD (9bit)
        data |= (pos_raw & 0xFFFF) << 24     # 位置 (16bit)
        data |= (vel_raw & 0xFFF) << 12      # 速度 (12bit)
        data |= (torque_raw & 0xFFF)         # 扭矩 (12bit)

        # 大端序输出
        return struct.pack('>Q', data)

    @staticmethod
    def _unpack_motor_feedback(data: bytes) -> MotorFeedback:
        """
        解析8字节CAN反馈数据

        Args:
            data: 8字节CAN反馈数据

        Returns:
            电机反馈数据
        """
        # 大端序解析
        value = struct.unpack('>Q', data)[0]

        # 提取各字段
        error_code = (value >> 56) & 0x1F
        pos_raw = (value >> 40) & 0xFFFF
        vel_raw = (value >> 28) & 0xFFF
        current_raw = (value >> 16) & 0xFFF
        coil_temp_raw = (value >> 8) & 0xFF
        mos_temp_raw = value & 0xFF

        # 转换为物理值
        feedback = MotorFeedback()
        feedback.error_code = error_code
        feedback.position = (pos_raw / 546.125) - 60.0
        feedback.velocity = (vel_raw / 34.125) - 60.0
        feedback.current = (current_raw / 4095.0) * 124.0 - 62.0
        feedback.coil_temp = (coil_temp_raw - 50) // 2
        feedback.mos_temp = (mos_temp_raw - 50) // 2

        return feedback

    @staticmethod
    def _calculate_checksum(data: bytes) -> int:
        """计算XOR校验和"""
        checksum = 0
        for byte in data:
            checksum ^= byte
        return checksum

    def send_motor_commands(self, commands: List[MotorCommand]) -> bool:
        """
        发送12个电机控制指令

        Args:
            commands: 12个电机控制指令列表

        Returns:
            发送是否成功
        """
        if not self.serial or not self.serial.is_open:
            print("设备未连接")
            return False

        if len(commands) != 12:
            print("必须提供12个电机指令")
            return False

        # 构建数据包
        packet = bytearray(self.USB_PACKET_SIZE)

        # 帧头
        packet[0] = self.HEADER
        packet[1] = self.CMD_TYPE_MOTOR_CTRL
        packet[2] = 0x00  # Length高字节
        packet[3] = 0x60  # Length低字节 (96字节电机数据)
        packet[4] = self.sequence
        packet[5] = 0x00  # Reserved

        # 打包12个电机数据
        for i, cmd in enumerate(commands):
            motor_data = self._pack_motor_command(cmd)
            offset = 6 + i * 8
            packet[offset:offset+8] = motor_data

        # 保留字节
        for i in range(102, 110):
            packet[i] = 0x00

        # 校验和
        packet[110] = self._calculate_checksum(packet[0:110])

        # 帧尾
        packet[111] = self.FOOTER

        # 发送数据
        try:
            self.serial.write(packet)
            self.sequence = (self.sequence + 1) & 0xFF
            return True
        except Exception as e:
            print(f"发送失败: {e}")
            return False

    def receive_motor_feedback(self, timeout: float = 0.1) -> List[MotorFeedback]:
        """
        接收12个电机反馈数据

        Args:
            timeout: 超时时间（秒）

        Returns:
            12个电机反馈数据列表，失败返回None
        """
        if not self.serial or not self.serial.is_open:
            print("设备未连接")
            return None

        # 设置超时
        old_timeout = self.serial.timeout
        self.serial.timeout = timeout

        try:
            # 读取数据包
            data = self.serial.read(self.USB_PACKET_SIZE)

            if len(data) != self.USB_PACKET_SIZE:
                return None

            # 验证帧头和帧尾
            if data[0] != self.HEADER or data[111] != self.FOOTER:
                print("数据包格式错误")
                return None

            # 验证校验和
            checksum = self._calculate_checksum(data[0:110])
            if checksum != data[110]:
                print("校验和错误")
                return None

            # 解析12个电机反馈
            feedbacks = []
            for i in range(12):
                offset = 6 + i * 8
                motor_data = data[offset:offset+8]
                feedback = self._unpack_motor_feedback(motor_data)
                feedbacks.append(feedback)

            return feedbacks

        except Exception as e:
            print(f"接收失败: {e}")
            return None
        finally:
            self.serial.timeout = old_timeout

    def stop_all_motors(self):
        """停止所有电机"""
        zero_cmd = MotorCommand()
        commands = [zero_cmd] * 12
        return self.send_motor_commands(commands)

    def print_feedback(self, feedbacks: List[MotorFeedback]):
        """打印电机反馈信息"""
        if not feedbacks:
            return

        print("\n" + "="*80)
        print(f"{'电机':<6} {'状态':<10} {'位置(rad)':<12} {'速度(rad/s)':<14} "
              f"{'电流(A)':<10} {'线圈温度(℃)':<12} {'MOS温度(℃)':<12}")
        print("="*80)

        for i, fb in enumerate(feedbacks, 1):
            status = self.ERROR_CODES.get(fb.error_code, f"未知({fb.error_code})")
            print(f"电机{i:<3} {status:<10} {fb.position:>10.3f}  {fb.velocity:>12.3f}  "
                  f"{fb.current:>8.2f}  {fb.coil_temp:>10}  {fb.mos_temp:>10}")
        print("="*80 + "\n")


def main():
    """示例程序"""
    import sys

    # 检查命令行参数
    if len(sys.argv) < 2:
        print("用法: python motor_controller.py <串口设备>")
        print("示例: python motor_controller.py /dev/ttyACM0")
        print("      python motor_controller.py COM3")
        sys.exit(1)

    port = sys.argv[1]

    # 创建控制器
    controller = MotorController(port)

    # 连接设备
    if not controller.connect():
        sys.exit(1)

    try:
        print("\n开始电机控制测试...")
        print("按Ctrl+C停止\n")

        # 创建12个电机指令
        commands = []
        for i in range(12):
            cmd = MotorCommand(
                kp=1.0,
                kd=0.5,
                position=0.0,
                velocity=0.0,
                torque=0.0
            )
            commands.append(cmd)

        # 控制循环
        count = 0
        while True:
            # 发送控制指令
            if controller.send_motor_commands(commands):
                count += 1

                # 接收反馈
                feedbacks = controller.receive_motor_feedback()

                if feedbacks:
                    # 每10次打印一次反馈
                    if count % 10 == 0:
                        controller.print_feedback(feedbacks)
                        print(f"已发送 {count} 次指令")

            # 控制频率约100Hz
            time.sleep(0.01)

    except KeyboardInterrupt:
        print("\n\n正在停止所有电机...")
        controller.stop_all_motors()
        time.sleep(0.1)

    finally:
        controller.disconnect()
        print("程序结束")


if __name__ == "__main__":
    main()
