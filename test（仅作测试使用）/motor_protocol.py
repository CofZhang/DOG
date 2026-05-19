#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
电机控制协议实现
基于 PROTOCOL.md 通讯协议文档
"""

import struct
import math

# ==================== 常量定义 ====================
USB_PKG_HEADER = 0xAA
USB_PKG_FOOTER = 0x55
USB_PKG_TOTAL_LEN = 164
USB_PKG_HEADER_LEN = 6
USB_PKG_DATA_LEN = 96
USB_PKG_MOTOR_CNT = 12
USB_PKG_MOTOR_DATA_LEN = 8
USB_PKG_RESERVED2_LEN = 8
USB_PKG_CHECKSUM_POS = 110
USB_PKG_FOOTER_POS = 111

CMD_TYPE_CONTROL = 0x10
CMD_TYPE_CONFIG = 0x11
CMD_TYPE_QUERY = 0x12

MOTOR_MODE_FORCE_POSITION = 0x00

KP_MIN = 0.0
KP_MAX = 5.0
KD_MIN = 0.0
KD_MAX = 50.0
POS_MIN = -60.0
POS_MAX = 60.0
VEL_MIN = -60.0
VEL_MAX = 60.0
TORQUE_MIN = -60.0
TORQUE_MAX = 60.0

# 电机ID配置
MOTOR_IDS = [0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C]


# ==================== 物理值与CAN原始值换算 ====================
def clamp(value, min_val, max_val):
    return max(min_val, min(value, max_val))


def motor_kp_phys_to_raw(kp_phys):
    kp_phys = clamp(kp_phys, KP_MIN, KP_MAX)
    raw = int(kp_phys * 819.0 + 0.5)
    return min(raw, 4095) & 0xFFF


def motor_kp_raw_to_phys(kp_raw):
    kp_raw = kp_raw & 0xFFF
    return kp_raw / 819.0


def motor_kd_phys_to_raw(kd_phys):
    kd_phys = clamp(kd_phys, KD_MIN, KD_MAX)
    raw = int(kd_phys / 0.0978 + 0.5)
    return min(raw, 511) & 0x1FF


def motor_kd_raw_to_phys(kd_raw):
    kd_raw = kd_raw & 0x1FF
    return kd_raw * 0.0978


def motor_pos_phys_to_raw(pos_phys):
    pos_phys = clamp(pos_phys, POS_MIN, POS_MAX)
    raw = int((pos_phys + 60.0) * 546.125 + 0.5)
    return min(raw, 65535) & 0xFFFF


def motor_pos_raw_to_phys(pos_raw):
    pos_raw = pos_raw & 0xFFFF
    return pos_raw / 546.125 - 60.0


def motor_vel_phys_to_raw(vel_phys):
    vel_phys = clamp(vel_phys, VEL_MIN, VEL_MAX)
    raw = int((vel_phys + 60.0) * 34.125 + 0.5)
    return min(raw, 4095) & 0xFFF


def motor_vel_raw_to_phys(vel_raw):
    vel_raw = vel_raw & 0xFFF
    return vel_raw / 34.125 - 60.0


def motor_torque_phys_to_raw(torque_phys):
    torque_phys = clamp(torque_phys, TORQUE_MIN, TORQUE_MAX)
    raw = int((torque_phys + 60.0) * 34.125 + 0.5)
    return min(raw, 4095) & 0xFFF


def motor_torque_raw_to_phys(torque_raw):
    torque_raw = torque_raw & 0xFFF
    return torque_raw / 34.125 - 60.0


def deg_to_rad(deg):
    return deg * math.pi / 180.0


def rad_to_deg(rad):
    return rad * 180.0 / math.pi


# ==================== 电机控制参数结构体 ====================
class MotorControlParam:
    def __init__(self, kp=1.0, kd=10.0, position=0.0, velocity=0.0, torque=0.0):
        self.kp = kp
        self.kd = kd
        self.position = position
        self.velocity = velocity
        self.torque = torque


# ==================== 电机控制数据打包（8字节Big-Endian）====================
def motor_pack_control_data(param):
    bits = 0
    bits |= ((MOTOR_MODE_FORCE_POSITION & 0x07) << 61)
    kp_raw = motor_kp_phys_to_raw(param.kp)
    bits |= ((kp_raw & 0xFFF) << 49)
    kd_raw = motor_kd_phys_to_raw(param.kd)
    bits |= ((kd_raw & 0x1FF) << 40)
    pos_raw = motor_pos_phys_to_raw(param.position)
    bits |= ((pos_raw & 0xFFFF) << 24)
    vel_raw = motor_vel_phys_to_raw(param.velocity)
    bits |= ((vel_raw & 0xFFF) << 12)
    torque_raw = motor_torque_phys_to_raw(param.torque)
    bits |= ((torque_raw & 0xFFF) << 0)
    
    data = struct.pack('>Q', bits)
    return data


# ==================== USB控制数据包打包 ====================
class USBControlPkg:
    def __init__(self):
        self.header = USB_PKG_HEADER
        self.cmd_type = CMD_TYPE_CONTROL
        self.length = USB_PKG_DATA_LEN
        self.sequence = 0
        self.reserved = 0
        self.motors = [MotorControlParam() for _ in range(12)]
        self.reserved2 = bytearray(8)
        self.checksum = 0
        self.footer = USB_PKG_FOOTER
    
    def pack(self):
        data = bytearray(USB_PKG_TOTAL_LEN)
        data[0] = self.header
        data[1] = self.cmd_type
        data[2] = (self.length >> 8) & 0xFF
        data[3] = self.length & 0xFF
        data[4] = self.sequence & 0xFF
        data[5] = self.reserved
        
        for i in range(12):
            motor_data = motor_pack_control_data(self.motors[i])
            offset = 6 + i * 8
            data[offset:offset+8] = motor_data
        
        for i in range(8):
            data[102 + i] = self.reserved2[i]
        
        checksum = 0
        for i in range(110):
            checksum ^= data[i]
        self.checksum = checksum & 0xFF
        data[110] = self.checksum
        data[111] = self.footer
        
        return data
    
    def set_motor_position_deg(self, motor_index, deg, kp=1.0, kd=10.0):
        rad = deg_to_rad(deg)
        if 0 <= motor_index < 12:
            self.motors[motor_index].kp = kp
            self.motors[motor_index].kd = kd
            self.motors[motor_index].position = rad
            self.motors[motor_index].velocity = 0.0
            self.motors[motor_index].torque = 0.0
    
    def set_all_motors_position_deg(self, deg, kp=1.0, kd=10.0):
        for i in range(12):
            self.set_motor_position_deg(i, deg, kp, kd)


# ==================== 电机反馈数据解包 ====================
class MotorFeedback:
    def __init__(self):
        self.error_code = 0
        self.position = 0.0
        self.velocity = 0.0
        self.current = 0.0
        self.coil_temp = 0.0
        self.mos_temp = 0.0


def motor_unpack_feedback_data(can_data):
    feedback = MotorFeedback()
    bits = struct.unpack('>Q', can_data)[0]
    
    feedback.error_code = (bits >> 56) & 0x1F
    
    pos_raw = (bits >> 40) & 0xFFFF
    feedback.position = motor_pos_raw_to_phys(pos_raw)
    
    vel_raw = (bits >> 28) & 0xFFF
    feedback.velocity = motor_vel_raw_to_phys(vel_raw)
    
    current_raw = (bits >> 16) & 0xFFF
    feedback.current = (current_raw / 4095.0) * 124.0 - 62.0
    
    coil_temp_raw = (bits >> 8) & 0xFF
    feedback.coil_temp = (coil_temp_raw - 50) / 2.0
    
    mos_temp_raw = bits & 0xFF
    feedback.mos_temp = (mos_temp_raw - 50) / 2.0
    
    return feedback


def unpack_feedback_pkg(data):
    if len(data) < 112:
        return None
    if data[0] != USB_PKG_HEADER or data[111] != USB_PKG_FOOTER:
        return None
    
    feedbacks = []
    for i in range(12):
        motor_data = data[6 + i*8 : 6 + i*8 + 8]
        feedbacks.append(motor_unpack_feedback_data(motor_data))
    
    return feedbacks
