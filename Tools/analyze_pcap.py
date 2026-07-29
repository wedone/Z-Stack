"""分析 pcap 文件，输出帧类型、地址、时间分布"""
import struct, sys
from collections import Counter

if len(sys.argv) < 2:
    print("用法: python analyze_pcap.py <pcap文件>")
    sys.exit(1)

path = sys.argv[1]
with open(path, 'rb') as f:
    magic, vmaj, vmin, zone, sigfig, snaplen, dlt = struct.unpack('<IHHiIII', f.read(24))
    pkts = []
    while True:
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
        data = f.read(caplen)
        ts = ts_sec + ts_usec / 1000000
        pkts.append((ts, data))

print(f'=== {path} ===')
print(f'总包数: {len(pkts)}')
if not pkts: sys.exit(0)

dur = pkts[-1][0] - pkts[0][0]
print(f'时长: {dur:.2f}s')
if dur > 0:
    print(f'速率: {len(pkts)/dur:.1f} 包/秒')

ftypes = Counter()
src_addrs = Counter()
dst_addrs = Counter()
for ts, data in pkts:
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    ftypes[ftype] += 1
    if len(data) >= 7:
        dst = data[5] | (data[6] << 8)
        if dst != 0xFFFF:
            dst_addrs[f'0x{dst:04X}'] += 1
    if len(data) >= 9 and not (fc & 0x40):
        src = data[7] | (data[8] << 8)
        src_addrs[f'0x{src:04X}'] += 1
    elif len(data) >= 9 and (fc & 0x40):
        src = data[7] | (data[8] << 8)
        src_addrs[f'0x{src:04X}'] += 1

type_names = {0: 'Beacon', 1: 'Data', 2: 'ACK', 3: 'MAC cmd'}
print('\n帧类型:')
for t, c in ftypes.most_common():
    print(f'  {type_names.get(t, f"type{t}")}: {c}')

print('\n目的地址 Top5:')
for a, c in dst_addrs.most_common(5):
    print(f'  {a}: {c}')

print('\n源地址 Top5:')
for a, c in src_addrs.most_common(5):
    print(f'  {a}: {c}')

print('\n时间分布 (5秒桶):')
buckets = Counter()
for ts, _ in pkts:
    rel = ts - pkts[0][0]
    buckets[int(rel / 5)] += 1
for b in sorted(buckets):
    print(f'  {b*5:>3}-{b*5+5}s: {buckets[b]}')
