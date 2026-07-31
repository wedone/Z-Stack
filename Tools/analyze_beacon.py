"""分析 Beacon Response 的源地址、PAN ID、pan_coord 和 RSSI"""
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
        pkts.append((ts_sec + ts_usec/1e6, data))

print(f"=== {path} ===")
print(f"总包数: {len(pkts)}\n")

beacon_responses = []
beacon_requests = []

for i, (ts, data) in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07

    if ftype == 0:  # Beacon
        # 802.15.4 Beacon: FC(2) + Seq(1) + Src PAN(2) + Src Addr(2 or 8)
        sam = (fc >> 14) & 0x03
        pos = 3
        src_pan = None
        src_addr = None
        if sam >= 2:
            src_pan = struct.unpack('<H', data[pos:pos+2])[0]
            pos += 2
            if sam == 2:
                src_addr = struct.unpack('<H', data[pos:pos+2])[0]
                pos += 2
            elif sam == 3:
                src_addr = data[pos:pos+8]
                pos += 8

        # Beacon payload: Superframe Spec(2) + GTS(1) + Pending(1) + Protocol(1) + ...
        # Superframe spec: beacon_order(4) + superframe_order(4) + final_cap_slot(4) +
        #                  battery_ext(1) + coord(1) + assoc_permit(1) + reserved(1)
        if pos + 2 <= len(data):
            sf_spec = struct.unpack('<H', data[pos:pos+2])[0]
            pan_coord = (sf_spec >> 15) & 0x01
            assoc_permit = (sf_spec >> 14) & 0x01
            beacon_order = sf_spec & 0x0F

            # 后面是协议字段（ZigBee Beacon）
            # beacon payload: superframe(2) + GTS(1) + pending(1) + protocol(1) + stack_profile(1) +
            #                 protocol_id(1) + router_capacity(1) + ... + extended_pan_id(8) + tx_offset(4) + ...
            # 简化：只取前几个字段
            protocol_id = data[pos+4] if pos+4 < len(data) else 0
            stack_profile = data[pos+5] if pos+5 < len(data) else 0
            # router_capacity, device_capacity, ...
            # 但实际格式更复杂，这里只看 pan_coord

            src_s = f'0x{src_addr:04X}' if isinstance(src_addr, int) else (src_addr.hex() if src_addr else '?')

            # RSSI: 在 ZBOSS sniffer 中，RSSI 通常附加在帧末尾
            # ZBOSS format: 4字节header + IEEE帧 + 2字节CRC状态/FCS
            # RSSI 可能在 FCS 之后（最后一个字节）
            rssi = None
            if len(data) > 0:
                # 假设最后一个字节是 RSSI（带符号）
                rssi_raw = data[-1]
                if rssi_raw >= 128:
                    rssi = rssi_raw - 256
                else:
                    rssi = rssi_raw

            beacon_responses.append({
                'idx': i,
                'ts': ts,
                'src': src_s,
                'src_pan': f'0x{src_pan:04X}' if src_pan is not None else '-',
                'pan_coord': pan_coord,
                'assoc_permit': assoc_permit,
                'beacon_order': beacon_order,
                'protocol_id': f'0x{protocol_id:02X}',
                'stack_profile': stack_profile & 0x0F,
                'rssi': rssi,
                'len': len(data),
            })

    elif ftype == 3:  # MAC cmd
        # 检查是否 Beacon Request
        dam = (fc >> 10) & 0x03
        sam = (fc >> 14) & 0x03
        pan_comp = (fc >> 6) & 0x01
        pos = 3
        dst_addr = src_addr = None
        if dam >= 2:
            pos += 2
            if dam == 2: dst_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif dam == 3: dst_addr = data[pos:pos+8]; pos += 8
        if sam >= 2:
            if not pan_comp: pos += 2
            if sam == 2: src_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
            elif sam == 3: src_addr = data[pos:pos+8]; pos += 8
        if pos < len(data) and data[pos] == 0x07:  # Beacon Request
            beacon_requests.append({'idx': i, 'ts': ts})

print(f"--- Beacon Requests ({len(beacon_requests)}) ---")
for e in beacon_requests[:5]:
    print(f"  #{e['idx']} ts={e['ts']:.6f}")

print(f"\n--- Beacon Responses ({len(beacon_responses)}) ---")
print(f"{'#':>4} {'src':>10} {'pan':>8} {'pc':>3} {'ap':>3} {'bo':>3} {'pid':>5} {'sp':>3} {'rssi':>5} {'len':>4}")
for e in beacon_responses:
    print(f"  #{e['idx']:3d} {e['src']:>10} {e['src_pan']:>8} {e['pan_coord']:>3} {e['assoc_permit']:>3} {e['beacon_order']:>3} {e['protocol_id']:>5} {e['stack_profile']:>3} {e['rssi']:>5} {e['len']:>4}")

# 按 PAN ID 分组
by_pan = Counter(e['src_pan'] for e in beacon_responses)
print(f"\n--- Beacon 按PAN分组 ---")
for pan, cnt in by_pan.most_common():
    print(f"  PAN {pan}: {cnt} 个 Beacon")

# 本网 Beacon（PAN=0x1A62）
print(f"\n--- 本网 Beacon（PAN=0x1A62）按 RSSI 排序 ---")
own = [e for e in beacon_responses if e['src_pan'] == '0x1A62']
own.sort(key=lambda x: -(x['rssi'] or -999))
for e in own:
    pc = '协调器' if e['pan_coord'] == 1 else '路由器'
    print(f"  {e['src']:>10} ({pc}) RSSI={e['rssi']:>4} dBm  assoc_permit={e['assoc_permit']}")
