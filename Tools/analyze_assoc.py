"""提取 Association Request/Response 的完整内容，分析 short_addr 分配机制"""
import struct, sys

path = sys.argv[1] if len(sys.argv) > 1 else "cap/烧录后未重新上电正常入网.pcap"

with open(path, 'rb') as f:
    f.read(24)
    pkts = []
    while True:
        h = f.read(16)
        if len(h) < 16: break
        ts_sec, ts_usec, caplen, origlen = struct.unpack('<IIII', h)
        data = f.read(caplen)
        pkts.append(data)

print(f"=== {path} ===")
print(f"总包数: {len(pkts)}\n")

# 找出所有 MAC cmd 包，打印完整内容
for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 3: continue  # MAC cmd

    pos = 3
    dam = (fc >> 10) & 0x03
    sam = (fc >> 14) & 0x03
    pan_comp = (fc >> 6) & 0x01
    seq = data[2]

    dst_pan = dst_addr = src_addr = None
    if dam >= 2:
        dst_pan = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
        if dam == 2:
            dst_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
        elif dam == 3:
            dst_addr = data[pos:pos+8]; pos += 8
    if sam >= 2:
        if not pan_comp: pos += 2
        if sam == 2:
            src_addr = struct.unpack('<H', data[pos:pos+2])[0]; pos += 2
        elif sam == 3:
            src_addr = data[pos:pos+8]; pos += 8

    if pos >= len(data): continue
    cmd_id = data[pos]
    cmd_name = {1:'AssocReq', 2:'AssocRsp', 4:'DataReq', 7:'BeaconReq'}.get(cmd_id, f'Cmd{cmd_id}')

    def fmt_addr(a):
        if a is None: return '?'
        if isinstance(a, int): return f'0x{a:04X}'
        return a.hex()

    print(f"#{i:3d} seq={seq:3d} {cmd_name:10s} src={fmt_addr(src_addr):20s} dst={fmt_addr(dst_addr):20s}", end='')

    if cmd_id == 0x01:  # Assoc Request
        # Capability Info
        if pos + 1 < len(data):
            cap = data[pos+1]
            dev_type = 'FFD' if cap & 0x01 else 'RFD'
            power = 'AC' if cap & 0x02 else 'Battery'
            rx_idle = 'On' if cap & 0x04 else 'Off'
            security = 'Yes' if cap & 0x08 else 'No'
            alloc_addr = 'Yes' if cap & 0x80 else 'No'
            print(f'  cap=[{dev_type},{power},idle={rx_idle},sec={security},allocAddr={alloc_addr}]', end='')
    elif cmd_id == 0x02:  # Assoc Response
        if pos + 3 < len(data):
            short_addr = struct.unpack('<H', data[pos+1:pos+3])[0]
            status = data[pos+3]
            status_str = {0:'Success', 1:'PAN full', 2:'PAN access denied', 3:'Frame too long', 4:'Not enough info'}.get(status, f'Err({status})')
            print(f'  short_addr=0x{short_addr:04X} status={status_str}', end='')

    print(f'  [raw={data.hex()}]')

# 找出 Beacon 帧看网络信息
print(f"\n--- Beacon 帧分析 ---")
for i, data in enumerate(pkts):
    if len(data) < 2: continue
    fc = struct.unpack('<H', data[:2])[0]
    ftype = fc & 0x07
    if ftype != 0: continue  # Beacon

    # Beacon: FC(2) + Seq(1) + SrcPan(2) + SrcAddr(2/8) + ...
    seq = data[2]
    src_pan = struct.unpack('<H', data[3:5])[0]
    sam = (fc >> 14) & 0x03
    if sam == 2:
        src_addr = struct.unpack('<H', data[5:7])[0]
        pos = 7
    elif sam == 3:
        src_addr = data[5:13]
        pos = 13
    else:
        src_addr = '?'; pos = 5

    # Superframe spec (2 bytes)
    sf = struct.unpack('<H', data[pos:pos+2])[0]
    pos += 2
    # GTS fields
    gts_perm = (sf >> 5) & 0x01
    beacon_order = sf & 0x0F
    sf_order = (sf >> 4) & 0x0F
    # PAN coordinator
    pan_coord = (sf >> 14) & 0x01

    def fmt(a):
        if isinstance(a, int): return f'0x{a:04X}'
        return a.hex() if isinstance(a, bytes) else str(a)

    print(f"#{i:3d} Beacon src={fmt(src_addr)} pan=0x{src_pan:04X} pan_coord={pan_coord} BO={beacon_order} SO={sf_order}")
