"""
usb_packet_builder.py
将12个电机的控制参数打包为符合DOG主机USB协议的数据包，并打印输出。

USB包格式（共164字节）：
  Byte 0      : 帧头 0xAA
  Byte 1      : 命令类型 0x10
  Byte 2~3    : 数据长度大端 0x00 0x60 (96)
  Byte 4      : 序列号
  Byte 5      : 保留 0x00
  Byte 6~101  : 12个电机×8字节控制数据
  Byte 102~109: 保留 0x00×8
  Byte 110    : XOR校验和（对 Byte 0~109 异或）
  Byte 111    : 帧尾 0x55
  Byte 112~163: 填充 0x00×52（USB底层3次传输对齐用）

每个电机8字节位域（Big-Endian 64bit）：
  Bit63-61 : 模式，固定 0x00
  Bit60-49 : KP，12bit，物理值 0~500
  Bit48-40 : KD，9bit， 物理值 0~5
  Bit39-24 : 位置，16bit，物理值 -12.5~+12.5 rad
  Bit23-12 : 速度，12bit，物理值 -18~+18 rad/s
  Bit11-0  : 扭矩，12bit，物理值 -30~+30 Nm
"""

# ==================== 参数范围常量 ====================
KP_MIN,   KP_MAX   = 0.0,   500.0
KD_MIN,   KD_MAX   = 0.0,   5.0
POS_MIN,  POS_MAX  = -12.5, 12.5
VEL_MIN,  VEL_MAX  = -18.0, 18.0
TOR_MIN,  TOR_MAX  = -30.0, 30.0

# USB包协议常量
USB_PKG_HEADER         = 0xAA
USB_PKG_FOOTER         = 0x55
CMD_TYPE_CONTROL       = 0x10
USB_PKG_DATA_LEN       = 96       # 12电机×8字节
USB_PKG_CHECKSUM_LEN   = 110      # XOR覆盖 Byte 0~109
USB_PKG_CHECKSUM_OFFSET = 110
USB_PKG_FOOTER_OFFSET  = 111
USB_PKG_TOTAL_LEN      = 164


# ==================== 换算函数 ====================

def _float_to_uint(val: float, val_min: float, val_max: float, bits: int) -> int:
    """物理值 → 无符号整数原始值（与官方 float_to_uint 一致）"""
    val = max(val_min, min(val_max, val))
    return int((val - val_min) / (val_max - val_min) * ((1 << bits) - 1) + 0.5)


def _pack_motor_8bytes(kp: float, kd: float, pos: float, vel: float, tor: float) -> bytes:
    """将单个电机参数打包为8字节 Big-Endian 数据"""
    kp_raw  = _float_to_uint(kp,  KP_MIN,  KP_MAX,  12)
    kd_raw  = _float_to_uint(kd,  KD_MIN,  KD_MAX,  9)
    pos_raw = _float_to_uint(pos, POS_MIN, POS_MAX, 16)
    vel_raw = _float_to_uint(vel, VEL_MIN, VEL_MAX, 12)
    tor_raw = _float_to_uint(tor, TOR_MIN, TOR_MAX, 12)

    bits = 0
    bits |= (0x00        & 0x7)   << 61   # 模式 Bit63-61
    bits |= (kp_raw      & 0xFFF) << 49   # KP   Bit60-49
    bits |= (kd_raw      & 0x1FF) << 40   # KD   Bit48-40
    bits |= (pos_raw     & 0xFFFF)<< 24   # 位置 Bit39-24
    bits |= (vel_raw     & 0xFFF) << 12   # 速度 Bit23-12
    bits |= (tor_raw     & 0xFFF) << 0    # 扭矩 Bit11-0

    return bits.to_bytes(8, byteorder='big')


# ==================== 主接口 ====================

def build_usb_packet(motors: list, sequence: int = 0) -> bytes:
    """
    将12个电机参数打包为164字节USB控制数据包。

    参数
    ----
    motors : list of dict，长度必须为12，每个dict包含：
        {
            'kp':  float,   # 0~500
            'kd':  float,   # 0~5
            'pos': float,   # rad, -12.5~+12.5
            'vel': float,   # rad/s, -18~+18
            'tor': float,   # Nm, -30~+30
        }
    sequence : int, 0~255，包序列号

    返回
    ----
    bytes，长度164
    """
    if len(motors) != 12:
        raise ValueError(f"需要12个电机参数，实际传入 {len(motors)} 个")

    buf = bytearray(USB_PKG_TOTAL_LEN)

    # 包头（Byte 0~5）
    buf[0] = USB_PKG_HEADER
    buf[1] = CMD_TYPE_CONTROL
    buf[2] = (USB_PKG_DATA_LEN >> 8) & 0xFF   # 0x00
    buf[3] = USB_PKG_DATA_LEN & 0xFF           # 0x60
    buf[4] = sequence & 0xFF
    buf[5] = 0x00

    # 12个电机数据（Byte 6~101）
    for i, m in enumerate(motors):
        data = _pack_motor_8bytes(m['kp'], m['kd'], m['pos'], m['vel'], m['tor'])
        offset = 6 + i * 8
        buf[offset:offset + 8] = data

    # 保留字节 Byte 102~109 已为0

    # XOR校验和（Byte 0~109 → Byte 110）
    checksum = 0
    for i in range(USB_PKG_CHECKSUM_LEN):
        checksum ^= buf[i]
    buf[USB_PKG_CHECKSUM_OFFSET] = checksum

    # 帧尾（Byte 111）
    buf[USB_PKG_FOOTER_OFFSET] = USB_PKG_FOOTER

    # Byte 112~163 保持0（填充）

    return bytes(buf)


def print_usb_packet(packet: bytes):
    """打印USB数据包的十六进制内容，每行16字节，并标注关键字段"""
    print(f"USB数据包（{len(packet)} 字节）：")
    print("-" * 60)
    for i in range(0, len(packet), 16):
        chunk = packet[i:i + 16]
        hex_str = ' '.join(f'{b:02X}' for b in chunk)
        print(f"  [{i:3d}] {hex_str}")
    print("-" * 60)
    print(f"  帧头:     0x{packet[0]:02X}")
    print(f"  命令类型: 0x{packet[1]:02X}")
    print(f"  数据长度: {(packet[2]<<8)|packet[3]}")
    print(f"  序列号:   {packet[4]}")
    print(f"  校验和:   0x{packet[USB_PKG_CHECKSUM_OFFSET]:02X}")
    print(f"  帧尾:     0x{packet[USB_PKG_FOOTER_OFFSET]:02X}")


# ==================== 交互式输入 ====================

def _input_float(prompt: str, val_min: float, val_max: float) -> float:
    """带范围校验的浮点数输入，输入非法时重新提示"""
    while True:
        try:
            val = float(input(prompt))
            if val < val_min or val > val_max:
                print(f"  超出范围 [{val_min}, {val_max}]，请重新输入")
                continue
            return val
        except ValueError:
            print("  请输入有效的数字")


def input_motors_interactive() -> list:
    """交互式逐个输入12个电机参数，返回 motors 列表"""
    motors = []
    print("=" * 60)
    print("请依次输入12个电机的控制参数")
    print("=" * 60)
    for i in range(12):
        print(f"\n--- 电机 {i+1} ---") 
        kp  = _input_float(f"  KP  (0 ~ 500):          ", KP_MIN,  KP_MAX)
        kd  = _input_float(f"  KD  (0 ~ 5):             ", KD_MIN,  KD_MAX)
        pos = _input_float(f"  位置 (-12.5 ~ 12.5 rad): ", POS_MIN, POS_MAX)
        vel = _input_float(f"  速度 (-18 ~ 18 rad/s):   ", VEL_MIN, VEL_MAX)
        tor = _input_float(f"  扭矩 (-30 ~ 30 Nm):      ", TOR_MIN, TOR_MAX)
        motors.append({'kp': kp, 'kd': kd, 'pos': pos, 'vel': vel, 'tor': tor})
    return motors


# ==================== 入口 ====================

if __name__ == '__main__':
    motors = input_motors_interactive()
    pkt = build_usb_packet(motors, sequence=0)
    print()
    print_usb_packet(pkt)
    print()
    print("完整数据包（可直接复制）：")
    print(' '.join(f'{b:02X}' for b in pkt))
