# z2m MQTT 调试指南

> 通过 MQTT 实时监控和调试 Zigbee 设备入网、状态和控制。配合抓包分析使用。

---

## 1. MQTT 连接配置

### 1.1 服务器信息

| 项 | 值 |
|---|---|
| Broker | `10.0.0.3:1883` |
| 用户名 | `amqtt` |
| 密码 | `mymqtt` |
| Base Topic | `zigbee2mqtt`（默认） |
| 协议 | MQTT v3.1.1 |

### 1.2 快速订阅（PowerShell）

使用 `mosquitto_sub`（需先安装 mosquitto 客户端）：

```powershell
# 订阅所有 z2m 消息（调试用）
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/#" -v

# 只订阅 bridge 事件（入网/离线/日志）
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/event" -v
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/logging" -v
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/state" -v
```

### 1.3 Python 客户端（推荐，便于脚本化）

```python
# tools/z2m_mqtt_monitor.py
import paho.mqtt.client as mqtt
import json, sys

BROKER = "10.0.0.3"
PORT = 1883
USER = "amqtt"
PASS = "mymqtt"

def on_connect(client, userdata, flags, rc):
    print(f"[已连接] rc={rc}")
    # 订阅所有事件
    client.subscribe([
        ("zigbee2mqtt/bridge/event", 0),       # 入网/离线/面试
        ("zigbee2mqtt/bridge/logging", 0),     # 日志
        ("zigbee2mqtt/bridge/devices", 0),     # 设备列表
        ("zigbee2mqtt/bridge/state", 0),       # bridge 状态
    ])

def on_message(client, userdata, msg):
    try:
        payload = json.loads(msg.payload.decode())
    except:
        payload = msg.payload.decode()
    print(f"[{msg.topic}] {json.dumps(payload, ensure_ascii=False) if isinstance(payload, dict) else payload}")

client = mqtt.Client()
client.username_pw_set(USER, PASS)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_forever()
```

运行：`python tools/z2m_mqtt_monitor.py`

---

## 2. 关键 MQTT Topics

### 2.1 调试常用 Topics

| Topic | 说明 | 调试用途 |
|---|---|---|
| `zigbee2mqtt/bridge/state` | bridge 在线状态 | 确认 z2m 运行中 |
| `zigbee2mqtt/bridge/info` | bridge 配置信息 | 查看协调器、通道、PAN ID |
| `zigbee2mqtt/bridge/devices` | **完整设备列表** | **查看设备 network_address（短地址）** |
| `zigbee2mqtt/bridge/event` | **设备入网/离线事件** | **实时监控入网流程** |
| `zigbee2mqtt/bridge/logging` | 日志（info/warn/error） | 查看入网错误、超时 |
| `zigbee2mqtt/FRIENDLY_NAME` | 设备状态 JSON | 查看设备当前状态 |
| `zigbee2mqtt/FRIENDLY_NAME/availability` | 设备在线/离线 | 确认设备是否在线 |

### 2.2 设备入网事件（`bridge/event`）

入网时 z2m 会依次发布以下事件：

```
1. device_joined     设备已关联到网络
   {"type":"device_joined","data":{"friendly_name":"0x...","ieee_address":"0x..."}}

2. device_interview  面试开始
   {"type":"device_interview","data":{"status":"started","ieee_address":"0x..."}}

3. device_interview  面试成功
   {"type":"device_interview","data":{"status":"successful","ieee_address":"0x...", "supported":true, "definition":{...}}}

4. device_announce   设备宣告（可选）
   {"type":"device_announce","data":{"friendly_name":"...","ieee_address":"0x..."}}
```

**失败情况**：
```
{"type":"device_interview","data":{"status":"failed","ieee_address":"0x..."}}
```

### 2.3 设备列表（`bridge/devices`）

**重点**：此 topic 包含每个设备的 `network_address`（短地址），可解决"抓包看不到短地址"的问题。

```json
[
  {
    "ieee_address": "0x00124b0022810fd5",      // MAC 长地址
    "type": "Coordinator",
    "network_address": 0,                       // 短地址 0x0000
    "friendly_name": "Coordinator"
  },
  {
    "ieee_address": "0xd6563a03004b1200",
    "type": "Router",
    "network_address": 54068,                   // 短地址 0xD334
    "friendly_name": "living_room_switch_1",
    "interview_state": "SUCCESSFUL"
  }
]
```

获取方法（PowerShell 一次查询）：

```powershell
# 获取设备列表并提取 ieee_address + network_address
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/devices" -C 1 | ConvertFrom-Json | ForEach-Object {
    [PSCustomObject]@{
        ieee = $_.ieee_address
        short = ("0x{0:X4}" -f $_.network_address)
        name = $_.friendly_name
        type = $_.type
        state = $_.interview_state
    }
} | Format-Table
```

---

## 3. 调试操作

### 3.1 允许入网

```powershell
# 全局允许入网 254 秒
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/permit_join" -m "{\"value\": true, \"time\": 254}"

# 只允许特定设备入网
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/permit_join" -m "{\"value\": true, \"time\": 60, \"device\": \"0x00124b0022810fd5\"}"

# 关闭入网
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/permit_join" -m "{\"value\": false, \"time\": 0}"
```

### 3.2 移除设备（配合重置入网测试）

```powershell
# 通过 friendly_name 移除
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/device/remove" -m "{\"id\": \"living_room_switch_1\", \"block\": true, \"force\": true}"

# 通过 IEEE 移除
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/device/remove" -m "{\"id\": \"0xd6563a03004b1200\", \"block\": true, \"force\": true}"
```

- `block: true` — 同时把设备加入黑名单，防止重新入网
- `force: true` — 即使设备离线也强制移除

### 3.3 控制设备（测试通信）

```powershell
# 开
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/living_room_switch_1/set" -m "{\"state\": \"ON\"}"

# 关
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/living_room_switch_1/set" -m "{\"state\": \"OFF\"}"

# 查询当前状态
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/living_room_switch_1/get" -m "{\"state\": \"\"}"
```

### 3.4 直接读写 ZCL 属性（高级调试）

```powershell
# 读 ZCL 属性
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/living_room_switch_1/set" -m "{\"read\": {\"cluster\": \"genOnOff\", \"attributes\": [0]}}"

# 写 ZCL 属性（如 startUpOnOff）
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/living_room_switch_1/set" -m "{\"write\": {\"cluster\": \"genOnOff\", \"payload\": {\"startUpOnOff\": 1}}}"
```

### 3.5 查看设备路由（可选）

z2m 可通过 `zigbee2mqtt/bridge/request/device/route_table` 触发路由表查询（需协调器支持），结果通过 `zigbee2mqtt/bridge/response/device/route_table` 返回。

---

## 4. 调试工作流（结合抓包）

### 4.1 入网问题调试流程

```
┌─────────────────────────────────────────────────────────────┐
│ 1. 开启抓包（CCLoader sniffer）                              │
│    python tools\sniffer_via_ccloader.py 10.0.0.147 11 cap\xxx.pcap │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│ 2. 订阅 z2m 事件                                             │
│    python tools\z2m_mqtt_monitor.py                         │
│    （监控 device_joined / device_interview / device_announce）│
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│ 3. 允许入网                                                  │
│    permit_join time=254                                     │
└────────────────────────────┬────────────────────────────────┘
                             │
┌────────────────────────────▼────────────────────────────────┐
│ 4. 重置设备（S1 长按 5 秒）                                   │
└────────────────────────────┬────────────────────────────────┘
                             │
              ┌──────────────┴───────────────┐
              ▼                              ▼
    ┌──────────────────┐           ┌──────────────────┐
    │ 抓包分析          │           │ z2m 事件         │
    │ - Beacon Request  │           │ - device_joined  │
    │ - Association     │           │ - interview      │
    │ - Transport Key   │           │ - success/fail   │
    └────────┬─────────┘           └────────┬─────────┘
             │                              │
             └──────────────┬───────────────┘
                            ▼
              ┌─────────────────────────┐
              │ 5. 对比分析              │
              │ - 抓包看入网流程是否完成 │
              │ - z2m 看面试是否成功    │
              │ - 查询设备 network_addr │
              └─────────────────────────┘
```

### 4.2 查询设备短地址（抓包辅助）

**问题**：抓包中设备入网前只有 MAC（长地址），入网后才分配短地址。z2m 可查询已入网设备的短地址：

```powershell
# 查询所有设备的 network_address
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/devices" -C 1 -W 5 | ConvertFrom-Json | Select-Object ieee_address, @{n="short";e={"0x{0:X4}" -f $_.network_address}}, friendly_name | Format-Table
```

**3米和7米设备短地址查询**（入网后执行）：

```powershell
# 查询特定设备
mosquitto_sub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/devices" -C 1 -W 5 | ConvertFrom-Json | Where-Object { $_.ieee_address -in @("0xd6563a03004b1200", "0x1cc13703004b1200", "0x57583a03004b1200") } | ForEach-Object { Write-Output "$($_.friendly_name): $($_.ieee_address) -> 0x$($_.network_address.ToString('X4'))" }
```

### 4.3 协调器检查

```powershell
# 检查协调器内存中缺失的路由器
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/coordinator_check" -m "{}"
# 结果在 zigbee2mqtt/bridge/response/coordinator_check
```

---

## 5. 当前网络设备清单（2026-07-30）

| IEEE 长地址 | 短地址 | 设备名称 | 固件类型 | 物理位置 |
|---|---|---|---|---|
| 0x00124b0022810fd5 | 0x0000 | **协调器** | z2m CC2652P | 客厅电视后 |
| 0xd6563a03004b1200 | 0xD334 | 客厅开关1（HGZBSwitch 正常入网） | 借壳 LXN-4S27LX1.0 | 客厅 |
| 0x1cc13703004b1200 | 0x4DA9 | 客厅开关1（HGZBSwitch，3米场景，断电 0x4B9F 后入网成功） | 借壳 LXN-4S27LX1.0 | 客厅 |
| 0x57583a03004b1200 | 待查 | 餐厅开关1（HGZBSwitch，7米场景） | 借壳 LXN-4S27LX1.0 | 餐厅 |
| 0x5c32d718004b1200 | 0x4B9F | 客厅开关2（ptvo 带路由）⚠️ | ptvo | 客厅（与3米设备并排） |
| - | 0x6F89 | 书房开关（ptvo） | ptvo | 书房 |
| - | 0x3110 | 书房蚊香插座 | 原厂 FB56+SKT14AL2.1 | 书房 |
| - | 0x7CDA | 大房蚊香插座 | 原厂 FB56+SKT14AL2.1 | 大房 |
| - | 0x6BF5 | 小房蚊香插座 | 原厂 FB56+SKT14AL2.1 | 小房 |
| - | 0xBF20 | 公卫插座 | 原厂 FB56+SKT14AL2.1 | 公卫 |
| - | 0x9435 | 厨房烟雾警报器 | 原厂 M415-6C | 厨房（z2m 状态正常） |

> 短地址"待查"的设备可通过 §4.2 查询。短地址在设备重新入网后会变化。

---

## 6. 常见问题

### Q1: 抓包中看不到设备短地址？

**A**: 入网前设备只有 MAC 长地址，短地址在 Association Response 时由协调器分配。抓包中看到的 `short_addr=0x4DA9` 就是协调器分配的。入网后在 z2m `bridge/devices` 中可查询确认。

### Q2: z2m 日志看不到入网，但抓包看到 Association？

**A**: 说明 Association 完成但 Transport Key 未送达，设备未真正入网。检查：
1. 抓包是否看到 Transport Key 帧（加密的 Data 帧）
2. z2m `bridge/logging` 是否有错误
3. 协调器是否配置了正确的 network key

### Q3: 设备入网后立即掉线？

**A**: 可能是：
1. 路由问题（设备通过错误路由器入网，之后无法通信）
2. 设备重启后未恢复网络状态
3. z2m 面试失败导致设备被标记为 unsupported

### Q4: 如何强制设备重新入网？

```powershell
# 1. 从 z2m 移除设备（block + force）
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/device/remove" -m "{\"id\": \"0x1cc13703004b1200\", \"block\": false, \"force\": true}"

# 2. 重置设备（S1 长按 5 秒）

# 3. 允许入网
mosquitto_pub -h 10.0.0.3 -p 1883 -u amqtt -P mymqtt -t "zigbee2mqtt/bridge/request/permit_join" -m "{\"value\": true, \"time\": 254}"

# 4. 等待设备入网（监控 bridge/event）
```

---

## 7. 参考文档

- [z2m 官方 MQTT Topics 文档](https://www.zigbee2mqtt.io/guide/usage/mqtt_topics_and_messages.html)
- [抓包分析-入网问题.md](抓包分析-入网问题.md) - 抓包发现的入网问题根因
- [诊断与优化方案.md](诊断与优化方案.md) - 整体诊断方法论
