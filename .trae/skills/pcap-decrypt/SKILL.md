---
name: "pcap-decrypt"
description: "使用 Wireshark/tshark 对 pcap 文件进行 ZigBee NWK 层解密，还原 ZCL/ZDP 应用层消息。当用户要求解密 pcap、解码加密数据、分析 ZigBee 应用层时调用。"
---

# ZigBee PCAP 解密 Skill

使用 Wireshark 或 tshark 对 CCLoader sniffer 抓取的 pcap 文件进行 NWK 层解密，还原 ZCL/ZDP 应用层消息。

## 解密所需信息

| 项 | 值 | 说明 |
|----|-----|------|
| Network Key（NWK Key） | `01030507090B0D0F00020406080A0C0C` | 16 字节 hex 字符串 |
| Security Level | `AES-128 Encryption, 32-bit Integrity Protection` | NWK 层安全级别（加密 + 32bit MIC） |
| Byte Order | `Normal` | 密钥字节序，默认 Normal（不交换） |

> Security Level 中 `32-bit` 必须带连字符，否则 Wireshark 无法匹配。

## 方式一：Wireshark 图形界面配置

1. 打开 pcap 文件
2. `编辑` → `首选项`（`Ctrl+Shift+P`）
3. 左侧 `Protocols` → `ZigBee Network Layer`
4. `Security Level` 下拉选择 `AES-128 Encryption, 32-bit Integrity Protection`
5. `Pre-configured Keys` → 点击 `Edit...` → `+` 新增一行：
   - **Key**：`01030507090B0D0F00020406080A0C0C`
   - **Byte order**：`Normal`
   - **Label**：（留空）
6. 点击 `OK` 保存，Wireshark 立即重新解析

### 持久化

密钥保存到 `zigbee_pc_keys` 文件，下次自动加载：

```
<Wireshark 安装目录>\Data\zigbee_pc_keys
```

内容格式：

```
"01030507090B0D0F00020406080A0C0C","Normal",""
```

## 方式二：tshark 命令行解密（推荐脚本使用）

### 查看解密后的包列表

```powershell
$tshark = "D:\Green\Wireshark 4.4.7 x64 Npcap1.50 mod\App\Wireshark\tshark.exe"
$pcap   = "cap/sniffer_20250101_120000.pcap"

& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0C","Normal",""' `
    -Y "zbee_nwk" -c 10
```

### 协议层级统计（验证解密是否成功）

```powershell
& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0C","Normal",""' `
    -z io,phs
```

成功时输出应包含 `zbee_aps` → `zbee_zcl` / `zbee_zdp` 子层：

```
  zbee_nwk    frames:593 bytes:30065
    zbee_aps  frames:289 bytes:14452
      zbee_zcl frames:246 bytes:12214
      zbee_zdp frames:39  bytes:2008
```

### 提取应用层字段

```powershell
& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0C","Normal",""' `
    -Y "zbee.zcl.frame_type == 0x01" `
    -T fields `
    -e frame.number -e zbee.nwk.src -e zbee.zcl.seqno -e zbee.zcl.attr_id -e zbee.zcl.attr_val
```

## 验证方法

| 现象 | 含义 |
|------|------|
| 包列表出现 `ZCL: Read Attributes` / `ZDP: Device Announcement` | 解密成功 |
| 协议统计 `zbee_zcl` / `zbee_zdp` 子层有帧数 | 解密成功 |
| 包列表只显示 `ZigBee 60 Command` / `Encrypted Payload` | 未解密（密钥或 Security Level 不对） |
| 出现 `Malformed Packet`（NWK 帧长度对不上） | 抓包脚本产出的 802.15.4 帧格式有误 |

## 常见问题

### Q1：配置了密钥还是解密不了

排查顺序：
1. 确认 Security Level 拼写：`AES-128 Encryption, 32-bit Integrity Protection`（`32-bit` 带连字符）
2. 确认密钥是 32 个 hex 字符（16 字节）
3. 确认 pcap 的 DLT 类型为 `230`（IEEE 802.15.4 no FCS）
4. 确认抓包脚本已正确移除 ZBOSS payload 末尾的 LQI + CRC 状态 2 字节

### Q2：tshark 报错 `Invalid format: "uat:..."`

PowerShell 中推荐用**单引号**包裹整个 `-o` 参数值，内部双引号不转义：

```powershell
-o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0C","Normal",""'
```

cmd.exe 中则用：

```cmd
-o "uat:zigbee_pc_keys:\"01030507090B0D0F00020406080A0C0C\",\"Normal\",\"\""
```

### Q3：多个网络/不同密钥怎么切换

Wireshark 的 `Pre-configured Keys` 表支持多行，会自动尝试所有密钥解密。如需隔离，使用 Wireshark Profile（`配置` → `新建配置`），每个 Profile 维护独立的 `zigbee_pc_keys` 文件。

## 参考路径

- tshark 可执行文件：`D:\Green\Wireshark 4.4.7 x64 Npcap1.50 mod\App\Wireshark\tshark.exe`
- 抓包脚本：`Tools/sniffer_via_ccloader.py`
- 相关文档：`NewProject-Docs/06-参考/Zigbee_PCAP解密配置.md`