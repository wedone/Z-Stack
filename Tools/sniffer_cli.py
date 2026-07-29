#!/usr/bin/env python3
"""
ZBOSS sniffer CLI 抓包工具
直接读取 COM 口 ZBOSS 数据, 实时生成 pcap 文件, 可用 tshark/Wireshark 分析

用法:
  python sniffer_cli.py COM4                     # 通道 11 (默认)
  python sniffer_cli.py COM4 15                  # 通道 15
  python sniffer_cli.py COM4 11 capture.pcap     # 指定输出文件

ZBOSS sniffer 串口协议 (115200, 8N1):
  PC -> sniffer: 1 字节通道号 (11-26)
  sniffer -> PC: [len(1)][type(1)][tail(2)][IEEE802.15.4 帧]
  - len: 总长度 (含 4 字节包头)
  - type: 0=OK, 1=TOO_BIG, 2=OVERFLOW
  - IEEE802.15.4 帧最后一个字节 bit7 = CRC OK 标志
"""
import sys
import os
import time
import struct

try:
    import serial
except ImportError:
    print("[ERROR] 缺少 pyserial: pip install pyserial")
    sys.exit(1)

# pcap 常量
PCAP_MAGIC = 0xa1b2c3d4
PCAP_VERSION_MAJOR = 2
PCAP_VERSION_MINOR = 4
PCAP_THISZONE = 0
PCAP_SIGFIGS = 0
PCAP_SNAPLEN = 65535
DLT_IEEE802_15_4_NOFCS = 230  # IEEE 802.15.4 无 FCS (ZBOSS 已剥离)

def write_pcap_header(f):
    """写 pcap 全局头"""
    header = struct.pack('<IHHiIII',
        PCAP_MAGIC,
        PCAP_VERSION_MAJOR,
        PCAP_VERSION_MINOR,
        PCAP_THISZONE,
        PCAP_SIGFIGS,
        PCAP_SNAPLEN,
        DLT_IEEE802_15_4_NOFCS
    )
    f.write(header)

def write_pcap_packet(f, data, ts_sec, ts_usec):
    """写一个 pcap 包记录"""
    caplen = len(data)
    origlen = caplen
    header = struct.pack('<IIII', ts_sec, ts_usec, caplen, origlen)
    f.write(header)
    f.write(data)

def parse_zboss_packet(buf, pos):
    """
    解析 ZBOSS 包, 返回 (ieee_frame, next_pos) 或 (None, pos+1) 如果数据不完整
    buf: 接收缓冲区
    pos: 当前解析位置
    """
    if pos + 4 > len(buf):
        return None, pos  # 包头不完整

    pkt_len = buf[pos]
    pkt_type = buf[pos + 1]

    if pkt_len < 5:  # 至少 4 字节头 + 1 字节数据
        return None, pos + 1

    if pos + pkt_len > len(buf):
        return None, pos  # 数据不完整

    if pkt_type != 0:  # 非 OK, 跳过
        return None, pos + pkt_len

    # 提取 IEEE 802.15.4 帧
    ieee_data = buf[pos + 4 : pos + pkt_len]

    # 最后一个字节 bit7 是 CRC 标志, 去掉
    if len(ieee_data) > 1:
        ieee_data = ieee_data[:-1]

    return ieee_data, pos + pkt_len

def main():
    if len(sys.argv) < 2:
        print("用法: python sniffer_cli.py <COM端口> [通道号] [输出文件]")
        print("示例: python sniffer_cli.py COM4")
        print("      python sniffer_cli.py COM4 15")
        print("      python sniffer_cli.py COM4 11 capture.pcap")
        sys.exit(1)

    port = sys.argv[1]
    channel = int(sys.argv[2]) if len(sys.argv) >= 3 else 11
    output = sys.argv[3] if len(sys.argv) >= 4 else os.path.join(
        os.path.dirname(__file__), "..", "cap",
        f"capture_{time.strftime('%Y%m%d_%H%M%S')}.pcap"
    )

    if channel < 11 or channel > 26:
        print(f"[ERROR] 通道号必须 11-26, 当前: {channel}")
        sys.exit(1)

    # 确保 cap 目录存在
    os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)

    print(f"=== ZBOSS Sniffer CLI 抓包 ===")
    print(f"端口: {port}")
    print(f"通道: {channel}")
    print(f"输出: {output}")
    print(f"波特率: 115200, 8N1")
    print(f"按 Ctrl+C 停止抓包")
    print()

    # 打开串口
    try:
        ser = serial.Serial(port, 115200, timeout=0.05)
        ser.reset_input_buffer()
        ser.reset_output_buffer()
    except Exception as e:
        print(f"[ERROR] 打开串口失败: {e}")
        sys.exit(1)

    # 发送通道号启动 sniffer
    print(f"[TX] 发送通道号: 0x{channel:02X}")
    ser.write(bytes([channel]))
    ser.flush()

    # 打开 pcap 文件
    with open(output, 'wb') as f:
        write_pcap_header(f)

        total_pkts = 0
        total_bytes = 0
        rx_buf = bytearray()
        start_time = time.time()
        last_report = start_time

        print(f"[RX] 监听中... (通道 {channel})")
        print(f"{'时间':>8} {'#':>5} {'长度':>5} {'内容(前16字节)'}")
        print("-" * 60)

        try:
            while True:
                # 读取数据
                data = ser.read(256)
                if data:
                    rx_buf.extend(data)
                    total_bytes += len(data)

                    # 解析 ZBOSS 包
                    pos = 0
                    while pos < len(rx_buf):
                        ieee_frame, next_pos = parse_zboss_packet(rx_buf, pos)
                        if ieee_frame is None:
                            if next_pos == pos:
                                break  # 数据不完整, 等待更多数据
                            pos = next_pos  # 跳过无效包
                            continue

                        # 写入 pcap
                        now = time.time()
                        ts_sec = int(now)
                        ts_usec = int((now - ts_sec) * 1000000)
                        write_pcap_packet(f, ieee_frame, ts_sec, ts_usec)
                        f.flush()

                        total_pkts += 1
                        hex_str = ' '.join(f'{b:02X}' for b in ieee_frame[:16])
                        elapsed = now - start_time
                        print(f"{elapsed:>8.3f} {total_pkts:>5} {len(ieee_frame):>5} {hex_str}")

                        pos = next_pos

                    # 移除已处理的数据
                    if pos > 0:
                        del rx_buf[:pos]

                # 定期报告
                now = time.time()
                if now - last_report > 5:
                    elapsed = now - start_time
                    rate = total_pkts / elapsed if elapsed > 0 else 0
                    sys.stdout.write(f"\r[统计] {elapsed:.0f}s | {total_pkts} 包 | {total_bytes} 字节 | {rate:.1f} 包/秒    \n")
                    sys.stdout.flush()
                    last_report = now

        except KeyboardInterrupt:
            pass
        finally:
            ser.close()

    elapsed = time.time() - start_time
    print()
    print("=" * 60)
    print(f"=== 抓包结束 ===")
    print(f"时长: {elapsed:.1f}s")
    print(f"包数: {total_pkts}")
    print(f"字节: {total_bytes}")
    print(f"文件: {output}")
    if total_pkts > 0:
        print(f"\n[OK] 抓包成功! 用 tshark 分析:")
        tshark = r"D:\Green\Wireshark 4.0.4 x64 Npcap1.50\App\Wireshark\tshark.exe"
        print(f'  "{tshark}" -r "{output}" -V')
        print(f"\n或用分析脚本:")
        print(f'  python D:\\VC\\Z-Stack\\tools\\analyze_capture.py "{output}"')
    else:
        print(f"\n[WARN] 未抓到包, 检查:")
        print(f"  1. sniffer 是否运行 (重新烧录固件)")
        print(f"  2. 通道是否正确 (z2m 使用的通道)")
        print(f"  3. 附近是否有 Zigbee 流量")

if __name__ == '__main__':
    main()
