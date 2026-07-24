#!/usr/bin/env python3
"""
Intel HEX 转 BIN 工具 (CCLoader 烧录专用)

CCLoader API 仅接受 .bin, 上传 .hex 会被拒绝。
本脚本将 Intel HEX 格式转换为 BIN, 缺失地址填 0xFF, 尾部填充到 256KB。

用法:
  python hex2bin.py input.hex [output.bin]

不指定 output 时默认输出同名 .bin 文件。

合并 OTA 分体固件 (Bootloader + 应用):
  python hex2bin.py Boot.hex App.hex output.bin
"""

import sys


def parse_hex(hex_path, data_map, base_addr_ref):
    """解析单个 Intel HEX 文件, 写入 data_map (phys_addr -> byte)"""
    base_addr = base_addr_ref[0]
    min_addr = data_map.get('_min')
    max_addr = data_map.get('_max')

    with open(hex_path, 'r') as f:
        for line_no, line in enumerate(f, 1):
            line = line.strip()
            if not line or line[0] != ':':
                continue
            raw = bytes.fromhex(line[1:])
            if len(raw) < 5:
                continue
            b_count = raw[0]
            b_addr = (raw[1] << 8) | raw[2]
            b_type = raw[3]
            data = raw[4:4 + b_count]

            # 校验和验证: 所有字节(含校验和字节)之和的低字节应为0
            if (sum(raw) & 0xFF) != 0:
                raise ValueError(f'{hex_path} line {line_no}: checksum error')

            if b_type == 0x01:       # 结束记录
                break
            elif b_type == 0x02:     # 扩展段地址
                base_addr = ((data[0] << 8) | data[1]) << 4
            elif b_type == 0x04:     # 扩展线性地址
                base_addr = ((data[0] << 8) | data[1]) << 16
            elif b_type == 0x05:     # 起始地址, 忽略
                continue
            elif b_type == 0x00:     # 数据记录
                phys = base_addr + b_addr
                for i, b in enumerate(data):
                    data_map[phys + i] = b
                if min_addr is None or phys < min_addr:
                    min_addr = phys
                end = phys + len(data) - 1
                if max_addr is None or end > max_addr:
                    max_addr = end

    base_addr_ref[0] = base_addr
    data_map['_min'] = min_addr
    data_map['_max'] = max_addr


def hex2bin(hex_paths, out_path, pad_to=0x40000):
    """将一个或多个 HEX 文件转为 BIN, 填充到 pad_to 字节 (默认 256KB)"""
    data_map = {}
    base_addr_ref = [0]  # 用列表包装以便在函数间共享可变值

    for hex_path in hex_paths:
        parse_hex(hex_path, data_map, base_addr_ref)

    min_addr = data_map.get('_min')
    max_addr = data_map.get('_max')
    if min_addr is None:
        raise ValueError('HEX 无数据记录')

    # 构造 BIN, 缺失地址填 0xFF
    bin_size = max(max_addr + 1, pad_to)
    bin_data = bytearray([0xFF] * bin_size)
    for addr, b in data_map.items():
        if addr == '_min' or addr == '_max':
            continue
        if addr < bin_size:
            bin_data[addr] = b

    with open(out_path, 'wb') as f:
        f.write(bin_data)

    print(f'转换完成: {", ".join(hex_paths)} -> {out_path}')
    print(f'  数据范围: 0x{min_addr:04X} ~ 0x{max_addr:04X}')
    print(f'  输出大小: {bin_size} 字节 ({bin_size // 1024}KB)')
    return bin_data


def main():
    args = sys.argv[1:]
    if len(args) < 1:
        print(__doc__)
        sys.exit(1)

    # 最后一个 .bin 参数视为输出文件
    if args[-1].endswith('.bin') and len(args) >= 2:
        out_path = args[-1]
        hex_paths = args[:-1]
    else:
        # 默认输出: 第一个 hex 文件同名 .bin
        hex_paths = args
        out_path = hex_paths[0].rsplit('.', 1)[0] + '.bin'

    hex2bin(hex_paths, out_path)


if __name__ == '__main__':
    main()
