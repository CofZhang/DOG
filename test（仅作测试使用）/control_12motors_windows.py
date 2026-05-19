#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Windows 平台电机控制程序
控制 12 个电机全部旋转 30 度
基于 PROTOCOL.md 通讯协议文档
"""

import time
import serial
import serial.tools.list_ports
from motor_protocol import USBControlPkg, unpack_feedback_pkg, rad_to_deg


def find_usb_device():
    ports = serial.tools.list_ports.comports()
    available_ports = []
    
    print("可用串口列表:")
    for i, port in enumerate(ports, 1):
        print(f"  [{i}] {port.device} - {port.description}")
        available_ports.append(port.device)
    
    if not available_ports:
        print("未找到可用串口！")
        return None
    
    if len(available_ports) == 1:
        print(f"自动选择: {available_ports[0]}")
        return available_ports[0]
    
    while True:
        try:
            choice = int(input("请选择串口编号: ")) - 1
            if 0 <= choice < len(available_ports):
                return available_ports[choice]
        except ValueError:
            pass
        print("无效选择，请重试")


def receive_feedback(ser, timeout=2.0):
    ser.timeout = timeout
    data = ser.read(164)
    if len(data) == 164 and data[0] == 0xAA and data[111] == 0x55:
        feedbacks = unpack_feedback_pkg(data)
        if feedbacks:
            return feedbacks
    return None


def main():
    print("=" * 60)
    print("  12 电机控制程序 - Windows 版本")
    print("  控制所有电机旋转 30 度")
    print("=" * 60)
    
    port_name = find_usb_device()
    if not port_name:
        return
    
    try:
        ser = serial.Serial(
            port=port_name,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=1.0
        )
        print(f"\n成功打开串口: {port_name}")
        print("注：CDC虚拟串口，波特率设置不影响实际速率\n")
    except Exception as e:
        print(f"打开串口失败: {e}")
        return
    
    try:
        pkg = USBControlPkg()
        
        print("[1/3] 设置所有电机到 0 度位置...")
        pkg.set_all_motors_position_deg(0.0, kp=1.0, kd=10.0)
        pkg.sequence = 1
        data = pkg.pack()
        ser.write(data)
        print(f"  发送数据包，长度: {len(data)} 字节")
        time.sleep(1.0)
        
        fb = receive_feedback(ser)
        if fb:
            print(f"  收到反馈 - 电机1位置: {rad_to_deg(fb[0].position):.2f}°")
        
        print("\n[2/3] 设置所有电机到 30 度位置...")
        pkg.set_all_motors_position_deg(30.0, kp=1.0, kd=10.0)
        pkg.sequence = 2
        data = pkg.pack()
        ser.write(data)
        print(f"  发送数据包，长度: {len(data)} 字节")
        time.sleep(2.0)
        
        fb = receive_feedback(ser)
        if fb:
            print(f"  收到反馈 - 电机1位置: {rad_to_deg(fb[0].position):.2f}°")
        
        print("\n[3/3] 设置所有电机回到 0 度位置...")
        pkg.set_all_motors_position_deg(0.0, kp=1.0, kd=10.0)
        pkg.sequence = 3
        data = pkg.pack()
        ser.write(data)
        print(f"  发送数据包，长度: {len(data)} 字节")
        time.sleep(1.0)
        
        fb = receive_feedback(ser)
        if fb:
            print(f"  收到反馈 - 电机1位置: {rad_to_deg(fb[0].position):.2f}°")
        
        print("\n" + "=" * 60)
        print("  控制完成！")
        print("=" * 60)
        
    except KeyboardInterrupt:
        print("\n\n用户中断程序")
    except Exception as e:
        print(f"\n发生错误: {e}")
    finally:
        if ser.is_open:
            ser.close()
            print("串口已关闭")


if __name__ == "__main__":
    main()
