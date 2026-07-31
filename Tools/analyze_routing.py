"""分析借壳固件 0xD334 在抓包中的路由行为"""
import struct, sys
from collections import Counter

path = sys.argv[1] if len(sys.argv) > 1 else "cap/7米重置入网1.pcap"

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

# 统计每个设备的发送/接收
send_count = Counter()
recv_count = Counter()
d334_activity = []

for i, data in enumerate(pkts):
    if len(data) < 5: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01

    pos = 3
    dst_addr = src_addr = None
    try:
        if dam >= 2:
            pos += 2  # dst pan
            if dam == 2: dst_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif dam == 3: dst_addr = data[pos:pos+8]; pos += 8
        if sam >= 2:
            if not pan_comp: pos += 2
            if sam == 2: src_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif sam == 3: src_addr = data[pos:pos+8]; pos += 8
    except (struct.error, IndexError):
        continue

    def fmt(a):
        if a is None: return None
        if isinstance(a, int): return f'0x{a:04X}'
        return a.hex()

    src_s = fmt(src_addr)
    dst_s = fmt(dst_addr)
    if src_s: send_count[src_s] += 1
    if dst_s: recv_count[dst_s] += 1

    # 记录 0xD334 的所有活动
    if src_s == '0xD334' or dst_s == '0xD334':
        ftype_name = {0:'Beacon', 1:'Data', 2:'ACK', 3:'MAC cmd'}.get(ftype, f'Type{ftype}')
        d334_activity.append({
            'idx': i,
            'type': ftype_name,
            'src': src_s or '?',
            'dst': dst_s or '?',
            'fc': f'0x{fc:04X}',
        })

print("--- 设备发送统计 (Top 10) ---")
for addr, cnt in send_count.most_common(10):
    print(f"  {addr}: {cnt} 包")

print(f"\n--- 0xD334 (客厅开关1, HGZBSwitch借壳) 活动统计 ---")
print(f"总活动: {len(d334_activity)} 包")
print(f"作为源: {sum(1 for a in d334_activity if a['src'] == '0xD334')}")
print(f"作为目的: {sum(1 for a in d334_activity if a['dst'] == '0xD334')}")

# 0xD334 与谁通信
d334_peers = Counter()
for a in d334_activity:
    if a['src'] == '0xD334' and a['dst']:
        d334_peers[f"→ {a['dst']}"] += 1
    elif a['dst'] == '0xD334' and a['src'] != '?':
        d334_peers[f"← {a['src']}"] += 1

print(f"\n0xD334 通信对象:")
for peer, cnt in d334_peers.most_common():
    print(f"  {peer}: {cnt} 包")

print(f"\n--- 0xD334 详细活动 (前20) ---")
for a in d334_activity[:20]:
    print(f"  #{a['idx']:3d} {a['type']:8s} {a['src']} → {a['dst']}  fc={a['fc']}")

# 检查是否有 Route Request / Route Reply
print(f"\n--- 路由相关帧 ---")
route_count = 0
for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 1: continue  # 只看 Data 帧

    # NWK 层：FC(2) + Dst(2) + Src(2) + Radius(1) + Seq(1) + ...
    # 但 802.15.4 MAC 层已经包含寻址，NWK 层在 payload 中
    # 简化：检查 NWK 帧控制
    pos = 3
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01
    if dam >= 2:
        pos += 2
        if dam == 2: pos += 2
        elif dam == 3: pos += 8
    if sam >= 2:
        if not pan_comp: pos += 2
        if sam == 2: pos += 2
        elif sam == 3: pos += 8

    if pos + 2 > len(data): continue
    nwk_fc = struct.unpack('<H', data[pos:pos+2])[0]
    nwk_ftype = nwk_fc & 0x03
    # NWK 帧类型: 0=Data, 1=Command, 2=Reserved, 3=Inter-PAN
    if nwk_ftype == 1:  # NWK Command
        if pos + 8 <= len(data):
            nwk_cmd = data[pos+8]  # NWK command ID
            # 0x01=Route Request, 0x02=Route Reply, 0x03=Link Status, etc.
            cmd_names = {
                0x01: 'Route Request',
                0x02: 'Route Reply',
                0x03: 'Link Status',
                0x04: 'Link Power Delta',
                0x05: 'Rejoin Request',
                0x06: 'Rejoin Response',
                0x07: 'Leave',
                0x08: 'Link Status',
            }
            cmd_name = cmd_names.get(nwk_cmd, f'NWK Cmd{nwk_cmd}')
            route_count += 1
            if route_count <= 20:
                print(f"  #{i:3d} {cmd_name:15s}")

print(f"路由相关帧总数: {route_count}")
