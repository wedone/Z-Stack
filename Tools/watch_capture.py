"""
监控 cap 目录, 自动分析新抓包文件
用法: python watch_capture.py
"""
import os
import sys
import time
import subprocess

CAP_DIR = r"D:\VC\Z-Stack\cap"
TSHARK = r"D:\Green\Wireshark 4.0.4 x64 Npcap1.50\App\Wireshark\tshark.exe"
ANALYZER = os.path.join(os.path.dirname(__file__), "analyze_capture.py")

def get_files():
    """获取目录中所有 pcapng/pcap 文件"""
    if not os.path.exists(CAP_DIR):
        return []
    return {f: os.path.getmtime(os.path.join(CAP_DIR, f))
            for f in os.listdir(CAP_DIR)
            if f.endswith(('.pcapng', '.pcap'))}

def main():
    print(f"=== 监控 {CAP_DIR} ===")
    print(f"在 Wireshark 中保存抓包文件到这个目录, 会自动分析")
    print(f"按 Ctrl+C 停止")
    print()

    # 记录初始文件
    last_files = get_files()
    if last_files:
        print(f"[初始] 已有 {len(last_files)} 个文件:")
        for f in last_files:
            print(f"  - {f}")

    print()
    print("[等待新文件...]")

    try:
        while True:
            time.sleep(2)
            current = get_files()

            # 检测新文件
            new_files = set(current.keys()) - set(last_files.keys())
            # 检测修改的文件 (Wireshark 保存时可能先创建临时文件)
            modified = {f for f in current if f in last_files and current[f] > last_files[f] + 1}

            for f in new_files | modified:
                path = os.path.join(CAP_DIR, f)
                # 等待文件写入完成
                size1 = os.path.getsize(path)
                time.sleep(1)
                size2 = os.path.getsize(path)
                if size1 != size2:
                    continue  # 还在写入

                if size2 < 100:
                    continue  # 太小,跳过

                print()
                print(f"[检测到] {f} ({size2/1024:.1f} KB)")
                print("-" * 60)

                # 运行分析
                try:
                    result = subprocess.run(
                        [sys.executable, ANALYZER, path],
                        capture_output=False, timeout=120
                    )
                except subprocess.TimeoutExpired:
                    print("[ERROR] 分析超时")
                except Exception as e:
                    print(f"[ERROR] {e}")

                print()
                print("[等待新文件...]")

            last_files = current

    except KeyboardInterrupt:
        print()
        print("[停止监控]")

if __name__ == '__main__':
    main()
