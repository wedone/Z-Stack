#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
HGZBSwitch 固件烧录脚本

流程:
  1. 读取 IAR 编译输出的 .hex, 转换为 256KB .bin (CC2530F256)
  2. 上传 .bin 到 CCLoader (ESP8266 无线烧录器)
  3. 发起异步烧录 (强制校验)
  4. 轮询烧录进度直到完成

用法:
  python tools/burn.py
  python tools/burn.py --ip 10.0.0.147
  python tools/burn.py --hex Projects/.../HGZBSwitch.hex
"""
import argparse
import os
import sys
import time

import requests

DEFAULT_IP = "10.0.0.147"
# 相对于项目根目录 (tools/ 的上级)
DEFAULT_HEX = os.path.join("Projects", "zstack", "HomeAutomation", "HGZBSwitch", "CC2530DB", "RouterEB", "Exe", "HGZBSwitch.hex")
REMOTE_BIN_NAME = "HGZBSwitch.bin"
CC2530_FLASH_SIZE = 256 * 1024  # CC2530F256 = 256KB
TIMEOUT = 10


def hex2bin(hex_path, bin_path, fill=0xFF):
    """Intel HEX -> BIN, 填充到 CC2530_FLASH_SIZE.
    支持: 00 数据, 01 EOF, 04 扩展线性地址."""
    flash = bytearray([fill] * CC2530_FLASH_SIZE)
    base = 0
    written = 0
    with open(hex_path, "r", encoding="ascii") as f:
        for line_no, raw in enumerate(f, 1):
            line = raw.strip()
            if not line or not line.startswith(":"):
                continue
            # :LLAAAATTDD...DCC
            try:
                data = bytes.fromhex(line[1:])
            except ValueError:
                print(f"[ERROR] 第 {line_no} 行 hex 解析失败: {raw!r}")
                return False
            if len(data) < 5:
                continue
            rec_len = data[0]
            addr = (data[1] << 8) | data[2]
            rec_type = data[3]
            payload = data[4:4 + rec_len]
            # 校验和 (整行字节和的最低字节 == 0)
            if (sum(data) & 0xFF) != 0:
                print(f"[ERROR] 第 {line_no} 行校验和错误")
                return False
            if rec_type == 0x00:  # 数据
                abs_addr = base + addr
                if abs_addr + rec_len > CC2530_FLASH_SIZE:
                    print(f"[ERROR] 第 {line_no} 行地址 0x{abs_addr:X} 超出 256KB 范围")
                    return False
                flash[abs_addr:abs_addr + rec_len] = payload
                written = max(written, abs_addr + rec_len)
            elif rec_type == 0x01:  # EOF
                break
            elif rec_type == 0x04:  # 扩展线性地址
                base = (payload[0] << 8) | payload[1]
                base <<= 16
            # 其他类型忽略
    with open(bin_path, "wb") as f:
        f.write(flash)
    print(f"[hex2bin] {hex_path} -> {bin_path} ({written} 字节有效, 填充到 {CC2530_FLASH_SIZE} 字节)")
    return True


def check_device(ip):
    """检查 CCLoader 设备就绪"""
    r = requests.get(f"http://{ip}/api/status", timeout=TIMEOUT)
    r.raise_for_status()
    s = r.json()
    if s["state"] != "idle":
        print(f"[ERROR] 设备非 idle: state={s['state']}")
        return None
    return s


def upload_bin(ip, bin_path, remote_name):
    """上传 .bin 到 CCLoader LittleFS"""
    with open(bin_path, "rb") as f:
        r = requests.post(f"http://{ip}/api/upload",
                          files={"file": (remote_name, f)},
                          timeout=60)
    r.raise_for_status()
    d = r.json()
    if not d.get("success"):
        print(f"[ERROR] 上传失败: {d}")
        return False
    print(f"[upload] {remote_name} ({d.get('size')} 字节) -> OK")
    return True


def burn_async(ip, filename):
    """发起异步烧录, 返回 (task_id, total_blocks)"""
    r = requests.post(f"http://{ip}/api/burn",
                      json={"filename": filename, "verify": True},
                      timeout=TIMEOUT)
    if r.status_code == 409:
        print(f"[ERROR] 设备忙: {r.json()}")
        return None
    if r.status_code == 404:
        print(f"[ERROR] 文件不存在: {r.json()}")
        return None
    r.raise_for_status()
    d = r.json()
    print(f"[burn] task_id={d.get('task_id')} total_blocks={d.get('total_blocks')} verify_forced={d.get('verify_forced')}")
    return d.get("task_id"), d.get("total_blocks", 0)


def poll_progress(ip, deadline_s=300):
    """轮询烧录进度"""
    start = time.time()
    last_pct = -1
    while time.time() - start < deadline_s:
        try:
            s = requests.get(f"http://{ip}/api/status", timeout=TIMEOUT).json()
        except Exception as e:
            print(f"  [warn] 轮询失败: {e}")
            time.sleep(2)
            continue
        b = s.get("burn", {})
        pct = b.get("percent", 0)
        cur = b.get("current_block", 0)
        tot = b.get("total_blocks", 0)
        done = b.get("done", False)
        err = b.get("error", "")
        if pct != last_pct:
            print(f"  [{time.time()-start:5.1f}s] {pct:3d}%  {cur}/{tot}  err={err!r}")
            last_pct = pct
        if done:
            return err
        time.sleep(1)
    return "timeout"


def main():
    ap = argparse.ArgumentParser(description="HGZBSwitch 固件烧录 (hex->bin->upload->burn)")
    ap.add_argument("--ip", default=DEFAULT_IP, help=f"CCLoader IP (默认 {DEFAULT_IP})")
    ap.add_argument("--hex", default=DEFAULT_HEX, help=f"hex 文件路径 (默认 {DEFAULT_HEX})")
    ap.add_argument("--remote", default=REMOTE_BIN_NAME, help=f"上传后的文件名 (默认 {REMOTE_BIN_NAME})")
    ap.add_argument("--no-burn", action="store_true", help="仅转换+上传, 不烧录")
    args = ap.parse_args()

    # 项目根目录 = tools/ 的上级目录
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    hex_path = args.hex if os.path.isabs(args.hex) else os.path.join(project_root, args.hex)
    bin_path = os.path.join(project_root, "tools", "HGZBSwitch_burn.bin")

    # 1. hex -> bin
    print(f"\n=== 步骤 1: hex2bin 转换 ===")
    if not os.path.exists(hex_path):
        print(f"[ERROR] hex 文件不存在: {hex_path}")
        sys.exit(1)
    if not hex2bin(hex_path, bin_path):
        sys.exit(1)

    # 2. 检查设备
    print(f"\n=== 步骤 2: 检查 CCLoader 设备 ({args.ip}) ===")
    try:
        s = check_device(args.ip)
    except Exception as e:
        print(f"[ERROR] 无法连接 CCLoader: {e}")
        sys.exit(1)
    if s is None:
        sys.exit(1)
    print(f"  state={s['state']} ip={s['wifi']['ip']} rssi={s['wifi']['rssi']} uptime={s['uptime']}s")

    # 3. 上传 bin
    print(f"\n=== 步骤 3: 上传固件 ===")
    if not upload_bin(args.ip, bin_path, args.remote):
        sys.exit(1)

    if args.no_burn:
        print(f"\n[--no-burn] 已上传, 跳过烧录")
        return

    # 4. 发起烧录
    print(f"\n=== 步骤 4: 发起异步烧录 ===")
    info = burn_async(args.ip, args.remote)
    if info is None:
        sys.exit(1)

    # 5. 轮询进度
    print(f"\n=== 步骤 5: 烧录进度 ===")
    err = poll_progress(args.ip)
    if err:
        print(f"\n[FAIL] 烧录失败: {err}")
        sys.exit(1)
    print(f"\n[OK] 烧录成功! 固件已写入 CC2530")


if __name__ == "__main__":
    main()
