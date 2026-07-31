"""检查 pcap 文件格式和帧结构"""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else "cap/7米重置入网1.pcap"

with open(path, 'rb') as f:
    # pcap global header
    magic = f.read(4)
    if magic == b'\xd4\xc3\xb2\xa1':
        endian = '<'
        print("字节序: 小端 (微秒)")
    elif magic == b'\xa1\xb2\xc3\xd4':
        endian = '>'
        print("字节序: 大端 (微秒)")
    elif magic == b'\x4d\x3c\xb2\xa1':
        endian = '<'
        print("字节序: 小端 (纳秒)")
    else:
        print(f"未知 magic: {magic.hex()}")
        sys.exit(1)

    vmaj, vmin, zone, sigfig, snaplen, dlt = struct.unpack(endian + 'HHiIII', f.read(20))
    print(f"版本: {vmaj}.{vmin}")
    print(f"snaplen: {snaplen}")
    print(f"DLT: {dlt}")

    DLT_NAMES = {
        195: 'IEEE802_15_4_WITHFCS',
        230: 'IEEE802_15_4_NOFCS',
        104: 'IEEE802_15_4',
        147: 'IEEE802_15_4_NONASK_PHY',
    }
    print(f"DLT 名称: {DLT_NAMES.get(dlt, '未知')}")

    # 读取前几个包
    print(f"\n--- 前5个包的原始数据 ---")
    for i in range(5):
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack(endian + 'IIII', h)
        data = f.read(caplen)
        print(f"\n包 #{i}: caplen={caplen}, origlen={origlen}")
        print(f"  hex: {data.hex()}")
        print(f"  最后4字节: {data[-4:].hex()}")
        print(f"  最后2字节: {data[-2:].hex()} (可能是 FCS 或 RSSI)")

    # 找到 Beacon 包
    print(f"\n--- Beacon 帧原始数据 ---")
    f.seek(24)
    idx = 0
    beacon_count = 0
    while beacon_count < 5:
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack(endian + 'IIII', h)
        data = f.read(caplen)
        if len(data) < 2: continue
        fc = struct.unpack('<H', data[:2])[0]
        ftype = fc & 0x07
        if ftype == 0:  # Beacon
            print(f"\nBeacon #{idx}: caplen={caplen}")
            print(f"  hex: {data.hex()}")
            print(f"  最后4字节: {data[-4:].hex()}")
            # 解析最后几个字节
            if len(data) >= 4:
                # ZBOSS sniffer: 可能末尾是 RSSI(1) + CRC状态(2) 或其他
                print(f"  倒数第1字节: {data[-1]} (signed: {data[-1] if data[-1]<128 else data[-1]-256})")
                print(f"  倒数第2字节: {data[-2]}")
                print(f"  倒数第3字节: {data[-3]}")
            beacon_count += 1
        idx += 1
