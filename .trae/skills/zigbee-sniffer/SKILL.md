---
name: "zigbee-sniffer"
description: "通过 CCLoader ESP8266 WiFi 抓包器捕获 ZigBee 802.15.4 数据包并保存为 pcap 文件。当用户要求抓包、sniffer、监控 ZigBee 网络时调用。"
---

# ZigBee Sniffer 抓包 Skill

通过 CCLoader ESP8266 WiFi 无线抓包器，捕获 ZigBee 802.15.4 无线数据包，保存为 pcap 文件供 Wireshark 分析。

## 依赖

- CCLoader ESP8266 无线抓包器（已烧录 ZBOSS sniffer 固件）
- Python 3 + `requests` 库
- 抓包脚本：`Tools/sniffer_via_ccloader.py`

## 用法

```powershell
python Tools/sniffer_via_ccloader.py <ESP8266_IP> <通道号> [输出pcap路径]
```

### 参数说明

| 参数 | 必填 | 默认值 | 说明 |
|------|------|--------|------|
| `ESP8266_IP` | 否 | `10.0.0.147` | CCLoader 设备的 IP 地址 |
| `通道号` | 否 | `11` | ZigBee 信道（11-26） |
| `输出pcap路径` | 否 | `cap/sniffer_<时间戳>.pcap` | 输出 pcap 文件路径 |

## 工作流程

1. 通过 HTTP POST `/api/sniffer/start` 启动 CCLoader 的 sniffer 模式
2. 通过 HTTP GET `/api/sniffer/stream` 接收实时数据流
3. 解析 ZBOSS 协议格式的二进制数据包：
   - 移除 ZBOSS 帧头（4 字节）
   - 移除末尾 LQI + CRC 状态（2 字节）
   - 提取纯 IEEE 802.15.4 无 FCS 帧
4. 以 DLT=230（IEEE 802.15.4 no FCS）格式写入 pcap 文件
5. 每 5 秒报告抓包进度（包数、缓冲大小）
6. Ctrl+C 停止抓包，自动停止 CCLoader sniffer 模式

## 输出文件

- DLT 类型：`230`（IEEE 802.15.4 no FCS）
- 与 ZBOSS 官方 GUI 抓包格式兼容
- 可直接在 Wireshark 中打开，配合 `pcap-decrypt` skill 解密 NWK 层加密

## 示例

### 基本抓包（默认参数）

```powershell
python Tools/sniffer_via_ccloader.py
```

### 指定 IP、信道和输出文件

```powershell
python Tools/sniffer_via_ccloader.py 10.0.0.147 15 cap/test_5min.pcap
```

### 抓包 1 分钟用于测试

```powershell
python Tools/sniffer_via_ccloader.py 10.0.0.147 11 cap/switch_test_1min.pcap
```

## 注意事项

- 抓包前确保 CCLoader 设备已上电且 WiFi 连接正常
- 抓包时 CCLoader 的 LED 会闪烁指示数据流
- 抓包过程中若大量丢包，控制台会输出 `[WARN] CCLoader 丢失 X 字节` 提示
- 抓包结束后会自动调用 `/api/stop` 停止 sniffer 模式