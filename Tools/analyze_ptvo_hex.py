"""分析 ptvo cc2530 hex/bin 文件，找 Z-Stack 版本字符串和协议栈特征"""
import sys, re

path = sys.argv[1] if len(sys.argv) > 1 else "cap/ptvo_cc2530.bin"

with open(path, 'rb') as f:
    data = f.read()

print(f"=== {path} ===")
print(f"文件大小: {len(data)} 字节\n")

# 搜索常见 Z-Stack 版本字符串
VERSION_PATTERNS = [
    b'Z-Stack',
    b'ZStack',
    b'zstack',
    b'Z-Stack 1.2',
    b'Z-Stack 2.5',
    b'Z-Stack 3.0',
    b'1.2.2a',
    b'2.5.1a',
    b'3.0.1',
    b'3.0.2',
    b'3.0.x',
    b'ZCL30',
    b'ZCL25',
    b'ZCL12',
    b'ptvo',
    b'PTVO',
    b'Texas Instruments',
    b'TI-IEEE',
]

print("--- 搜索版本字符串 ---")
for pat in VERSION_PATTERNS:
    idx = 0
    while True:
        idx = data.find(pat, idx)
        if idx < 0: break
        # 提取上下文
        start = max(0, idx - 8)
        end = min(len(data), idx + len(pat) + 40)
        context = data[start:end]
        # 只打印可打印字符
        printable = ''.join(chr(b) if 32 <= b < 127 else '.' for b in context)
        print(f"  0x{idx:05X}: {printable}")
        idx += len(pat)

# 搜索所有可打印 ASCII 字符串（>=6字符）
print("\n--- 所有可打印字符串 (>=8字符, 可能含版本信息) ---")
strings_found = []
current = []
start_pos = 0
for i, b in enumerate(data):
    if 32 <= b < 127:
        if not current:
            start_pos = i
        current.append(chr(b))
    else:
        if len(current) >= 8:
            s = ''.join(current)
            strings_found.append((start_pos, s))
        current = []
if len(current) >= 8:
    strings_found.append((start_pos, ''.join(current)))

# 过滤有意义的字符串
MEANINGFUL_PATTERNS = [
    r'Z-?Stack', r'ZCL', r'ptvo', r'PTVO', r'cc2530', r'CC2530',
    r'1\.2\.', r'2\.5\.', r'3\.0\.', r'version', r'VERSION',
    r'Build', r'BUILD', r'Date', r'DATE',
    r'router', r'Router', r'ROUTER',
    r'ZigBee', r'zigbee', r'ZIGBEE',
    r'Home', r'HOME',
    r'201[0-9]', r'202[0-9]',  # 年份
    r'vw', r'VW',
]

for pos, s in strings_found:
    for pat in MEANINGFUL_PATTERNS:
        if re.search(pat, s):
            print(f"  0x{pos:05X}: {s}")
            break

# 搜索 ptvo 相关字符串
print("\n--- ptvo 相关字符串 ---")
for pos, s in strings_found:
    if 'ptvo' in s.lower() or 'PTVO' in s:
        print(f"  0x{pos:05X}: {s}")

# Z-Stack 协议栈版本特征：搜索 ZDApp 版本字符串
print("\n--- Z-Stack 关键版本标识 ---")
ZSTACK_MARKERS = [
    b'Z-Stack',
    b'ZStack',
    b'ZDApp',
    b'ZGlobals',
    b'ZDSecMgr',
    b'bdb_',
    b'BDB_',
]
for marker in ZSTACK_MARKERS:
    idx = 0
    count = 0
    while True:
        idx = data.find(marker, idx)
        if idx < 0: break
        if count < 3:  # 每个标记最多打印3次
            start = max(0, idx - 4)
            end = min(len(data), idx + len(marker) + 30)
            ctx = data[start:end]
            printable = ''.join(chr(b) if 32 <= b < 127 else '.' for b in ctx)
            print(f"  0x{idx:05X} '{marker.decode()}': {printable}")
        idx += len(marker)
        count += 1
    if count > 0:
        print(f"  总计 {count} 处")
