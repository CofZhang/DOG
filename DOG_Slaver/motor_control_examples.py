#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
电机控制示例 - 正弦波位置控制
演示如何使用motor_controller.py控制12个电机进行正弦波运动
"""

import sys
import time
import math
from motor_controller import MotorController, MotorCommand


def sine_wave_control(controller: MotorController, duration: float = 10.0, frequency: float = 0.5):
    """
    正弦波位置控制

    Args:
        controller: 电机控制器实例
        duration: 运行时长（秒）
        frequency: 正弦波频率（Hz）
    """
    print(f"\n开始正弦波控制...")
    print(f"运行时长: {duration}秒")
    print(f"频率: {frequency}Hz")
    print(f"幅度: ±30度 (±0.524 rad)")
    print("按Ctrl+C提前停止\n")

    start_time = time.time()
    count = 0

    try:
        while (time.time() - start_time) < duration:
            # 计算当前时间
            t = time.time() - start_time

            # 生成正弦波位置（幅度±30度 = ±0.524 rad）
            amplitude = 0.524  # 30度
            position = amplitude * math.sin(2 * math.pi * frequency * t)

            # 创建12个电机指令（所有电机同步运动）
            commands = []
            for i in range(12):
                cmd = MotorCommand(
                    kp=2.0,         # 较高的KP获得更好的位置跟踪
                    kd=0.5,         # 适当的KD提供阻尼
                    position=position,
                    velocity=0.0,
                    torque=0.0
                )
                commands.append(cmd)

            # 发送控制指令
            if controller.send_motor_commands(commands):
                count += 1

                # 接收反馈
                feedbacks = controller.receive_motor_feedback()

                # 每50次打印一次状态
                if feedbacks and count % 50 == 0:
                    print(f"时间: {t:.2f}s, 目标位置: {position:.3f} rad, "
                          f"电机1实际位置: {feedbacks[0].position:.3f} rad, "
                          f"误差: {abs(position - feedbacks[0].position):.3f} rad")

            # 控制频率1kHz
            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n用户中断")

    print(f"\n控制结束，共发送 {count} 次指令")


def step_response_test(controller: MotorController, target_position: float = 0.5):
    """
    阶跃响应测试

    Args:
        controller: 电机控制器实例
        target_position: 目标位置（rad）
    """
    print(f"\n开始阶跃响应测试...")
    print(f"目标位置: {target_position} rad")
    print("测试时长: 2秒\n")

    start_time = time.time()
    positions = []
    times = []

    try:
        while (time.time() - start_time) < 2.0:
            t = time.time() - start_time

            # 创建阶跃指令
            commands = []
            for i in range(12):
                cmd = MotorCommand(
                    kp=2.0,
                    kd=0.5,
                    position=target_position,
                    velocity=0.0,
                    torque=0.0
                )
                commands.append(cmd)

            # 发送控制指令
            if controller.send_motor_commands(commands):
                # 接收反馈
                feedbacks = controller.receive_motor_feedback()

                if feedbacks:
                    # 记录电机1的位置
                    positions.append(feedbacks[0].position)
                    times.append(t)

                    # 打印状态
                    if len(positions) % 100 == 0:
                        print(f"时间: {t:.3f}s, 位置: {feedbacks[0].position:.3f} rad, "
                              f"误差: {abs(target_position - feedbacks[0].position):.3f} rad")

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n用户中断")

    # 分析结果
    if positions:
        print("\n阶跃响应分析:")
        print(f"最终位置: {positions[-1]:.3f} rad")
        print(f"稳态误差: {abs(target_position - positions[-1]):.3f} rad")

        # 计算上升时间（达到90%目标值的时间）
        target_90 = target_position * 0.9
        for i, pos in enumerate(positions):
            if pos >= target_90:
                print(f"上升时间(90%): {times[i]:.3f}s")
                break


def individual_motor_control(controller: MotorController):
    """
    单独控制每个电机

    Args:
        controller: 电机控制器实例
    """
    print("\n开始单独电机控制测试...")
    print("每个电机将依次移动到不同位置")
    print("测试时长: 12秒\n")

    start_time = time.time()

    try:
        while (time.time() - start_time) < 12.0:
            t = time.time() - start_time

            # 创建12个不同的指令
            commands = []
            for i in range(12):
                # 每个电机的相位不同
                phase = (i / 12.0) * 2 * math.pi
                position = 0.3 * math.sin(2 * math.pi * 0.5 * t + phase)

                cmd = MotorCommand(
                    kp=2.0,
                    kd=0.5,
                    position=position,
                    velocity=0.0,
                    torque=0.0
                )
                commands.append(cmd)

            # 发送控制指令
            if controller.send_motor_commands(commands):
                # 接收反馈
                feedbacks = controller.receive_motor_feedback()

                # 每100次打印一次状态
                if feedbacks and int(t * 1000) % 100 == 0:
                    print(f"时间: {t:.2f}s")
                    for i, fb in enumerate(feedbacks[:3], 1):  # 只打印前3个电机
                        print(f"  电机{i}: 位置={fb.position:.3f} rad, "
                              f"速度={fb.velocity:.3f} rad/s, "
                              f"电流={fb.current:.2f} A")

            time.sleep(0.001)

    except KeyboardInterrupt:
        print("\n用户中断")


def main():
    """主程序"""
    if len(sys.argv) < 2:
        print("用法: python motor_control_examples.py <串口设备> [测试模式]")
        print("\n测试模式:")
        print("  1 - 正弦波控制（默认）")
        print("  2 - 阶跃响应测试")
        print("  3 - 单独电机控制")
        print("\n示例:")
        print("  python motor_control_examples.py /dev/ttyACM0 1")
        print("  python motor_control_examples.py COM3 2")
        sys.exit(1)

    port = sys.argv[1]
    test_mode = int(sys.argv[2]) if len(sys.argv) > 2 else 1

    # 创建控制器
    controller = MotorController(port)

    # 连接设备
    if not controller.connect():
        sys.exit(1)

    try:
        # 先停止所有电机
        print("初始化：停止所有电机...")
        controller.stop_all_motors()
        time.sleep(0.5)

        # 根据测试模式执行不同的测试
        if test_mode == 1:
            sine_wave_control(controller, duration=10.0, frequency=0.5)
        elif test_mode == 2:
            step_response_test(controller, target_position=0.5)
        elif test_mode == 3:
            individual_motor_control(controller)
        else:
            print(f"未知的测试模式: {test_mode}")

    except Exception as e:
        print(f"\n错误: {e}")

    finally:
        # 停止所有电机
        print("\n停止所有电机...")
        controller.stop_all_motors()
        time.sleep(0.1)

        # 断开连接
        controller.disconnect()
        print("程序结束")


if __name__ == "__main__":
    main()
