"""分析所有抓包中借壳设备（HGZBSwitch）的信号质量（LQI）"""
import struct, sys, os
from collections import defaultdict, Counter

# 借壳设备的短地址（HGZBSwitch）
HGZB_SHORT_ADDRS = {
    '0xD334': '客厅开关1（客厅1）',
    '0x4DA9': '客厅开关1（3米场景）',
    '0x8723': '餐厅开关1（7米场景，第一次）',
    '0x703F': '餐厅开关1（7米场景，第二次）',
}

# 所有抓包文件
CAP_DIR = "D:/VC/Z-Stack/cap"
PCAP_FILES = [
    "烧录后未重新上电正常入网.pcap",
    "烧录后未重新上电不能入网-离3米无遮挡.pcap",
    "烧录后未重新上电不能入网-离7米有非墙体遮挡.pcap",
    "3米先入网7米后入网.pcap",
    "7米重置不能入网.pcap",
    "7米重置入网1.pcap",
    "7米重置入网2.pcap",
    "7米进行interviwe.pcap",
    "借壳信号质量测试.pcap",
]

def parse_pcap(path):
    """解析 pcap，返回 [(ts, data)]"""
    pkts = []
    with open(path, 'rb') as f:
        f.read(24)
        while True:
            h = f.read(16)
            if len(h) < 16: break
            ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
            data = f.read(caplen)
            pkts.append((ts_sec + ts_usec/1e6, data))
    return pkts

def get_short_addr(data):
    """从 MAC 帧中提取源/目的短地址和 LQI"""
    if len(data) < 2: return None, None, None
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01

    pos = 3
    dst_short = src_short = None
    try:
        if dam == 2:
            pos += 2  # dst pan
            dst_short = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
        elif dam == 3:
            pos += 2
            pos += 8
        if sam == 2:
            if not pan_comp: pos += 2
            src_short = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
        elif sam == 3:
            if not pan_comp: pos += 2
            pos += 8
    except Exception:
        return None, None, None

    # LQI 在帧末尾最后一字节（ZBOSS sniffer 格式）
    lqi = data[-1] if len(data) > 0 else None
    return dst_short, src_short, lqi

print("=" * 80)
print("借壳固件（HGZBSwitch）信号质量分析")
print("=" * 80)

for pcap_name in PCAP_FILES:
    pcap_path = os.path.join(CAP_DIR, pcap_name)
    if not os.path.exists(pcap_path):
        continue
    pkts = parse_pcap(pcap_path)
    print(f"\n--- {pcap_name} ({len(pkts)} 包) ---")

    # 收集借壳设备的 LQI
    hgzbs_lqi = defaultdict(list)  # short_addr -> [lqi]
    # 也收集其他设备的 LQI 作为对比
    all_lqi = defaultdict(list)

    for ts, data in pkts:
        dst, src, lqi = get_short_addr(data)
        if lqi is None: continue
        if src is not None:
            src_hex = f'0x{src:04X}'
            all_lqi[src_hex].append(lqi)
            if src_hex in HGZB_SHORT_ADDRS:
                hgzbs_lqi[src_hex].append(lqi)
        if dst is not None:
            dst_hex = f'0x{dst:04X}'
            # 接收方的 LQI 不太准确（是发送方的信号），但可参考
            if dst_hex in HGZB_SHORT_ADDRS:
                # 这是发给借壳设备的包，LQI 反映对端到借壳设备的链路
                pass  # 暂不统计接收 LQI

    if not hgzbs_lqi:
        print("  本抓包中无借壳设备活动")
        continue

    print(f"  借壳设备活动:")
    for addr, lqis in hgzbs_lqi.items():
        name = HGZB_SHORT_ADDRS.get(addr, '?')
        avg = sum(lqis) / len(lqis)
        mx = max(lqis)
        mn = min(lqis)
        print(f"    {addr} ({name}): {len(lqis)} 包, LQI 平均={avg:.1f} 最大={mx} 最小={mn}")

    # 对比其他设备
    print(f"  其他设备 LQI 对比 (Top 5):")
    sorted_devs = sorted(all_lqi.items(), key=lambda x: -len(x[1]))[:5]
    for addr, lqis in sorted_devs:
        if addr in HGZB_SHORT_ADDRS: continue
        avg = sum(lqis) / len(lqis)
        mx = max(lqis)
        mn = min(lqis)
        print(f"    {addr}: {len(lqis)} 包, LQI 平均={avg:.1f} 最大={mx} 最小={mn}")

print("\n" + "=" * 80)
print("信号质量评估说明")
print("=" * 80)
print("""
ZBOSS sniffer 帧末尾最后一字节为 LQI（链路质量指示，0-255）：
- LQI > 200: 信号极好（近距离无遮挡）
- LQI 100-200: 信号良好（正常通信）
- LQI 50-100: 信号较弱（边缘通信）
- LQI < 50: 信号差（可能丢包）

注意：LQI 是 sniffer 接收到的信号质量，不是设备实际接收质量。
      但可作为相对参考，对比不同设备在相同环境下的信号强度。
""")
