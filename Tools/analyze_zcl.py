"""分析 pcap 中的 ZCL 帧，找固件 ID (SwBuildId, ModelId, ManufacturerName 等)"""
import struct, sys
from collections import Counter

path = sys.argv[1] if len(sys.argv) > 1 else "cap/7米进行interviwe.pcap"

with open(path, 'rb') as f:
    f.read(24)
    pkts = []
    while True:
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
        data = f.read(caplen)
        pkts.append(data)

print(f"=== {path} ===")
print(f"总包数: {len(pkts)}\n")

# 找 ZCL 帧（cluster 0x0000 = GenBasic）
CLUSTER_NAMES = {
    0x0000: 'GenBasic',
    0x0006: 'GenOnOff',
    0x0003: 'GenIdentify',
    0x0019: 'GenOta',
    0x000A: 'GenTime',
}

def parse_mac_addr(data, pos):
    """解析 MAC 帧地址，返回 (dst, src, payload_pos)"""
    fc = struct.unpack('<H', data[:2])[0]
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01
    p = pos
    dst_addr = src_addr = None
    if dam >= 2:
        p += 2  # dst pan
        if dam == 2: dst_addr = struct.unpack('<H', data[p:p+2])[0]; p += 2
        elif dam == 3: dst_addr = data[p:p+8]; p += 8
    if sam >= 2:
        if not pan_comp: p += 2
        if sam == 2: src_addr = struct.unpack('<H', data[p:p+2])[0]; p += 2
        elif sam == 3: src_addr = data[p:p+8]; p += 8
    return dst_addr, src_addr, p

def fmt(a):
    if a is None: return None
    if isinstance(a, int): return f'0x{a:04X}'
    return ':'.join(f'{b:02x}' for b in a)

zcl_frames = []

for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 1: continue  # 只看 Data 帧

    try:
        dst_addr, src_addr, pos = parse_mac_addr(data, 3)
    except Exception:
        continue

    # NWK 层
    if pos + 8 > len(data): continue
    nwk_fc = struct.unpack('<H', data[pos:pos+2])[0]
    nwk_ftype = nwk_fc & 0x03
    if nwk_ftype != 0: continue  # 只看 NWK Data
    nwk_dst = struct.unpack('<H', data[pos+2:pos+4])[0]
    nwk_src = struct.unpack('<H', data[pos+4:pos+6])[0]
    nwk_radius = data[pos+6]
    nwk_seq = data[pos+7]
    pos += 8

    # NWK security frame control (if security bit set)
    nwk_sec = (nwk_fc >> 3) & 0x01
    if nwk_sec:
        # skip aux header (1-14 bytes typical)
        if pos + 1 > len(data): continue
        sec_ctrl = data[pos]
        pos += 1
        # FC bits: nwk_sec_level(2) + key_id(3) + ...
        sec_level = sec_ctrl & 0x03
        key_id_type = (sec_ctrl >> 3) & 0x03
        # nonce includes src addr(2/8) + frame counter(4)
        if pos + 5 > len(data): continue
        pos += 4  # frame counter
        # 如果 key_id_type 不为 0，后面还有 key_source(8) + key_seq(1)
        if key_id_type != 0:
            pos += 8
        # 加密的 payload 后面有 MIC（sec_level 决定长度）
        # 简化：标记加密，跳过
        encrypted = True
    else:
        encrypted = False

    if pos >= len(data): continue
    # APS 层
    aps_fc = data[pos]
    pos += 1
    aps_ftype = aps_fc & 0x03
    if aps_ftype != 0: continue  # 只看 APS Data
    aps_dst_ep = data[pos] if pos < len(data) else 0; pos += 1
    aps_cluster = struct.unpack('<H', data[pos:pos+2])[0] if pos+2 <= len(data) else 0; pos += 2
    aps_src_ep = data[pos] if pos < len(data) else 0; pos += 1
    aps_counter = data[pos] if pos < len(data) else 0; pos += 1

    # ZCL
    if pos >= len(data): continue
    zcl_fc = data[pos]; pos += 1
    zcl_ftype = zcl_fc & 0x03
    zcl_cmd_dir = (zcl_fc >> 2) & 0x03
    zcl_cmd = data[pos] if pos < len(data) else 0; pos += 1

    cluster_name = CLUSTER_NAMES.get(aps_cluster, f'Cls{aps_cluster:04X}')

    # 0x00 = Read Attributes, 0x01 = Read Attributes Response
    zcl_cmd_names = {
        0x00: 'Read Attrs',
        0x01: 'Read Attrs Rsp',
        0x02: 'Write Attrs',
        0x04: 'Write Attrs Rsp',
        0x05: 'Write Attrs NoRsp',
        0x06: 'Configure Reporting',
        0x07: 'Configure Reporting Rsp',
        0x0A: 'Report Attrs',
        0x0B: 'Default Response',
        0x0C: 'Discover Attrs',
        0x0D: 'Discover Attrs Rsp',
    }
    cmd_name = zcl_cmd_names.get(zcl_cmd, f'Cmd{zcl_cmd:02X}')

    entry = {
        'idx': i,
        'src': f'0x{nwk_src:04X}',
        'dst': f'0x{nwk_dst:04X}',
        'mac_src': fmt(src_addr),
        'cluster': cluster_name,
        'cluster_id': aps_cluster,
        'cmd': cmd_name,
        'cmd_id': zcl_cmd,
        'encrypted': encrypted,
        'payload': data[pos:],
        'aps_dst_ep': aps_dst_ep,
    }
    zcl_frames.append(entry)

print(f"--- ZCL 帧总数: {len(zcl_frames)} ---\n")

# 按 cluster 分组
by_cluster = Counter(f['cluster'] for f in zcl_frames)
print("按 Cluster 分组:")
for c, n in by_cluster.most_common():
    print(f"  {c}: {n}")

print("\n--- GenBasic (0x0000) ZCL 帧 ---")
for f in zcl_frames:
    if f['cluster_id'] != 0x0000: continue
    enc = '🔒' if f['encrypted'] else '🔓'
    print(f"\n#{f['idx']:3d} {f['src']} → {f['dst']} (ep{f['aps_dst_ep']}) {enc} {f['cmd']}")
    # 解析 Read Attrs Rsp
    if f['cmd_id'] == 0x01 and not f['encrypted']:
        payload = f['payload']
        p = 0
        while p + 4 <= len(payload):
            attr_id = struct.unpack('<H', payload[p:p+2])[0]; p += 2
            status = payload[p]; p += 1
            if status != 0:
                print(f"   attr 0x{attr_id:04X} status={status}")
                continue
            dtype = payload[p]; p += 1
            # 根据数据类型读取
            ATTR_NAMES = {
                0x0000: 'ZCLVersion',
                0x0001: 'AppVersion',
                0x0002: 'StackVersion',
                0x0003: 'HWVersion',
                0x0004: 'ManufacturerName',
                0x0005: 'ModelIdentifier',
                0x0006: 'DateCode',
                0x0007: 'PowerSource',
                0x4000: 'SwBuildId',
                0x000A: 'ProductCode',
                0x000B: 'SWBuildID(alt)',
            }
            attr_name = ATTR_NAMES.get(attr_id, f'Attr{attr_id:04X}')
            # 解析值
            val_str = ''
            try:
                if dtype == 0x20:  # uint8
                    val = payload[p]; p += 1
                    val_str = f'{val}'
                elif dtype == 0x21:  # uint16
                    val = struct.unpack('<H', payload[p:p+2])[0]; p += 2
                    val_str = f'{val}'
                elif dtype == 0x30:  # octet string
                    slen = payload[p]; p += 1
                    val_str = payload[p:p+slen].hex()
                    p += slen
                elif dtype == 0x42:  # char string
                    slen = payload[p]; p += 1
                    val_str = payload[p:p+slen].decode('utf-8', errors='replace')
                    p += slen
                else:
                    val_str = f'type=0x{dtype:02X}'
                    # 尝试跳过
                    if dtype in (0x28, 0x29): p += 1
                    elif dtype in (0x29, 0x2B): p += 2
                    else: break
            except Exception as e:
                val_str = f'parse_err: {e}'
            print(f"   {attr_name} (0x{attr_id:04X}, type=0x{dtype:02X}): {val_str}")

print("\n--- 所有 ZCL 帧的源设备 ---")
src_devices = Counter(f['src'] for f in zcl_frames)
for s, n in src_devices.most_common():
    # 找 mac_src
    mac = next((f['mac_src'] for f in zcl_frames if f['src'] == s), '?')
    print(f"  {s} (MAC={mac}): {n} 个 ZCL 帧")
