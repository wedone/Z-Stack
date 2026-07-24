# Z2M 接入方案

> **状态**: 已完成。借壳 HGZB-4S (Nue/3A)，z2m 自动识别，无需 External Converter。
>
> **断电记忆**: v0.2.0起支持。HGZB-4S 的 `m.onOff` 默认启用 `powerOnBehavior`，Z2M 页面直接显示 `Power-on behavior` 配置项。

## 借壳方案选型

### 选型标准

借壳设备必须满足：
1. 使用标准 ZCL genOnOff 簇（`m.onOff()`），不使用 Tuya 私有 DP 协议
2. 不依赖 `tuya.configureMagicPacket` 或 `legacy.fz.tuya_switch`
3. Endpoint 映射为 `l1:1, l2:2, l3:3, l4:4`（4 个独立端点）
4. **`powerOnBehavior` 默认启用**（不显式设为 false），支持断电记忆配置

### 选定: HGZB-4S (Nue / 3A)

**选择理由**：
- `m.onOff` 默认启用 `powerOnBehavior`，Z2M 页面直接显示 `Power-on behavior` 下拉框
- 纯标准 ZCL，无 Tuya 私有协议依赖
- 4 路开关端点映射 `l1:1, l2:2, l3:3, l4:4`
- 代码最简洁，无额外配置干扰
- `zigbeeModel` 匹配，只需改 ModelId 为 `LXN-4S27LX1.0`

### 候选设备一览

基于 `zigbee-herdsman-converters` 仓库全量筛选（15 个含 `l1:1, l4:4` 映射的文件）：

| 优先级 | model | 厂商 | zigbeeModel | powerOnBehavior | Tuya依赖 | input_state |
|--------|-------|------|-------------|:---:|:---:|:---:|
| ⭐⭐⭐ | **HGZB-4S** | Nue/3A | `LXN-4S27LX1.0` | ✅ 默认启用 | ❌ | ❌ |
| ⭐⭐ | ZB-SW04 | eWeLink | `ZB-SW04` | ✅ 默认启用 | ❌ | ❌ |
| ⭐ | alab.switch | Alab | `alab.switch` | ❌ 显式禁用 | ❌ | ✅ EP5-8 |
| ⭐ | dqhome.re4 | DQHOME | `dqhome.re4` | ❌ 显式禁用 | ❌ | ❌ |
| ⭐ | ROB_200-050-0 | ROBB | `ROB_200-050-0` | ✅ 默认启用 | ❌ | ❌ (5路含USB) |
| ❌ | TS0004 | Tuya | `TS0004` | - | ✅ tuyaBase | - |
| ❌ | WP33-EU | LELLKI | `WP33-EU` | ❌ | ✅ tuyaBase | - |
| ❌ | QARZ4LR | QA | `QARZ4LR` | - | ✅ tuyaBase | - |
| ❌ | TB26-4 | Zemismart | `TB26-4` | - | ✅ legacy.tuya_switch | - |

> **关键发现**：`alab.switch` 是唯一有 `input_state (EP5-8)` 的设备，但 `powerOnBehavior: false` 被显式禁用，导致断电记忆无法通过 Z2M 配置。换壳到 HGZB-4S 牺牲了 `input_state` 显示，换取了内置 `powerOnBehavior` 支持。

> **`m.onOff` 默认行为**：查 `modernExtend.ts` 源码，`powerOnBehavior` 参数默认为 `true`，启用后自动添加 `power_on_behavior` expose + `startUpOnOff` 属性 reporting。HGZB-4S 和 ZB-SW04 均未显式指定该参数，故默认启用。

## 固件端设置

修改 `zcl_samplesw_data.c`：

```c
const uint8 zclSampleSw_ManufacturerName[] = { 16, 'T','e','x','a','s','I','n','s','t','r','u','m','e','n','t','s' };
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };
```

## HGZB-4S 设备定义

文件路径: `zigbee-herdsman-converters/src/devices/nue_3a.ts`

```javascript
{
    zigbeeModel: ["LXN-4S27LX1.0"],
    model: "HGZB-4S",
    vendor: "Nue / 3A",
    description: "Smart light switch - 4 gang v2.0",
    extend: [m.deviceEndpoints({endpoints: {l1: 1, l2: 2, l3: 3, l4: 4}}), m.onOff({endpointNames: ["l1", "l2", "l3", "l4"]})],
}
```

## Endpoint 映射

| Endpoint | 功能 | 簇 | Z2M识别 | 固件实现 |
|----------|------|-----|:---:|:---:|
| EP 1 (l1) | 继电器 1 | genOnOff | ✅ | ✅ |
| EP 2 (l2) | 继电器 2 | genOnOff | ✅ | ✅ |
| EP 3 (l3) | 继电器 3 | genOnOff | ✅ | ✅ |
| EP 4 (l4) | 继电器 4 | genOnOff | ✅ | ✅ |
| EP 5-8 | 触摸输入状态 | genAnalogInput | ❌ Z2M忽略 | ✅ 保留(见下) |

### 关于 EP5-8 input_state

换壳到 HGZB-4S 后，Z2M 不识别 EP5-8（HGZB-4S 定义中无这些端点），但固件保留这些端点：

- **触摸操作仍能反映到 Z2M**：触摸触发继电器翻转 → 继电器状态变化 → ZCL Report Attributes 上报 → Z2M 显示开关状态变化
- **EP5-8 被忽略**：Z2M 不会显示独立的 `input_state` 触摸按键状态
- **保留原因**：改动最小化、降低风险、未来可扩展（如向 Z2M 上游提 PR 或换回 alab.switch）

## 断电记忆 (v0.2.0+)

### Z2M 页面操作

换壳后无需任何额外配置，Z2M 设备页面每路开关 (l1-l4) 下自动出现 `Power-on behavior` 下拉框：

| 选项 | startUpOnOff值 | 说明 |
|------|----------------|------|
| Off | 0x00 | 上电关闭 |
| On | 0x01 | 上电开启 |
| Toggle | 0x02 | 上电翻转断电前状态 |
| Previous | 0xFF | 恢复断电前状态 (默认) |

> 4路共用同一配置 (写任一端点的 power_on_behavior 即更新全局 startUpOnOff)。

### 固件侧实现

- ZCL属性: genOnOff cluster 的 startUpOnOff (0x4003), enum8, 读写权限
- NV持久化: 配置和继电器状态写入 Flash (NV ID: 0x0F10/0x0F11)
- Flash寿命优化: 延迟写入(5秒合并) + 对比写入(相同则跳过)

## 如何查找其他借壳设备

1. 克隆 `https://github.com/Koenkk/zigbee-herdsman-converters`
2. 在 `src/devices/` 搜索 `m.onOff` + `l1: 1` + `l4: 4`
3. 确认不使用 `legacy.fz.tuya_switch`、`tuya.modernExtend.tuyaBase`
4. 确认 `m.onOff` 未显式设置 `powerOnBehavior: false`
5. 查看设备图片: `https://www.zigbee2mqtt.io/devices/<model>.html`
