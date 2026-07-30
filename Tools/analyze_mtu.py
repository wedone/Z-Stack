"""分析 interview 抓包中 0x703F 的通信模式，计算 MTU 和帧大小"""
import struct
from collections import Counter

with open('D:/VC/Z-Stack/cap/7米进行interviwe.pcap', 'rb') as f:
    f.read(24)
    pkts = []
    for i in range(400):
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
        data = f.read(caplen)
        pkts.append((i, data, caplen))

TARGET = 0x703F

frames_703f = []
for i, data, caplen in pkts:
    if len(data) < 11: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 1: continue

    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01
    pos = 3
    dst_short = src_short = None
    try:
        if dam >= 2:
            pos += 2
            if dam == 2: dst_short = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif dam == 3: pos += 8
        if sam >= 2:
            if not pan_comp: pos += 2
            if sam == 2: src_short = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif sam == 3: pos += 8
    except: continue

    if dst_short is None or src_short is None: continue
    if dst_short != TARGET and src_short != TARGET: continue

    if pos + 8 > len(data): continue
    nwk_fc = struct.unpack('<H', data[pos:pos+2])[0]
    nwk_sec = (nwk_fc >> 3) & 1
    nwk_dst = struct.unpack('<H', data[pos+2:pos+4])[0]
    nwk_src = struct.unpack('<H', data[pos+4:pos+6])[0]
    pos += 8

    nwk_sec_len = 0
    if nwk_sec:
        if pos >= len(data): continue
        sec_ctrl = data[pos]; pos += 1
        pos += 4
        key_id_type = (sec_ctrl >> 3) & 3
        nwk_sec_len = 5
        if key_id_type != 0:
            pos += 8
            nwk_sec_len += 8
        nwk_sec_len += 4

    if pos >= len(data): continue
    aps_fc = data[pos]; pos += 1
    aps_ftype = aps_fc & 0x03
    aps_sec = (aps_fc >> 4) & 1
    aps_frag = (aps_fc >> 3) & 1
    if aps_ftype != 0: continue

    pos += 1; pos += 2; pos += 1; pos += 1

    aps_sec_len = 0
    if aps_sec:
        if pos >= len(data): continue
        aps_sec_ctrl = data[pos]; pos += 1
        pos += 4
        aps_key_id_type = (aps_sec_ctrl >> 3) & 3
        aps_sec_len = 5
        if aps_key_id_type != 0:
            pos += 8
            aps_sec_len += 8
        aps_sec_len += 4

    if aps_frag:
        pos += 2

    zcl_payload_len = len(data) - pos
    if zcl_payload_len < 0: zcl_payload_len = 0

    direction = 'SEND' if src_short == TARGET else 'RECV'
    frames_703f.append({
        'idx': i, 'dir': direction, 'caplen': caplen,
        'nwk_sec': nwk_sec, 'aps_sec': aps_sec, 'aps_frag': aps_frag,
        'nwk_sec_len': nwk_sec_len, 'aps_sec_len': aps_sec_len,
        'zcl_payload_len': zcl_payload_len,
    })

print('=== 0x703F (餐厅开关1) 通信分析 ===')
print()
send_frames = [f for f in frames_703f if f['dir'] == 'SEND']
recv_frames = [f for f in frames_703f if f['dir'] == 'RECV']
print('0x703F 相关帧总数: %d' % len(frames_703f))
print('  SEND (0x703F 发出): %d' % len(send_frames))
print('  RECV (0x703F 收到): %d' % len(recv_frames))

print()
print('--- 安全配置统计 ---')
nwk_sec_count = sum(1 for f in frames_703f if f['nwk_sec'])
aps_sec_count = sum(1 for f in frames_703f if f['aps_sec'])
frag_count = sum(1 for f in frames_703f if f['aps_frag'])
print('  NWK 安全启用: %d/%d' % (nwk_sec_count, len(frames_703f)))
print('  APS 安全启用: %d/%d' % (aps_sec_count, len(frames_703f)))
print('  APS 分帧: %d/%d' % (frag_count, len(frames_703f)))

if frames_703f:
    f0 = frames_703f[0]
    nwk_sl = f0['nwk_sec_len']
    aps_sl = f0['aps_sec_len']
    total_sec = nwk_sl + aps_sl
    print()
    print('--- 安全开销 (首帧) ---')
    print('  NWK 安全开销: %d 字节' % nwk_sl)
    print('  APS 安全开销: %d 字节' % aps_sl)
    print('  总安全开销: %d 字节' % total_sec)

    mac_max = 116
    nwk_hdr = 8
    aps_hdr = 6
    af_hdr = 0
    mtu = mac_max - nwk_hdr - nwk_sl - aps_hdr - aps_sl - af_hdr
    print()
    print('--- MTU 计算 ---')
    print('  MAC_MAX_FRAME_SIZE: %d' % mac_max)
    print('  NWK 头: %d' % nwk_hdr)
    print('  NWK 安全: %d' % nwk_sl)
    print('  APS 头: %d' % aps_hdr)
    print('  APS 安全: %d' % aps_sl)
    print('  AF 头: %d' % af_hdr)
    print('  MTU = %d - %d - %d - %d - %d - %d = %d 字节' % (mac_max, nwk_hdr, nwk_sl, aps_hdr, aps_sl, af_hdr, mtu))

print()
print('--- 0x703F 发送帧的 ZCL payload 大小 ---')
send_zcl = Counter(f['zcl_payload_len'] for f in send_frames)
for s, c in sorted(send_zcl.items()):
    print('  ZCL payload %d 字节: %d 帧' % (s, c))

print()
print('--- 0x703F 收到帧的 ZCL payload 大小 ---')
recv_zcl = Counter(f['zcl_payload_len'] for f in recv_frames)
for s, c in sorted(recv_zcl.items()):
    print('  ZCL payload %d 字节: %d 帧' % (s, c))

# 计算 v1.0.9 vs v1.0.10 的 Read Attrs Rsp 大小
print()
print('=== Read Attrs Rsp 帧大小计算 ===')
attrs = {
    0x0000: ('uint8', 1),
    0x0001: ('uint8', 1),
    0x0004: ('CHAR_STR "Linxeee"', 1+6),
    0x0005: ('CHAR_STR "LXN-4S27LX1.0"', 1+13),
    0x0006: ('CHAR_STR "20260729"', 1+8),
    0x0007: ('ENUM8', 1),
    0x0010: ('CHAR_STR 17B', 1+17),
    0x0011: ('ENUM8', 1),
    0x0012: ('BOOLEAN', 1),
    0x4000: ('CHAR_STR SwBuildId', '16/17'),
    0xFFFD: ('UINT16', 2),
}

print('属性响应大小 (每个 = attrID(2) + status(1) + type(1) + value):')
for attr_id, (desc, val_size) in sorted(attrs.items()):
    if isinstance(val_size, str):
        v9 = int(val_size.split('/')[0])
        v10 = int(val_size.split('/')[1])
        print('  0x%04X %-25s v1.0.9=%d v1.0.10=%d' % (attr_id, desc, 2+1+1+v9, 2+1+1+v10))
    else:
        print('  0x%04X %-25s %d' % (attr_id, desc, 2+1+1+val_size))

# 常见 z2m 读取组合
print()
print('--- z2m 批量读取的响应帧大小估算 ---')
combos = [
    ('7属性(无HWVer,无LocDesc)', [0x0000, 0x0004, 0x0005, 0x0006, 0x0007, 0x4000, 0xFFFD]),
    ('8属性(含HWVer)', [0x0000, 0x0001, 0x0004, 0x0005, 0x0006, 0x0007, 0x4000, 0xFFFD]),
    ('8属性(含LocDesc)', [0x0000, 0x0004, 0x0005, 0x0006, 0x0007, 0x0010, 0x4000, 0xFFFD]),
    ('9属性(含HWVer+LocDesc)', [0x0000, 0x0001, 0x0004, 0x0005, 0x0006, 0x0007, 0x0010, 0x4000, 0xFFFD]),
]

val_sizes = {
    0x0000: 1, 0x0001: 1, 0x0004: 1+6, 0x0005: 1+13,
    0x0006: 1+8, 0x0007: 1, 0x0010: 1+17, 0x0011: 1,
    0x0012: 1, 0xFFFD: 2,
}

if frames_703f:
    mtu_val = mtu
    print('MTU = %d 字节' % mtu_val)
    print()
    for name, attr_list in combos:
        payload_v9 = sum(2+1+1+val_sizes[a] for a in attr_list if a != 0x4000) + 2+1+1+1+16
        payload_v10 = sum(2+1+1+val_sizes[a] for a in attr_list if a != 0x4000) + 2+1+1+1+17
        zcl_v9 = 3 + payload_v9
        zcl_v10 = 3 + payload_v10
        status9 = 'OK' if zcl_v9 <= mtu_val else 'FRAG!'
        status10 = 'OK' if zcl_v10 <= mtu_val else 'FRAG!'
        print('%-30s v1.0.9 ZCL=%d [%s]  v1.0.10 ZCL=%d [%s]' % (name, zcl_v9, status9, zcl_v10, status10))
