"""深入分析 pcap 中的入网流程：Beacon、Association、Data Request 等"""
import struct, sys
from collections import Counter, defaultdict

path = sys.argv[1] if len(sys.argv) > 1 else "cap/烧录后未重新上电正常入网.pcap"

with open(path, 'rb') as f:
    f.read(24)  # pcap header
    pkts = []
    while True:
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
        data = f.read(caplen)
        pkts.append(data)

print(f"=== {path} ===")
print(f"总包数: {len(pkts)}")

# MAC 命令类型
MAC_CMDS = {
    0x01: "Assoc Request",
    0x02: "Assoc Response",
    0x03: "Disassoc Notif",
    0x04: "Data Request",
    0x05: "PAN ID Conflict",
    0x06: "Orphan Notif",
    0x07: "Beacon Request",
    0x08: "Coord Realign",
    0x09: "GTS Request",
}

# 统计
beacon_requests = []
assoc_requests = []
assoc_responses = []
data_requests = []
orphan_notifs = []

for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 3: continue  # 非 MAC cmd

    # MAC cmd 在 frame payload 末尾（在有 security 时位置不同，简化处理）
    # 802.15.4: FC(2) + Seq(1) + Addr fields + cmd_id(1) + payload
    # 解析地址模式
    dam = (fc >> 10) & 0x03  # dst addressing mode
    sam = (fc >> 14) & 0x03  # src addressing mode
    pan_comp = (fc >> 6) & 0x01

    pos = 3  # 跳过 FC(2) + Seq(1)
    # dst pan id (2)
    dst_pan = None
    dst_addr = None
    if dam >= 2:
        dst_pan = struct.unpack('<H', data[pos:pos+2])[0]
        pos += 2
        if dam == 2:  # 16-bit
            dst_addr = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
        elif dam == 3:  # 64-bit
            dst_addr = data[pos:pos+8]
            pos += 8

    # src pan id (if not compressed)
    src_pan = None
    src_addr = None
    if sam >= 2:
        if not pan_comp:
            src_pan = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
        if sam == 2:
            src_addr = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
        elif sam == 3:
            src_addr = data[pos:pos+8]
            pos += 8

    if pos >= len(data): continue
    cmd_id = data[pos]
    cmd_name = MAC_CMDS.get(cmd_id, f"Cmd{cmd_id}")

    entry = {
        'idx': i,
        'cmd': cmd_name,
        'cmd_id': cmd_id,
        'src': f'0x{src_addr:04X}' if isinstance(src_addr, int) else (src_addr.hex() if src_addr else '?'),
        'dst': f'0x{dst_addr:04X}' if isinstance(dst_addr, int) else (dst_addr.hex() if dst_addr else '?'),
        'dst_pan': f'0x{dst_pan:04X}' if dst_pan is not None else '-',
    }

    # Association Response 有额外数据（short addr + status）
    if cmd_id == 0x02 and pos + 3 <= len(data):
        short_addr = struct.unpack('<H', data[pos+1:pos+3])[0]
        status = data[pos+3] if pos+3 < len(data) else -1
        entry['short_addr'] = f'0x{short_addr:04X}'
        entry['status'] = status  # 0=success

    if cmd_id == 0x01: assoc_requests.append(entry)
    elif cmd_id == 0x02: assoc_responses.append(entry)
    elif cmd_id == 0x04: data_requests.append(entry)
    elif cmd_id == 0x06: orphan_notifs.append(entry)
    elif cmd_id == 0x07: beacon_requests.append(entry)

print(f"\n--- 入网流程 ---")
print(f"Beacon Request: {len(beacon_requests)}")
print(f"Association Request: {len(assoc_requests)}")
print(f"Association Response: {len(assoc_responses)}")
print(f"Data Request: {len(data_requests)}")
print(f"Orphan Notification: {len(orphan_notifs)}")

print(f"\n--- Beacon Requests ---")
for e in beacon_requests[:10]:
    print(f"  #{e['idx']} src={e['src']} -> {e['dst']} pan={e['dst_pan']}")

print(f"\n--- Association Requests ---")
for e in assoc_requests[:10]:
    print(f"  #{e['idx']} src={e['src']} -> {e['dst']} pan={e['dst_pan']}")

print(f"\n--- Association Responses ---")
for e in assoc_responses[:10]:
    sa = e.get('short_addr', '?')
    st = e.get('status', '?')
    st_str = 'success' if st == 0 else (f'fail({st})' if isinstance(st, int) else st)
    print(f"  #{e['idx']} src={e['src']} -> {e['dst']} short_addr={sa} status={st_str}")

# 设备活动汇总
print(f"\n--- 新设备活动 ---")
new_devices = set()
for e in beacon_requests + assoc_requests + orphan_notifs:
    if e['src'] != '?' and e['src'] not in ('0x0000', '0x4B9F', '0x7CDA', '0xBF20', '0x6F89', '0x3110', '0x6BF5'):
        new_devices.add(e['src'])
print(f"疑似新设备: {new_devices}")

for dev in new_devices:
    acts = []
    for e in beacon_requests:
        if e['src'] == dev: acts.append(f"#{e['idx']}BeaconReq")
    for e in assoc_requests:
        if e['src'] == dev: acts.append(f"#{e['idx']}AssocReq")
    for e in assoc_responses:
        if e['dst'] == dev: acts.append(f"#{e['idx']}AssocRsp(sa={e.get('short_addr','?')},st={e.get('status','?')})")
    for e in data_requests:
        if e['src'] == dev: acts.append(f"#{e['idx']}DataReq")
    print(f"  {dev}: {' -> '.join(acts) if acts else '无活动'}")
