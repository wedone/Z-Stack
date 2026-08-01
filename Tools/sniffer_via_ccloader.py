#!/usr/bin/env python3
"""
CCLoader Sniffer 客户端 - 通过 WiFi /api/sniffer/stream 接收 sniffer 数据
修复版：使用二进制透传（非 Base64），32KB 缓冲，低延迟

用法: python sniffer_via_ccloader.py [ESP8266_IP] [通道号] [输出pcap]
示例: python sniffer_via_ccloader.py 10.0.0.147 11 capture.pcap
"""
import sys, os, time, struct, requests

IP = sys.argv[1] if len(sys.argv) > 1 else "10.0.0.147"
CHANNEL = int(sys.argv[2]) if len(sys.argv) > 2 else 11
OUTPUT = sys.argv[3] if len(sys.argv) > 3 else f"cap/sniffer_{time.strftime('%Y%m%d_%H%M%S')}.pcap"

DLT_IEEE802_15_4_NOFCS = 230

def write_pcap_header(f):
    f.write(struct.pack('<IHHiIII', 0xa1b2c3d4, 2, 4, 0, 0, 65535, DLT_IEEE802_15_4_NOFCS))

def write_pcap_packet(f, data, ts=None):
    if ts is None:
        ts = time.time()
    sec = int(ts)
    usec = int((ts - sec) * 1000000)
    f.write(struct.pack('<IIII', sec, usec, len(data), len(data)))
    f.write(data)

def parse_zboss_packets(buf):
    """从字节流中解析完整的 ZBOSS 包，返回 (packets, remaining_bytes)"""
    packets = []
    pos = 0
    while pos + 4 <= len(buf):
        pkt_len = buf[pos]
        pkt_type = buf[pos + 1]
        if pkt_len < 5:
            pos += 1
            continue
        if pos + pkt_len > len(buf):
            break
        if pkt_type == 0xFF:  # 丢包标记
            dropped = struct.unpack('>I', buf[pos+4:pos+8])[0] if pos+8 <= len(buf) else 0
            print(f"[WARN] CCLoader 丢失 {dropped} 字节")
            pos += pkt_len
            continue
        if pkt_type != 0:  # 非 OK，跳过
            pos += pkt_len
            continue
        # 提取 IEEE 802.15.4 帧（移除最后 2 字节：LQI + CRC 状态）
        # ZBOSS sniffer payload: [802.15.4帧(无FCS)][LQI(1B)][CRC状态(1B)]
        # 之前只删 1 字节会保留 LQI，导致 wireshark 帧解析错位无法解密
        payload = buf[pos + 4 : pos + pkt_len]
        if len(payload) > 2:
            ieee_frame = payload[:-2]
            packets.append(ieee_frame)
        pos += pkt_len
    return packets, buf[pos:]

def main():
    os.makedirs(os.path.dirname(OUTPUT) or ".", exist_ok=True)
    print(f"=== CCLoader Sniffer (stream 模式) ===")
    print(f"ESP8266: {IP}")
    print(f"通道: {CHANNEL}")
    print(f"输出: {OUTPUT}")
    print(f"[1] 启动 sniffer 模式...")

    r = requests.post(f"http://{IP}/api/sniffer/start",
                      json={"channel": CHANNEL}, timeout=10)
    if r.status_code != 200:
        print(f"[ERROR] 启动失败: {r.text}")
        sys.exit(1)
    print(f"[OK] sniffer 已启动: {r.json()}")

    stream_buf = bytearray()
    pkt_count = 0
    start_time = time.time()
    last_report = start_time

    print(f"[RX] 监听中... (Ctrl+C 停止)")

    with open(OUTPUT, 'wb') as f:
        write_pcap_header(f)
        try:
            with requests.get(f"http://{IP}/api/sniffer/stream",
                              stream=True, timeout=None) as r:
                for chunk in r.iter_content(chunk_size=1024):
                    if not chunk:
                        continue
                    stream_buf.extend(chunk)

                    packets, remaining = parse_zboss_packets(stream_buf)
                    stream_buf = bytearray(remaining)

                    for pkt in packets:
                        write_pcap_packet(f, pkt)
                        pkt_count += 1

                    now = time.time()
                    if now - last_report > 5:
                        elapsed = now - start_time
                        print(f"  [{elapsed:.0f}s] 包数={pkt_count} 缓冲={len(stream_buf)}B")
                        last_report = now
                        f.flush()

        except KeyboardInterrupt:
            pass

    elapsed = time.time() - start_time
    print(f"\n=== 抓包结束 ===")
    print(f"时长: {elapsed:.1f}s | 包数: {pkt_count} | 文件: {OUTPUT}")

    try:
        requests.post(f"http://{IP}/api/stop", timeout=5)
    except:
        pass

if __name__ == '__main__':
    main()
