"""通过 z2m MQTT 读取借壳设备的 SwBuildId 属性"""
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import json, sys, time

BROKER = "10.0.0.3"
PORT = 1883
USER = "amqtt"
PASS = "mymqtt"

# 借壳设备的 friendly_name（从之前抓包得知短地址 0x703F = 餐厅开关1, 0xD334 = 客厅开关1）
# 用 IEEE 地址作为 friendly_name
TARGETS = [
    "0x57583a03004b1200",  # 餐厅开关1（7米场景）
    "0xd6563a03004b1200",  # 客厅开关1
]

responses = []

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[已连接] rc={reason_code}")
    # 订阅设备状态 topic
    for target in TARGETS:
        client.subscribe(f"zigbee2mqtt/{target}")
    # 触发读取 GenBasic cluster 的 SwBuildId (0x4000) 属性
    for target in TARGETS:
        payload = json.dumps({
            "read": {
                "cluster": "genBasic",
                "attributes": ["swBuildId", "modelId", "manufacturerName", "hwVersion", "dateCode", "zclVersion", "appVersion", "stackVersion"]
            }
        })
        client.publish(f"zigbee2mqtt/{target}/set", payload)
        print(f"[发送] zigbee2mqtt/{target}/set: {payload}")

def on_message(client, userdata, msg):
    topic = msg.topic
    try:
        payload = json.loads(msg.payload.decode())
    except:
        payload = msg.payload.decode()
    print(f"[{topic}] {json.dumps(payload, ensure_ascii=False) if isinstance(payload, dict) else payload}")
    responses.append((topic, payload))

client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
client.username_pw_set(USER, PASS)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)

# 运行 15 秒收集响应
client.loop_start()
time.sleep(15)
client.loop_stop()
client.disconnect()

print(f"\n=== 共收到 {len(responses)} 条响应 ===")
