"""
Zigbee 抓包分析脚本
用法:
  python analyze_capture.py <pcapng文件>
  python analyze_capture.py D:\VC\Z-Stack\cap\capture.pcapng

功能:
  1. 用 tshark 解析 pcapng 文件
  2. 统计包类型、设备地址
  3. 提取 ZCL 命令、重传、延迟
  4. 输出结构化分析报告
"""
import sys
import os
import subprocess
import json
from collections import Counter, defaultdict

TSHARK = r"D:\Green\Wireshark 4.0.4 x64 Npcap1.50\App\Wireshark\tshark.exe"

def run_tshark(pcapng_path, fields=None, read_filter=None, display_filter=None):
    """运行 tshark 获取数据"""
    cmd = [TSHARK, "-r", pcapng_path, "-T", "json"]
    if fields:
        for f in fields:
            cmd.extend(["-e", f])
    if read_filter:
        cmd.extend(["-Y", read_filter])
    if display_filter:
        cmd.extend(["-Y", display_filter])

    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60, encoding="utf-8", errors="replace")
        if result.returncode != 0:
            print(f"[tshark 错误] {result.stderr[:500]}")
            return []
        # tshark json 输出是数组,每行一个 json 对象 (当用 -T json 时)
        try:
            return json.loads(result.stdout)
        except json.JSONDecodeError:
            # 可能是 jsonlines
            packets = []
            for line in result.stdout.strip().split('\n'):
                line = line.strip().rstrip(',')
                if line.startswith('{'):
                    try:
                        packets.append(json.loads(line))
                    except: pass
            return packets
    except subprocess.TimeoutExpired:
        print("[ERROR] tshark 超时")
        return []
    except Exception as e:
        print(f"[ERROR] {e}")
        return []

def analyze(pcapng_path):
    """分析抓包文件"""
    if not os.path.exists(pcapng_path):
        print(f"[ERROR] 文件不存在: {pcapng_path}")
        return

    size = os.path.getsize(pcapng_path)
    print(f"=== 抓包分析: {os.path.basename(pcapng_path)} ===")
    print(f"文件大小: {size/1024:.1f} KB")
    print()

    # 1. 基本信息 - 包总数
    print("--- 基本统计 ---")
    packets = run_tshark(pcapng_path)
    total = len(packets)
    print(f"总包数: {total}")

    if total == 0:
        print("[WARN] 无数据包,可能是文件格式问题或抓包失败")
        return

    # 2. 帧类型统计
    print()
    print("--- 帧类型统计 ---")
    frame_types = Counter()
    ieee_types = Counter()
    zbee_types = Counter()
    zcl_cmds = Counter()
    src_dst = Counter()
    pan_ids = Counter()
    channels = Counter()

    for pkt in packets:
        layers = pkt.get("_source", {}).get("layers", {})
        if "frame" in layers:
            ft = layers["frame"].get("frame.type", "?")
            frame_types[ft] += 1
        if "wpan" in layers:
            ftype = layers["wpan"].get("wpan.frame_type", "?")
            ieee_types[ftype] += 1
            src = layers["wpan"].get("wpan.src64", layers["wpan"].get("wpan.src16", "?"))
            dst = layers["wpan"].get("wpan.dst64", layers["wpan"].get("wpan.dst16", "?"))
            pan = layers["wpan"].get("wpan.pan_id", "?")
            ch = layers["wpan"].get("wpan.channel", "?")
            src_dst[f"{src} -> {dst}"] += 1
            pan_ids[pan] += 1
            channels[ch] += 1
        if "zbee_nwk" in layers:
            zt = layers["zbee_nwk"].get("zbee.nwk.frame_type", "?")
            zbee_types[zt] += 1
        if "zbee_zcl" in layers:
            cmd = layers["zbee_zcl"].get("zbee.zcl.cluster.id", "?") + "/" + layers["zbee_zcl"].get("zbee.zcl.command.id", "?")
            zcl_cmds[cmd] += 1

    print(f"IEEE 802.15.4 帧类型: {dict(ieee_types)}")
    print(f"PAN ID: {dict(pan_ids)}")
    print(f"通道: {dict(channels)}")
    print(f"ZigBee NWK 帧类型: {dict(zbee_types)}")
    print(f"ZCL 命令 (cluster/cmd): {dict(zcl_cmds)}")

    # 3. 通信链路
    print()
    print("--- 通信链路 (前 10) ---")
    for link, count in src_dst.most_common(10):
        print(f"  {count:4d}  {link}")

    # 4. 重传统计 (同源同序号重复)
    print()
    print("--- 重传分析 ---")
    seq_counter = Counter()
    for pkt in packets:
        layers = pkt.get("_source", {}).get("layers", {})
        if "wpan" in layers:
            src = layers["wpan"].get("wpan.src64", layers["wpan"].get("wpan.src16", "?"))
            seq = layers["wpan"].get("wpan.seq_no", "?")
            if src != "?" and seq != "?":
                seq_counter[(src, seq)] += 1
    retransmits = sum(1 for v in seq_counter.values() if v > 1)
    retransmit_pkts = sum(v - 1 for v in seq_counter.values() if v > 1)
    print(f"重复序号数: {retransmits}")
    print(f"重传包数: {retransmit_pkts}")
    if retransmits > 0:
        print(f"重传率: {retransmit_pkts/total*100:.1f}%")

    # 5. 时间分析
    print()
    print("--- 时间分析 ---")
    if total >= 2:
        first_ts = float(packets[0].get("_source",{}).get("layers",{}).get("frame",{}).get("frame.time_epoch","0"))
        last_ts = float(packets[-1].get("_source",{}).get("layers",{}).get("frame",{}).get("frame.time_epoch","0"))
        duration = last_ts - first_ts
        print(f"抓包时长: {duration:.2f}s")
        print(f"平均速率: {total/duration:.1f} 包/秒")

    # 6. 详细包列表 (前 30)
    print()
    print("--- 包列表 (前 30) ---")
    print(f"{'#':>4} {'时间':>8} {'源':>20} {'目的':>20} {'类型':<15} {'详情'}")
    for i, pkt in enumerate(packets[:30], 1):
        layers = pkt.get("_source", {}).get("layers", {})
        ts = float(layers.get("frame",{}).get("frame.time_relative","0"))
        src = "?"
        dst = "?"
        ftype = "?"
        detail = ""
        if "wpan" in layers:
            src = layers["wpan"].get("wpan.src64", layers["wpan"].get("wpan.src16", "?"))
            dst = layers["wpan"].get("wpan.dst64", layers["wpan"].get("wpan.dst16", "?"))
            ftype = layers["wpan"].get("wpan.frame_type", "?")
        if "zbee_zcl" in layers:
            cluster = layers["zbee_zcl"].get("zbee.zcl.cluster.id", "?")
            cmd = layers["zbee_zcl"].get("zbee.zcl.command.id", "?")
            detail = f"ZCL {cluster}/{cmd}"
        elif "zbee_aps" in layers:
            detail = "APS " + layers["zbee_aps"].get("zbee.aps.cluster","?")
        print(f"{i:>4} {ts:>8.3f} {str(src)[:20]:>20} {str(dst)[:20]:>20} {str(ftype)[:15]:<15} {detail}")

    if total > 30:
        print(f"... 还有 {total-30} 个包")

    print()
    print("=== 分析完成 ===")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        # 自动检测 cap 目录最新文件
        cap_dir = r"D:\VC\Z-Stack\cap"
        if os.path.exists(cap_dir):
            files = [os.path.join(cap_dir, f) for f in os.listdir(cap_dir)
                     if f.endswith(('.pcapng', '.pcap'))]
            if files:
                latest = max(files, key=os.path.getmtime)
                print(f"[自动检测] 最新文件: {latest}")
                analyze(latest)
            else:
                print(f"用法: python {sys.argv[0]} <pcapng文件>")
                print(f"cap 目录为空,请先保存抓包文件到 {cap_dir}")
        else:
            print(f"用法: python {sys.argv[0]} <pcapng文件>")
    else:
        analyze(sys.argv[1])
