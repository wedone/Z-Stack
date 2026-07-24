# Z2M 接入方案

> **状态**: 已完成。借壳 alab.switch，z2m 自动识别，无需 External Converter。

## 借壳方案选型

### 选型标准

借壳设备必须满足：
1. 使用标准 ZCL genOnOff 簇（`m.onOff()`），不使用 Tuya 私有 DP 协议
2. 不依赖 `tuya.configureMagicPacket` 或 `legacy.fz.tuya_switch`
3. Endpoint 映射为 `l1:1, l2:2, l3:3, l4:4`（4 个独立端点）

### 选定: alab.switch

**选择理由**：
- 功能最多：4路开关 + 4路输入状态 + 命令事件
- 完全不依赖 Tuya 私有协议：纯标准 ZCL
- `zigbeeModel` 匹配，只需改 ModelId

### 候选设备一览

| 优先级 | model | 厂商 | zigbeeModel | 依赖Tuya私有协议 |
|--------|-------|------|-------------|-----------------|
| ⭐⭐⭐ | **alab.switch** | Alab | `alab.switch` | ❌ |
| ⭐⭐ | TS0004 | Tuya | `TS0004` | ✅ tuyaBase |
| ⭐⭐ | HGZB-4S | Nue/3A | `LXN-4S27LX1.0` | ❌ |
| ⭐ | ZB-SW04 | eWeLink | `ZB-SW04` | ❌ |
| ⭐ | dqhome.re4 | DQHOME | `dqhome.re4` | ❌ |

> **匹配方式区别**：`zigbeeModel` 只需 ModelId 匹配，ManufacturerName 可任意值。`fingerprint` 需 ModelId + ManufacturerName 同时精确匹配。

## 固件端设置

修改 `zcl_samplesw_data.c`：

```c
const uint8 zclSampleSw_ManufacturerName[] = { 16, 'T','e','x','a','s','I','n','s','t','r','u','m','e','n','t','s' };
const uint8 zclSampleSw_ModelId[] = { 11, 'a','l','a','b','.','s','w','i','t','c','h' };
```

## alab.switch 设备定义

文件路径: `zigbee-herdsman-converters/src/devices/custom_devices_diy.ts`

```javascript
{
    zigbeeModel: ["alab.switch"],
    model: "alab.switch",
    vendor: "Alab",
    description: "Four channel relay board with four inputs",
    extend: [
        m.deviceEndpoints({endpoints: {l1: 1, l2: 2, l3: 3, l4: 4, in1: 5, in2: 6, in3: 7, in4: 8}}),
        m.onOff({powerOnBehavior: false, configureReporting: false, endpointNames: ["l1", "l2", "l3", "l4"]}),
        m.commandsOnOff({endpointNames: ["l1", "l2", "l3", "l4"]}),
        m.numeric({name: "input_state", cluster: "genAnalogInput", attribute: "presentValue",
                   endpointNames: ["in1", "in2", "in3", "in4"]}),
    ],
}
```

## Endpoint 映射

| Endpoint | 功能 | 簇 | 状态 |
|----------|------|-----|------|
| EP 1 (l1) | 继电器 1 | genOnOff | ✅ 已实现 |
| EP 2 (l2) | 继电器 2 | genOnOff | ✅ 已实现 |
| EP 3 (l3) | 继电器 3 | genOnOff | ✅ 已实现 |
| EP 4 (l4) | 继电器 4 | genOnOff | ✅ 已实现 |
| EP 5 (in1) | 输入 1 | genAnalogInput | ❌ 待实现 |
| EP 6 (in2) | 输入 2 | genAnalogInput | ❌ 待实现 |
| EP 7 (in3) | 输入 3 | genAnalogInput | ❌ 待实现 |
| EP 8 (in4) | 输入 4 | genAnalogInput | ❌ 待实现 |

## 备选: 自定义 External Converter

如借壳方案不满足需求，可使用 External Converter：

```javascript
const definition = {
    zigbeeModel: ['DIY4GANG-01'],
    model: 'DIY4GANG-01',
    vendor: 'DIYRuZ',
    description: '4 gang smart switch (DIY)',
    fromZigbee: [fz.on_off],
    toZigbee: [tz.on_off],
    exposes: [
        e.switch().withEndpoint('l1'),
        e.switch().withEndpoint('l2'),
        e.switch().withEndpoint('l3'),
        e.switch().withEndpoint('l4'),
    ],
    endpoint: (device) => { return {l1: 1, l2: 2, l3: 3, l4: 4}; },
    meta: {multiEndpoint: true},
};
```

## 如何查找其他借壳设备

1. 克隆 `https://github.com/Koenkk/zigbee-herdsman-converters`
2. 在 `src/devices/` 搜索 `m.onOff` + `l1: 1` + `l4: 4`
3. 确认不使用 `legacy.fz.tuya_switch`、`tuya.modernExtend.tuyaBase`
4. 查看设备图片: `https://www.zigbee2mqtt.io/devices/<model>.html`
