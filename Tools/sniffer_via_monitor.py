#!/usr/bin/env python3
"""
临时抓包脚本：通过 CCLoader /api/monitor 接口接收 ZBOSS sniffer 数据
（绕过 sniffer 模式 bug，用 monitor 模式抓包）

用法: python sniffer_via_monitor.py [ESP8266_IP] [输出pcap]
示例: python sniffer_via_monitor.py 10.0.0.147 capture.pcap
"""
import sys, os, time, struct, base64, requests

IP = sys.argv[1] if len(sys.argv) > 1 else "10.0.0.147"
OUTPUT = sys.argv[2] if len(sys.argv) > 2 else f"cap/monitor_{time.strftime('%Y%m%d_%H%M%S')}.pcap"

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
            # 无效长度，跳过 1 字节重新同步
            pos += 1
            continue
        if pos + pkt_len > len(buf):
            break  # 数据不完整，等待更多数据
        if pkt_type == 0xFF:  # 丢包标记
            dropped = struct.unpack('>I', buf[pos+4:pos+8])[0]
            print(f"[WARN] 丢失 {dropped} 字节")
            pos += pkt_len
            continue
        if pkt_type != 0:  # 非 OK，跳过
            pos += pkt_len
            continue
        # 提取 IEEE 802.15.4 帧（移除最后 1 字节 CRC 状态）
        payload = buf[pos + 4 : pos + pkt_len]
        if len(payload) > 1:
            ieee_frame = payload[:-1]  # 移除 CRC 状态字节
            packets.append(ieee_frame)
        pos += pkt_len
    return packets, buf[pos:]

def main():
    os.makedirs(os.path.dirname(OUTPUT) or ".", exist_ok=True)
    print(f"=== CCLoader Monitor 抓包 ===")
    print(f"ESP8266: {IP}")
    print(f"输出: {OUTPUT}")
    print(f"[1] 启动 monitor 模式 (115200)...")

    # 启动 monitor
    r = requests.post(f"http://{IP}/api/monitor",
                      json={"baud": 115200, "auto_reset": False}, timeout=10)
    if r.status_code != 200:
        print(f"[ERROR] 启动失败: {r.text}")
        sys.exit(1)
    print(f"[OK] monitor 已启动")

    # 开始接收
    offset = 0
    stream_buf = bytearray()
    pkt_count = 0
    start_time = time.time()
    last_report = start_time

    print(f"[RX] 监听中... (Ctrl+C 停止)")

    with open(OUTPUT, 'wb') as f:
        write_pcap_header(f)
        try:
            while True:
                try:
                    r = requests.get(f"http://{IP}/api/monitor/buffer?since={offset}",
                                     timeout=5)
                    d = r.json()
                except Exception as e:
                    print(f"  [warn] 请求失败: {e}")
                    time.sleep(1)
                    continue

                if d.get("data"):
                    raw = base64.b64decode(d["data"])
                    stream_buf.extend(raw)
                    offset = d.get("total", offset)

                    # 解析 ZBOSS 包
                    packets, remaining = parse_zboss_packets(stream_buf)
                    stream_buf = bytearray(remaining)

                    for pkt in packets:
                        write_pcap_packet(f, pkt)
                        pkt_count += 1

                # 定期报告
                now = time.time()
                if now - last_report > 5:
                    elapsed = now - start_time
                    print(f"  [{elapsed:.0f}s] 包数={pkt_count} 缓冲={len(stream_buf)}B offset={offset}")
                    last_report = now
                    f.flush()

                time.sleep(0.1)  # 100ms 轮询

        except KeyboardInterrupt:
            pass

    elapsed = time.time() - start_time
    print(f"\n=== 抓包结束 ===")
    print(f"时长: {elapsed:.1f}s | 包数: {pkt_count} | 文件: {OUTPUT}")

    # 停止 monitor
    try:
        requests.post(f"http://{IP}/api/stop", timeout=5)
    except:
        pass

if __name__ == '__main__':
    main()
