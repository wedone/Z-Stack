# ZigBee PCAP 解密配置

## 文档用途

CCLoader sniffer 抓包产出的 `.pcap` 文件中包含 ZigBee NWK 层加密帧，需要正确的 Network Key 和 Security Level 才能解密查看应用层（ZCL/ZDP）内容。本文档说明 Wireshark 图形界面和 Tshark 命令行两种解密配置方法。

> 相关背景：抓包脚本 [sniffer_via_ccloader.py](../../Tools/sniffer_via_ccloader.py) 已修复 ZBOSS sniffer 固件 payload 格式（移除末尾 LQI + CRC 状态字节），产出的 pcap 与 ZBOSS 官方 GUI 抓包格式一致，可用本配置正常解密。

## 解密所需信息

| 项 | 值 | 说明 |
|----|-----|------|
| Network Key（NWK Key） | `01030507090B0D0F00020406080A0C0D` | 16 字节 hex 字符串（ZigBee 默认测试密钥） |
| Security Level | `AES-128 Encryption, 32-bit Integrity Protection` | NWK 层安全级别（加密 + 32bit MIC） |
| Byte Order | `Normal` | 密钥字节序，默认 Normal（不交换） |
| Label | （空） | 用户自定义标签，可选 |

> **注意拼写**：Security Level 选项中 `32-bit` 必须带连字符，否则 Wireshark 无法匹配，配置不生效。

## Wireshark 图形界面配置

### 步骤

1. **打开首选项**：菜单栏 `编辑` → `首选项`（或快捷键 `Ctrl+Shift+P`）
2. **定位协议**：左侧树展开 `Protocols` → 找到 `ZigBee Network Layer`
   - 也可在首选项顶部的过滤输入框输入 `zigbee` 快速定位
3. **设置 Security Level**：
   - 找到 `Security Level` 下拉框
   - 选择 `AES-128 Encryption, 32-bit Integrity Protection`
4. **添加 Pre-configured Key**：
   - 找到 `Pre-configured Keys` 表格，点击右侧 `Edit...`（或 `+` 按钮）
   - 弹出编辑窗口后点击 `+` 新增一行，依次填入：
     - **Key**：`01030507090B0D0F00020406080A0C0D`
     - **Byte order**：`Normal`
     - **Label**：（留空或填备注，如 `客厅开关网络`）
   - 点击 `OK` 保存
5. **应用并关闭**：点击首选项窗口的 `OK`，Wireshark 立即重新解析当前 pcap

### 验证

配置正确后，原本显示为 `ZigBee 60 Command` / `Encrypted Payload` 的包会变成可读的应用层消息，例如：
- `ZigBee HA 52 ZCL: Read Attributes, Seq: 198`
- `ZigBee ZDP 55 Device Announcement, Nwk Addr: 0x0297`
- `ZigBee HA 50 ZCL: Report Attributes, Seq: 167`

如果仍然显示 `Encrypted Payload` 或 `Malformed Packet`，说明密钥/Security Level 不对，或抓包脚本产出的帧格式有误。

### 持久化

Wireshark 会把密钥保存到个人配置目录下的 `zigbee_pc_keys` 文件，下次启动自动加载，无需重复配置。 portable 版本路径示例：

```
<Wireshark 安装目录>\Data\zigbee_pc_keys
```

文件内容格式（每行一个密钥）：

```
"01030507090B0D0F00020406080A0C0D","Normal",""
```

## Tshark 命令行配置

Tshark 通过 `-o` 参数覆盖首选项，无需 GUI 即可解密。两个关键参数：

| 参数 | 作用 |
|------|------|
| `-o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection"` | 设置 NWK 层 Security Level |
| `-o "uat:zigbee_pc_keys:\"<KEY>\",\"Normal\",\"\""` | 添加 Pre-configured Key（UAT 表） |

`uat:zigbee_pc_keys` 是 Wireshark 的 User Access Table，三列分别为 `Key`、`Byte order`、`Label`，列内字符串需用双引号包裹。

### 基础示例：查看解密后的包列表

```powershell
$tshark = "D:\Green\Wireshark 4.4.7 x64 Npcap1.50 mod\App\Wireshark\tshark.exe"
$pcap   = "d:\VC\CCLoader\cap\fix_test_2min.pcap"

& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0D","Normal",""' `
    -Y "zbee_nwk" `
    -c 10
```

输出示例（看到 `ZCL` / `ZDP` 字样即解密成功）：

```
    3   1.623404       0x0000 → Broadcast    ZigBee 60 Link Status
    7   3.235095       0x0000 → 0x434c       ZigBee HA 52 ZCL: Read Attributes, Seq: 198
   10   3.235428       0x0000 → 0x434c       ZigBee HA 52 ZCL: Read Attributes, Seq: 198
```

### 协议层级统计：对比解密前后

```powershell
& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0D","Normal",""' `
    -z io,phs
```

正常解密后统计中应出现 `zbee_zcl` 和 `zbee_zdp` 子层，例如：

```
  zbee_nwk                               frames:593 bytes:30065
    zbee_aps                             frames:289 bytes:14452
      zbee_zcl                           frames:246 bytes:12214
      zbee_zdp                           frames:39  bytes:2008
```

若只有 `zbee_nwk` 而没有 `zbee_aps` 子层，说明解密未生效。

### 导出解密后的应用层字段

提取所有 ZCL Report Attributes 的属性值（用于自动化分析设备上报数据）：

```powershell
& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0D","Normal",""' `
    -Y "zbee.zcl.frame_type == 0x01" `
    -T fields `
    -e frame.number -e zbee.nwk.src -e zbee.zcl.seqno -e zbee.zcl.attr_id -e zbee.zcl.attr_val
```

### 导出解密后的完整 pcap（脱敏用）

```powershell
& $tshark -r $pcap `
    -o "zbee_nwk.seclevel:AES-128 Encryption, 32-bit Integrity Protection" `
    -o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0D","Normal",""' `
    -w decrypted.pcap
```

> 注意：`-w` 导出的 pcap 仍然包含原始加密帧，但在 Wireshark/tshark 重新打开时会自动用配置的密钥解密显示。Wireshark 默认不会把解密后的明文写入 pcap（这是协议特性，不是 bug）。

## 验证方法速查

| 现象 | 含义 |
|------|------|
| 包列表出现 `ZCL: Read Attributes` / `ZDP: Device Announcement` 等应用层消息 | 解密成功 |
| 协议统计 `zbee_zcl` / `zbee_zdp` 子层有帧数 | 解密成功 |
| 包列表只显示 `ZigBee 60 Command` / `Encrypted Payload` | 未解密（密钥或 Security Level 不对） |
| 出现 `Malformed Packet`（特别是 NWK 帧长度对不上） | 抓包脚本产出的 802.15.4 帧格式有误（如 LQI 字节未移除） |

## 常见问题

### Q1：配置了密钥还是解密不了

排查顺序：
1. **确认 Security Level 拼写**：必须是 `AES-128 Encryption, 32-bit Integrity Protection`（`32-bit` 带连字符）。Wireshark 对大小写不敏感，但对连字符敏感。
2. **确认密钥大小写无关但位数正确**：必须是 32 个 hex 字符（16 字节）。
3. **确认抓包脚本格式正确**：CCLoader sniffer 脚本必须删除 ZBOSS sniffer payload 末尾的 LQI + CRC 状态 2 字节（详见 [sniffer_via_ccloader.py](../../Tools/sniffer_via_ccloader.py) 的 `parse_zboss_packets` 函数）。否则帧长度错位，Wireshark 解析失败。
4. **确认 DLT 类型**：pcap 文件 DLT 必须是 `230`（IEEE 802.15.4 no FCS），脚本已默认设置。

### Q2：tshark 报 `Invalid format: "uat:..."` 

`-o` 参数值中的双引号在 PowerShell 中容易转义错误。推荐用**单引号包裹整个 `-o` 参数值**，内部双引号不转义：

```powershell
-o 'uat:zigbee_pc_keys:"01030507090B0D0F00020406080A0C0D","Normal",""'
```

在 cmd.exe 中则用：

```cmd
-o "uat:zigbee_pc_keys:\"01030507090B0D0F00020406080A0C0D\",\"Normal\",\"\""
```

### Q3：如何确认 Wireshark 实际加载的密钥

查看个人配置目录下的 `zigbee_pc_keys` 文件，每行一个密钥。若文件不存在，说明 GUI 中从未保存过密钥。

### Q4：多个网络/不同密钥怎么切换

Wireshark 的 `Pre-configured Keys` 表支持多行，会自动尝试所有密钥解密每个包。如需隔离不同网络的抓包，建议使用 Wireshark Profile（`配置` → `新建配置`），每个 Profile 维护独立的 `zigbee_pc_keys` 文件。

## 参考路径

- tshark 可执行文件：`D:\Green\Wireshark 4.4.7 x64 Npcap1.50 mod\App\Wireshark\tshark.exe`
- 抓包脚本：`D:\VC\Z-Stack\Tools\sniffer_via_ccloader.py`
- 测试 pcap：`d:\VC\CCLoader\cap\fix_test_2min.pcap`（已修复 ZBOSS payload 格式，可正常解密）
- 对比 pcap：`d:\VC\CCLoader\cap\1.pcap`（ZBOSS 官方 GUI 抓包，可解密，作为参考）
