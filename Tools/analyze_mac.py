"""列出 pcap 中所有出现过的 MAC 长地址"""
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

mac_count = Counter()
mac_pkts = {}

for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01

    pos = 3
    dst_mac = src_mac = None
    try:
        if dam == 3:  # 64-bit dst
            pos += 2  # dst pan
            dst_mac = data[pos:pos+8]; pos += 8
        elif dam == 2:
            pos += 2
            pos += 2
        if sam == 3:  # 64-bit src
            if not pan_comp: pos += 2
            src_mac = data[pos:pos+8]; pos += 8
        elif sam == 2:
            if not pan_comp: pos += 2
            pos += 2
    except Exception:
        continue

    for mac in [dst_mac, src_mac]:
        if mac is None: continue
        mac_hex = ':'.join(f'{b:02x}' for b in mac)
        mac_count[mac_hex] += 1
        if mac_hex not in mac_pkts:
            mac_pkts[mac_hex] = []
        if len(mac_pkts[mac_hex]) < 5:
            mac_pkts[mac_hex].append((i, ftype, 'src' if mac is src_mac else 'dst'))

print("--- 所有 MAC 长地址 ---")
for mac, cnt in mac_count.most_common():
    samples = mac_pkts[mac][:3]
    sample_str = ', '.join(f"#{s[0]}(type={s[1]},{s[2]})" for s in samples)
    print(f"  {mac}: {cnt} 包  ({sample_str})")
