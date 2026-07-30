"""查询 z2m 设备列表，输出短地址映射"""
import paho.mqtt.client as mqtt
from paho.mqtt.client import CallbackAPIVersion
import json, sys

BROKER = "10.0.0.3"
PORT = 1883
USER = "amqtt"
PASS = "mymqtt"

def on_connect(client, userdata, flags, reason_code, properties):
    print(f"[已连接] rc={reason_code}")
    client.subscribe("zigbee2mqtt/bridge/devices")

def on_message(client, userdata, msg):
    if msg.topic == "zigbee2mqtt/bridge/devices":
        devices = json.loads(msg.payload.decode())
        print(f"\n=== 设备列表 ({len(devices)} 个) ===")
        print(f"{'IEEE':>18} {'短地址':>8} {'类型':>12} {'面试状态':>12} {'名称'}")
        print("-" * 90)
        for d in devices:
            ieee = d.get('ieee_address', '-')
            short = d.get('network_address', None)
            short_s = f"0x{short:04X}" if short is not None else '-'
            dtype = d.get('type', '-')
            state = d.get('interview_state', '-')
            name = d.get('friendly_name', '-')
            # 软件构建 ID（z2m interview 后存储）
            sw_build = d.get('software_build_id', '-')
            print(f"{ieee:>18} {short_s:>8} {dtype:>12} {state:>12} {name}  sw={sw_build}")
        client.disconnect()

client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2)
client.username_pw_set(USER, PASS)
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_forever()
