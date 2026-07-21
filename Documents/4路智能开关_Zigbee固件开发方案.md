# 4路智能开关 Zigbee 固件开发方案

## 1. 项目概述

基于 CC2530 芯片的 4 路智能墙壁开关，通过 Zigbee 协议接入 Zigbee2MQTT 智能家居系统。

| 项目 | 内容 |
|------|------|
| 主控芯片 | CC2530F256RHAR |
| 通信协议 | Zigbee 3.0 |
| 接入平台 | Zigbee2MQTT → Home Assistant |
| 固件框架 | 基于 DIYRuZ_RT（Z-Stack 3.0） |
| 设备角色 | Router（路由器） |
| 借壳型号 | TS0004（Tuya 4路开关） |

---

## 2. 硬件设计

### 2.1 核心芯片

- **U1**: CC2530 — 主控 MCU + Zigbee 射频
- **U2**: CC2530F256RHAR — 射频前端（原理图中为完整芯片引脚图）

### 2.2 外设接口

#### J2 — 调试/烧录接口（5Pin）

| 引脚 | 功能 | 说明 |
|------|------|------|
| 1 | GND | 地 |
| 2 | RST | 复位 |
| 3 | DC | 调试时钟 |
| 4 | DD | 调试数据 |
| 5 | 3V3 | 电源 |

#### J5 — 扩展接口（10Pin 立贴针）

| 引脚 | 功能 | 说明 |
|------|------|------|
| 1,2 | GND | 地 |
| 3 | 3V3 | 电源 |
| 4 | P2_0 | 预留 GPIO |
| 5 | P1_6 | 预留 GPIO |
| 6 | P1_2 | 预留 GPIO |
| 7 | P1_0 | 预留 GPIO |
| 8-10 | 3V3 | 电源 |

### 2.3 输入部分

#### S1 — 物理按键

- 型号: TS24CA
- 功能: 复位/配网按键
- 连接: P1_3/S1 → CC2530 P1_3

#### WTC6106BSI — 4路触摸芯片

| 触摸通道 | 触摸芯片输出 | CC2530 输入 | 功能 |
|----------|-------------|-------------|------|
| TOUCH1 | OUT0 | P0_4 | 第1路触摸输入 |
| TOUCH2 | OUT1 | P0_5 | 第2路触摸输入 |
| TOUCH3 | OUT2 | P0_6 | 第3路触摸输入 |
| TOUCH4 | OUT3 | P0_7 | 第4路触摸输入 |
| — | OUT4 | — | 未连接 |
| — | OUT5 | — | 未连接 |

> **信号特征**: WTC6106BSI 内置 16bit CDC + RISC 处理器，**直接输出高低电平**指示按键动作。触摸时输出低电平（有效），松开恢复高电平，为持续电平信号而非脉冲。CC2530 通过 100ms 轮询检测电平变化即可识别触摸事件。

### 2.4 输出部分

#### 继电器控制（4路）

| 路数 | CC2530 引脚 | 功能 | 触发方式 |
|------|-------------|------|----------|
| 第1路 | P1_0 | 继电器控制 | 低电平触发 |
| 第2路 | P1_2 | 继电器控制 | 低电平触发 |
| 第3路 | P1_6 | 继电器控制 | 低电平触发 |
| 第4路 | P2_0 | 继电器控制 | 低电平触发 |

#### 状态指示灯（4路，反逻辑）

| 路数 | CC2530 引脚 | 功能 | 逻辑说明 |
|------|-------------|------|----------|
| D1 | P0_0 | 第1路状态灯 | 继电器 OFF → LED 亮 |
| D2 | P0_1 | 第2路状态灯 | 继电器 OFF → LED 亮 |
| D3 | P0_2 | 第3路状态灯 | 继电器 OFF → LED 亮 |
| D4 | P0_3 | 第4路状态灯 | 继电器 OFF → LED 亮 |

> **反逻辑设计**: LED 与继电器状态相反，用于在黑暗环境中标识开关位置。
> **LED 型号**: 0603 贴片 LED，常规接法。

---

## 3. PTVO 固件配置参考

> **说明**: PTVO 为闭源付费工具，仅输出编译后的 hex 固件，无法获取源码。以下内容仅作为**配置逻辑参考**，实际固件开发基于 DIYRuZ_RT 开源代码手写实现。

PTVO 配置工具用于生成自定义 Zigbee 固件，以下为当前硬件配置:

### 3.1 PTVO 引脚命名对照表

PTVO 工具使用 `P` + `端口号` + `引脚号` 的命名方式，与标准命名对照如下:

| PTVO 命名 | 标准命名 | 功能 |
|-----------|---------|------|
| P00 | P0_0 | LED D1 |
| P01 | P0_1 | LED D2 |
| P02 | P0_2 | LED D3 |
| P03 | P0_3 | LED D4 |
| P04 | P0_4 | 触摸输入 1 |
| P05 | P0_5 | 触摸输入 2 |
| P06 | P0_6 | 触摸输入 3 |
| P07 | P0_7 | 触摸输入 4 |
| P10 | P1_0 | 继电器 1 |
| P12 | P1_2 | 继电器 2 |
| P16 | P1_6 | 继电器 3 |
| P20 | P2_0 | 继电器 4 |

### 3.2 输出配置

| 输出 | PTVO 引脚 | 标准引脚 | 类型 | 上拉/下拉 | 反相 | 功能 |
|------|----------|---------|------|----------|------|------|
| Output 1 | P33 | — | Group switch | Pull-up | — | 虚拟开关（群组控制） |
| Output 2 | P34 | — | Group switch | Pull-up | — | 虚拟开关（群组控制） |
| Output 3 | P35 | — | Group switch | Pull-up | — | 虚拟开关（群组控制） |
| Output 4 | P36 | — | Group switch | Pull-up | — | 虚拟开关（群组控制） |
| Output 5 | P00 | P0_0 | GPIO | Pull-up | — | LED D1 控制 |
| Output 6 | P01 | P0_1 | GPIO | Pull-up | — | LED D2 控制 |
| Output 7 | P02 | P0_2 | GPIO | Pull-up | — | LED D3 控制 |
| Output 8 | P03 | P0_3 | GPIO | Pull-up | — | LED D4 控制 |
| Output 9 | P10 | P1_0 | GPIO | Pull-up | **Inversed** | 继电器 1 控制 |
| Output 10 | P12 | P1_2 | GPIO | Pull-up | **Inversed** | 继电器 2 控制 |
| Output 11 | P16 | P1_6 | GPIO | Pull-up | **Inversed** | 继电器 3 控制 |
| Output 12 | P20 | P2_0 | GPIO | Pull-up | **Inversed** | 继电器 4 控制 |

> **Group switch 说明**: PTVO 的虚拟 IO（P33-P36）用于触发群组，同时控制关联的物理输出（继电器 + LED）。
> **Inversed 说明**: 逻辑状态 ON 映射到 GPIO 低电平输出，用于适配低电平触发的继电器。

### 3.3 输入配置

| 输入 | PTVO 引脚 | 标准引脚 | 类型 | 上拉/下拉 | 关联输出 | 功能 |
|------|----------|---------|------|----------|----------|------|
| Input 1 | P04 | P0_4 | GPIO | Pull-up | Link to out 1 | 触摸输入 1 |
| Input 2 | P05 | P0_5 | GPIO | Pull-up | Link to out 2 | 触摸输入 2 |
| Input 3 | P06 | P0_6 | GPIO | Pull-up | Link to out 3 | 触摸输入 3 |
| Input 4 | P07 | P0_7 | GPIO | Pull-up | Link to out 4 | 触摸输入 4 |

### 3.4 Inversed 机制说明

**Inversed 是 PTVO 软件层面的逻辑反相功能，非 CC2530 硬件功能。**

Inversed 仅作用于 **PTVO 应用层到 GPIO 硬件层** 的映射，**不改变 Zigbee 通讯层的语义**。

#### 三层模型

```
Zigbee 通讯层          应用层/PTVO 逻辑          GPIO 硬件层
┌─────────────┐       ┌─────────────────┐       ┌─────────────────┐
│ 云端/APP/HA  │──────→│   逻辑状态 ON/OFF  │──────→│   GPIO 输出电平   │
│ 发送 OnOff  │       │                 │       │                 │
└─────────────┘       └─────────────────┘       └─────────────────┘
                              │                          │
                              ▼                          ▼
                        无 Inversed: ON=HIGH        继电器电路
                        有 Inversed: ON=LOW         （低电平触发）
```

| 配置 | 逻辑 ON 时的 GPIO 输出 | 逻辑 OFF 时的 GPIO 输出 |
|------|----------------------|-----------------------|
| 无 Inversed | HIGH (高电平) | LOW (低电平) |
| 有 Inversed | LOW (低电平) | HIGH (高电平) |

> **注意**: Inversed 只改变 GPIO 电平映射，Zigbee 报文 `OnOff: On` 始终对应逻辑状态 ON，`OnOff: Off` 始终对应逻辑状态 OFF。

#### 本设备的电平关系

| 设备 | 配置 | 有效电平 | 说明 |
|------|------|---------|------|
| 继电器 | Pull-up + **Inversed** | **LOW** | 低电平触发吸合 |
| LED | Pull-up + **无 Inversed** | **HIGH** | 高电平点亮 |

#### 反逻辑实现原理

继电器和 LED 的有效电平**相反**，通过 Group switch **同逻辑状态联动**，天然实现"继电器 OFF 时 LED 亮"的效果。

| 逻辑状态 | Zigbee 报文 | 继电器 GPIO | LED GPIO | 继电器物理 | LED 物理 |
|---------|------------|-------------|----------|-----------|---------|
| **OFF** | `OnOff: Off` | **HIGH** | **HIGH** | 断开 | **亮** ✅ |
| **ON** | `OnOff: On` | **LOW** | **LOW** | 吸合 | **灭** ✅ |

> 上电初始状态: Pull-up 使 GPIO 默认 HIGH → 继电器断开 + LED 亮，符合"默认关灯但指示灯亮"的需求。

---

## 4. 固件架构

### 4.1 技术栈

```
┌─────────────────────────────────────┐
│  应用层 (Application Layer)          │
│  ├── 4路开关状态管理                  │
│  ├── 按键/触摸事件处理                │
│  ├── 继电器 + LED 联动控制            │
│  ├── 断电记忆功能 (NV 存储)           │
│  ├── 开关同步 (Z2M Binding)          │
│  └── 状态上报 (Reporting)            │
├─────────────────────────────────────┤
│  ZCL 层 (Zigbee Cluster Library)     │
│  ├── genOnOff Cluster × 4 (Endpoint)│
│  ├── genBasic Cluster               │
│  ├── genIdentify Cluster            │
│  └── genGroups Cluster (Binding)    │
├─────────────────────────────────────┤
│  Z-Stack 3.0 协议栈                  │
│  ├── 网络管理 (NWK)                 │
│  ├── 应用支持子层 (APS)              │
│  ├── 设备对象 (ZDO)                 │
│  └── 路由功能 (Router)              │
├─────────────────────────────────────┤
│  HAL 硬件抽象层                      │
│  ├── GPIO 驱动                      │
│  ├── 定时器                         │
│  ├── 中断管理                        │
│  └── NV 存储 (Flash)                │
└─────────────────────────────────────┘
```

### 4.2 基于 DIYRuZ_RT 的修改策略

| DIYRuZ_RT 原有结构 | 修改目标 | 说明 |
|-------------------|----------|------|
| 1 个开关 Endpoint | 4 个开关 Endpoint | 每路独立控制 |
| 1 个温度 Endpoint | 移除 | 本设备无温度传感器 |
| 单路按键处理 | 4 路触摸 + 1 路配网按键 | 支持本地控制 |
| 无 LED 控制 | 增加 4 路 LED 反逻辑控制 | 状态指示 |
| 无断电记忆 | 增加 NV 存储开关状态 | **已确认实现** |
| 无 Binding | 增加 genGroups + Z2M Binding | **已确认：开关同步** |
| Router 功能 | 保留 | 作为 Zigbee 中继节点 |
| DS18B20 温度驱动 | 移除 | 无温度传感器 |
| OTA 升级 | 预留（本版本不实现） | Flash 分区预留 + Cluster 预留 |

---

## 5. Zigbee 设备模型

### 5.1 Endpoint 结构

```
Endpoint 1: genOnOff (第1路开关) → 继电器 1 / LED 1
Endpoint 2: genOnOff (第2路开关) → 继电器 2 / LED 2
Endpoint 3: genOnOff (第3路开关) → 继电器 3 / LED 3
Endpoint 4: genOnOff (第4路开关) → 继电器 4 / LED 4
```

### 5.2 Cluster 定义

每个 Endpoint 包含:

| Cluster | ID | 方向 | 属性 | 说明 |
|---------|-----|------|------|------|
| genBasic | 0x0000 | In | ModelIdentifier, ManufacturerName | 仅 EP1 包含完整属性 |
| genIdentify | 0x0003 | In | IdentifyTime | |
| genOnOff | 0x0006 | In/Out | onOff | 每路独立 |
| genGroups | 0x0004 | In/Out | — | 支持 Binding/群组控制 |

> **genBasic 优化**: 仅 Endpoint 1 包含完整的 genBasic 属性（ModelId、ManufacturerName 等），Endpoint 2~4 的 genBasic 仅包含 ClusterRevision 等最小属性，节省 Flash 空间。

### 5.3 设备标识

| 属性 | 值 | 说明 |
|------|-----|------|
| ModelIdentifier | `alab.switch` | 借壳 Alab 4路继电器板（长度前缀=11，无空格填充） |
| ManufacturerName | `TexasInstruments` | 保持默认（zigbeeModel 匹配不检查此字段） |
| DeviceID | `ZCL_HA_DEVICEID_ON_OFF_SWITCH` (0x0013) | Zigbee HA 标准开关 |

---

## 6. Zigbee2MQTT 接入方案

### 6.1 方案对比

| 方案 | ModelIdentifier | ManufacturerName | 是否需要 External Converter | 复杂度 | 风险 |
|------|----------------|-----------------|---------------------------|--------|------|
| **A: 借壳 alab.switch** | `alab.switch` | 任意 | ❌ 不需要 | 低 | 低（标准 ZCL，不依赖私有协议） |
| B: 自定义型号 | `DIY4GANG-01` | 自定义 | ✅ 需要 | 中 | 低（完全可控） |

### 6.2 借壳设备选型

#### 6.2.1 选型标准

借壳设备必须满足以下条件才能实现完整功能控制：

1. 使用标准 ZCL genOnOff 簇（`m.onOff()`），不使用 Tuya 私有 DP 协议
2. 不依赖 `tuya.configureMagicPacket` 或 `legacy.fz.tuya_switch`
3. Endpoint 映射为 `l1:1, l2:2, l3:3, l4:4`（4 个独立端点）

#### 6.2.2 推荐借壳设备一览

以下 5 个设备均使用标准 ZCL，不依赖 Tuya 私有协议：

| 优先级 | model | 厂商 | zigbeeModel | 匹配方式 | 功能数 | 描述 | 设备定义文件 |
|--------|-------|------|-------------|---------|--------|------|-------------|
| ⭐⭐⭐ | **alab.switch** | Alab | `alab.switch` | zigbeeModel | 最多 | Four channel relay board with four inputs | `src/devices/custom_devices_diy.ts` |
| ⭐⭐ | **TS0004** | Tuya | `TS0004` | zigbeeModel | 中 | Smart light switch - 4 gang with neutral wire | `src/devices/tuya.ts` |
| ⭐⭐ | **HGZB-4S** | Nue/3A | `LXN-4S27LX1.0` | zigbeeModel | 少 | Smart light switch - 4 gang v2.0 | `src/devices/nue_3a.ts` |
| ⭐ | **ZB-SW04** | eWeLink | `ZB-SW04` | zigbeeModel | 少 | Smart light switch - 4 gang | `src/devices/ewelink.ts` |
| ⭐ | **dqhome.re4** | DQHOME | `dqhome.re4` | zigbeeModel | 少 | DQSmart Switch 4 Gang | `src/devices/dqsmart.ts` |

> **注意**：`zigbeeModel` 匹配只需 ModelId 对上即可，ManufacturerName 可任意值。
>
> **Tuya TS0601 系列设备（如 Moes ZTS-EU_4gang、Zemismart TB26-4、Manhot MH03-4Z-OLED）不适用**：它们使用 Tuya 私有 DP 协议（cluster 0xEF00），非标准 genOnOff，标准 Z-Stack 固件无法控制。

#### 6.2.3 功能对比

| 功能 | alab.switch | TS0004 | HGZB-4S | ZB-SW04 | dqhome.re4 |
|------|:-----------:|:------:|:-------:|:-------:|:----------:|
| 4路开关 on/off | ✅ | ✅ | ✅ | ✅ | ✅ |
| 状态上报（内置） | ✅ | ✅ | ✅ | ✅ | ✅ |
| power_on_behavior | ❌ | ✅ | ✅ | ❌ | ❌ |
| backlight_mode | ❌ | ✅ | ❌ | ❌ | ❌ |
| switch_type | ❌ | ✅（条件） | ❌ | ❌ | ❌ |
| input_state（4路输入） | ✅ | ❌ | ❌ | ❌ | ❌ |
| commandsOnOff（命令事件） | ✅ | ❌ | ❌ | ❌ | ❌ |
| 依赖 Tuya 私有协议 | ❌ | ✅ tuyaBase | ❌ | ❌ | ❌ |

**功能说明**：

| 功能 | 方向 | 说明 | Z-Stack 可实现性 |
|------|------|------|-----------------|
| 4路开关 on/off | Z2M→设备 | zigbee2mqtt 控制继电器通断 | 需 4 个 EP + genOnOff 簇 |
| 状态上报 | 设备→Z2M | 继电器状态变化时自动上报（ZCL 标准 Reportable 属性） | genOnOff OnOff 属性内置 |
| power_on_behavior | Z2M→设备 | 断电重新上电后继电器恢复状态（off/previous/on） | genOnOff StartUpOnOff 属性 |
| backlight_mode | Z2M→设备 | 背光模式控制 | ❌ 依赖 Tuya 私有协议 |
| switch_type | Z2M→设备 | 开关类型（toggle/state/momentary） | ❌ 依赖 Tuya 私有协议 |
| input_state | 设备→Z2M | 上报物理按键按下/释放状态（genAnalogInput 簇） | 需 EP 5~8 + genAnalogInput 簇 |
| commandsOnOff | 设备→Z2M | 上报用户操作动作（on/off/toggle），可触发自动化 | genOnOff 命令发送 |

#### 6.2.4 选定方案: 借壳 alab.switch

**选择理由**：
- 功能最多：4路开关 + 4路输入状态 + 命令事件
- 完全不依赖 Tuya 私有协议：纯标准 ZCL（genOnOff + genAnalogInput）
- input_state + commandsOnOff 完美匹配"4路触摸按键智能开关"场景
- `zigbeeModel` 匹配，只需改 ModelId，ManufacturerName 可保持任意值

**固件端设置**（修改 `zcl_samplesw_data.c`）：

```c
// zigbeeModel 匹配只需 ModelId，ManufacturerName 可任意
const uint8 zclSampleSw_ManufacturerName[] = { 16, 'T','e','x','a','s','I','n','s','t','r','u','m','e','n','t','s' };
const uint8 zclSampleSw_ModelId[] = { 11, 'a','l','a','b','.','s','w','i','t','c','h' };
```

**ZCL 字符串格式**：`[长度字节][字符数据]`，长度前缀必须等于实际字符数，不能用空格填充。

**alab.switch 设备定义**（`src/devices/custom_devices_diy.ts`）：

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

**Endpoint 映射**：

| Endpoint | 功能 | 簇 | 说明 |
|----------|------|-----|------|
| EP 1 (l1) | 继电器 1 | genOnOff | 开关控制 + 状态上报 + 命令事件 |
| EP 2 (l2) | 继电器 2 | genOnOff | 同上 |
| EP 3 (l3) | 继电器 3 | genOnOff | 同上 |
| EP 4 (l4) | 继电器 4 | genOnOff | 同上 |
| EP 5 (in1) | 输入 1 | genAnalogInput | 按键状态上报 |
| EP 6 (in2) | 输入 2 | genAnalogInput | 按键状态上报 |
| EP 7 (in3) | 输入 3 | genAnalogInput | 按键状态上报 |
| EP 8 (in4) | 输入 4 | genAnalogInput | 按键状态上报 |

#### 6.2.5 如何查找其他借壳设备

**克隆仓库**：

```bash
git clone https://github.com/Koenkk/zigbee-herdsman-converters.git
```

**搜索方法**：

1. 用 VSCode 打开仓库，在 `src/devices/` 目录下搜索
2. 搜索 `m.onOff` + `l1: 1` + `l4: 4` 找到使用标准 ZCL 的 4 路开关
3. 确认不使用 `legacy.fz.tuya_switch`、`legacy.tz.tuya_switch_state`、`tuya.modernExtend.tuyaBase`
4. 查看设备图片：`https://www.zigbee2mqtt.io/devices/<model>.html`

**匹配方式区别**：
- `zigbeeModel`：只需 ModelId 匹配，ManufacturerName 可任意值。固件只改 1 个属性
- `fingerprint`：ModelId + ManufacturerName 必须同时精确匹配。固件需改 2 个属性

### 6.3 备选方案: 自定义 External Converter

如借壳方案不满足需求，使用 External Converter:

```javascript
// diy-4gang-switch.js
const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;

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
    endpoint: (device) => {
        return {l1: 1, l2: 2, l3: 3, l4: 4};
    },
    meta: {multiEndpoint: true},
    configure: async (device, coordinatorEndpoint, logger) => {
        for (const ep of [1, 2, 3, 4]) {
            const endpoint = device.getEndpoint(ep);
            if (!endpoint) continue;
            await reporting.bind(endpoint, coordinatorEndpoint, ['genOnOff']);
            await reporting.onOff(endpoint);
        }
    },
};

module.exports = definition;
```

---

## 7. 关键功能需求

### 7.1 本地控制

| 触发源 | 动作 | 结果 |
|--------|------|------|
| 触摸按键 1 | 按下 | 切换第1路继电器状态 + LED 反相 |
| 触摸按键 2 | 按下 | 切换第2路继电器状态 + LED 反相 |
| 触摸按键 3 | 按下 | 切换第3路继电器状态 + LED 反相 |
| 触摸按键 4 | 按下 | 切换第4路继电器状态 + LED 反相 |

> **触摸信号**: WTC6106BSI 输出持续高低电平，触摸时 OUT 拉低，松开恢复高电平。CC2530 使用 100ms 轮询检测下降沿（高→低）触发切换。

### 7.2 远程控制 (Zigbee)

| 命令 | 动作 | 结果 |
|------|------|------|
| OnOff: On (Endpoint 1) | 第1路开 | 继电器吸合 + LED 灭 |
| OnOff: Off (Endpoint 1) | 第1路关 | 继电器断开 + LED 亮 |
| OnOff: On (Endpoint 2) | 第2路开 | 继电器吸合 + LED 灭 |
| OnOff: Off (Endpoint 2) | 第2路关 | 继电器断开 + LED 亮 |
| ... | ... | ... |

### 7.3 状态上报

- 本地操作后主动上报状态变化
- 支持 Zigbee Reporting 机制
- 支持响应 Coordinator 的 Read 请求

### 7.4 配网流程

| 步骤 | 操作 | 说明 |
|------|------|------|
| 1 | 长按 S1 按键 5秒 | 进入配网模式 |
| 2 | LED 闪烁 | 配网中指示 |
| 3 | 协调器允许入网 | Zigbee2MQTT 端操作 |
| 4 | 入网成功 | LED 停止闪烁 |

---

## 8. Router 路由功能

### 8.1 功能说明

DIYRuZ_RT 固件默认以 **Router（路由器）** 角色加入 Zigbee 网络。本设备保留该功能，使 4 路智能开关同时具备 **Zigbee 信号中继** 能力。

### 8.2 Router 的优势

| 优势 | 说明 |
|------|------|
| 扩展网络覆盖 | 开关安装位置通常分布在房间各处，天然适合作为中继节点 |
| 增强网络稳定性 | 为电池供电的终端设备（如传感器）提供多条路由路径 |
| 无需额外设备 | 不增加硬件成本，固件层面保留即可 |
| 常供电优势 | 墙壁开关长期通电，不会因断电导致路由表丢失 |

### 8.3 实现要点

DIYRuZ_RT 基于 Z-Stack 3.0 的 Router 配置，主要涉及以下参数：

```c
// Z-Stack 设备类型配置
#define ZDO_COORDINATOR 0    // 不是协调器
#define RTR_NWK           1    // 启用路由功能

// 路由表配置
#define MAX_RTG_ENTRIES   40   // 路由表条目数
#define MAX_RREQ_ENTRIES  8    // 路由请求条目数
#define MAX_NEIGHBOR_ENTRIES 16 // 邻居表条目数
```

### 8.4 入网方式

作为 Router，设备通过以下方式加入网络：

| 方式 | 操作 | 适用场景 |
|------|------|---------|
| 按键配网 | 长按 S1 进入配网模式，协调器允许入网 | 首次安装 |
| 直接加入 | 通过已知网络密钥直接加入 | 批量部署 |

### 8.5 与 End Device 的区别

| 特性 | Router（本设备） | End Device（电池传感器） |
|------|-----------------|------------------------|
| 供电方式 | 常供电（火线） | 电池供电 |
| 路由功能 | ✅ 支持 | ❌ 不支持 |
| 网络角色 | 可转发数据包 | 仅收发自身数据 |
| 休眠机制 | 不休眠 | 周期性休眠 |
| 父节点依赖 | 不依赖 | 必须关联父节点 |

### 8.6 注意事项

| 注意点 | 说明 |
|--------|------|
| 网络容量 | 单个 Zigbee 网络理论支持 65000+ 节点，实际受协调器性能限制 |
| 路由深度 | 最大 15 跳，普通家庭环境通常不超过 3~5 跳 |
| 固件兼容性 | 保留 DIYRuZ_RT 的 ZDO 配置，无需额外修改网络层代码 |

---

## 9. 断电记忆功能

### 9.1 功能说明

断电记忆功能指开关在断电后再次上电时，自动恢复断电前的开关状态。这是智能墙壁开关的常见需求，避免停电后来电时所有灯突然亮起或全部关闭的尴尬情况。

### 9.2 功能确认

**已确认实现**。断电记忆是智能墙壁开关的核心体验功能，与主流产品保持一致。

### 9.3 实现方案

CC2530 内部有 **Flash 存储器**，可利用 NV (Non-Volatile) 存储机制保存开关状态。

DIYRuZ_RT 原有代码已实现单路继电器状态的 NV 存储（NV ID: 0x0402），扩展为 4 路即可。

**存储内容**:
```c
// NV 存储项: 4路开关状态 (1 byte, 每bit代表1路)
// bit0=第1路, bit1=第2路, bit2=第3路, bit3=第4路
// 0=OFF, 1=ON
#define NV_DIYRuZRT_RELAY_STATE_ID  0x0402
uint8 RELAY_STATE;  // 4路状态打包在1字节中
```

> **简化方案**: 直接复用现有 `RELAY_STATE` 变量，从 1 bit 扩展为 4 bit（每路 1 bit），无需新增 NV ID。首次上电默认全关（RELAY_STATE=0）。

**触发保存时机**:
- 每次开关状态变化后延迟 1~2 秒保存（避免频繁写入 Flash，使用定时器事件触发）

**上电恢复流程**:
```c
void zclDIYRuZRT_Init(byte task_id) {
    // ... 其他初始化 ...
    // 读取 NV 中的继电器状态
    if (SUCCESS == osal_nv_item_init(NV_DIYRuZRT_RELAY_STATE_ID, 1, &RELAY_STATE)) {
        osal_nv_read(NV_DIYRuZRT_RELAY_STATE_ID, 0, 1, &RELAY_STATE);
    }
    // 应用4路继电器和LED状态
    applyRelayAll();
}
```

### 9.4 注意事项

| 注意点 | 说明 |
|--------|------|
| Flash 擦写寿命 | CC2530 Flash 约 10 万次擦写，延迟写入策略可大幅降低写入频率 |
| 写入延迟 | Flash 写入期间设备可能无响应，使用定时器延迟写入避免阻塞 |
| 首次上电 | 无 NV 数据时默认全关（RELAY_STATE=0）→ 继电器断开 + LED 亮 |

---

## 10. 开关同步功能 (Z2M Binding)

### 10.1 功能说明

开关同步功能允许两个或多个 Zigbee 开关通过 Binding 实现状态联动，无需通过协调器中转。例如：走廊两端的开关控制同一盏灯，按任意一端开关，两端状态同步变化。

### 10.2 实现方案

通过 Zigbee Binding 机制 + genGroups Cluster 实现。Z2M 端配置 Binding，设备端支持 genGroups Cluster 即可。

**设备端需要**:
1. **genGroups Cluster (0x0004)** — 已在 DIYRuZ_RT 的 InClusterList 中，保留即可
2. **BDB_COMMISSIONING_MODE_FINDING_BINDING** — 源码中已启用
3. **Binding 响应** — Z-Stack 3.0 协议栈自动处理 Bind Request/Response

### 10.3 工作原理

```
┌──────────┐   Binding Table   ┌──────────┐
│  开关 A   │◄────────────────►│  开关 B   │
│ EP1: OnOff│   直连通信        │ EP1: OnOff│
└──────────┘  (不经协调器)     └──────────┘

Z2M 配置: zigbee2mqtt/bridge/request/device/bind
```

**Z2M 端配置示例**:
```json
{
    "from": "开关A/1",
    "to": "开关B/1"
}
```

### 10.4 关键实现点

| 要点 | 说明 |
|------|------|
| genGroups Cluster | 保留在 InClusterList 中，支持 Add Group / Remove Group 等命令 |
| Binding Table | Z-Stack 3.0 自动维护，应用层无需额外处理 |
| 本地控制联动 | 本地按键切换后上报 OnOff 状态，绑定的对端设备自动收到 |
| 无需 genScenes | 开关同步不需要场景功能，保持简洁 |

---

## 11. OnOff 回调与 Endpoint 路由

### 11.1 问题描述

DIYRuZ_RT 原有代码只有 1 个 Endpoint，`zclDIYRuZRT_OnOffCB(uint8 cmd)` 回调不需要区分 Endpoint。扩展为 4 个 Endpoint 后，OnOff 命令回调需要知道目标 Endpoint 才能控制对应的继电器。

### 11.2 Z-Stack 回调机制分析

Z-Stack 3.0 的 OnOff 回调机制：

1. **回调注册绑定 Endpoint**: `zclGeneral_RegisterCmdCallbacks(endpoint, &callbacks)` 注册时指定了 Endpoint 编号
2. **ZCL 层自动路由**: Z-Stack 收到 OnOff 命令后，根据目标 Endpoint 查找对应的回调注册表，**只调用匹配 Endpoint 的回调**
3. **回调签名不含 Endpoint**: `void (*OnOffCB)(uint8 cmd)` — 不传 Endpoint 参数
4. **可通过全局变量获取**: 回调内通过 `zcl_getRawAFMsg()` 可获取原始 AF 消息，其中包含 `endPoint` 字段

### 11.3 多 Endpoint 实现方案

**方案: 为每个 Endpoint 注册独立的回调函数**

```c
// 4个独立的 OnOff 回调，每个回调"知道"自己属于哪个 Endpoint
static void zclDIYRuZRT_OnOffCB_EP1(uint8 cmd) { handleOnOff(1, cmd); }
static void zclDIYRuZRT_OnOffCB_EP2(uint8 cmd) { handleOnOff(2, cmd); }
static void zclDIYRuZRT_OnOffCB_EP3(uint8 cmd) { handleOnOff(3, cmd); }
static void zclDIYRuZRT_OnOffCB_EP4(uint8 cmd) { handleOnOff(4, cmd); }

// 4组回调结构体
static zclGeneral_AppCallbacks_t zclDIYRuZRT_CmdCallbacks_EP1 = {
    NULL, NULL, zclDIYRuZRT_OnOffCB_EP1, NULL, NULL, NULL, ...
};
// ... EP2, EP3, EP4 类似

// 初始化时分别注册
zclGeneral_RegisterCmdCallbacks(1, &zclDIYRuZRT_CmdCallbacks_EP1);
zclGeneral_RegisterCmdCallbacks(2, &zclDIYRuZRT_CmdCallbacks_EP2);
zclGeneral_RegisterCmdCallbacks(3, &zclDIYRuZRT_CmdCallbacks_EP3);
zclGeneral_RegisterCmdCallbacks(4, &zclDIYRuZRT_CmdCallbacks_EP4);
```

**核心处理函数**:
```c
static void handleOnOff(uint8_t ep, uint8_t cmd) {
    uint8_t ch = ep - 1;  // Endpoint 1~4 → 通道 0~3

    if (cmd == COMMAND_ON) {
        updateRelay(ch, TRUE);
    } else if (cmd == COMMAND_OFF) {
        updateRelay(ch, FALSE);
    } else if (cmd == COMMAND_TOGGLE) {
        updateRelay(ch, !(RELAY_STATE & BV(ch)));
    }
}
```

> **验证建议**: 首次调试时，在每个回调中加入 LED 闪烁或串口打印，确认 Z-Stack 确实按 Endpoint 分别调用。

---

## 12. OTA 升级预留方案

### 12.1 策略

**本版本不实现 OTA 功能，仅做 Flash 分区预留和 Cluster 注册预留**，后续版本再完成 OTA 实现。

### 12.2 CC2530F256 Flash 分区现状

CC2530F256 共 256KB Flash（128 页，每页 2KB），当前分区：

| 区域 | 页范围 | 大小 | 地址 | 说明 |
|------|--------|------|------|------|
| Boot Loader | 0~3 | 8KB | 0x0000~0x1FFF | Serial Boot Loader（已预留） |
| 用户代码 | 4~120 | ~234KB | 0x2000~0x78000 | 应用程序 + Z-Stack |
| NV 存储 | 121~126 | 12KB | — | 6 页 NV |
| Lock Bits | 127 | 2KB | — | Flash 锁定位 |

### 12.3 OTA 预留方案

后续实现 OTA 时，需要从用户代码区末尾划分出 OTA Image 区域：

| 区域 | 页范围 | 大小 | 说明 |
|------|--------|------|------|
| Boot Loader | 0~3 | 8KB | 已有，需升级为 OTA Boot Loader |
| 用户代码 | 4~60 | ~114KB | 当前固件（含 Z-Stack 约占 80~100KB，有富裕） |
| OTA Image | 61~120 | ~120KB | OTA 下载区（预留） |
| NV 存储 | 121~126 | 12KB | 不变 |
| Lock Bits | 127 | 2KB | 不变 |

### 12.4 本版本预留内容

1. **xcl 链接脚本注释**: 在 IAR 项目 xcl 文件中添加 OTA 分区注释，标记 0x1E000 为 OTA Image 起始地址
2. **OTA Cluster 注册**: 在 Endpoint 1 添加 `ZCL_CLUSTER_ID_OTA` (0x0019) 的 InCluster，使设备在 Z2M 端显示支持 OTA
3. **编译开关**: 添加 `#define ZCL_OTA 0` 宏，后续设为 1 即可启用

```c
// preinclude.h 预留
#define ZCL_OTA  0  // 0=预留, 1=启用 OTA

// zcl_DIYRuZRT_data.c 中 InClusterList 预留
#if ZCL_OTA
  ZCL_CLUSTER_ID_OTA,
#endif
```

---

## 13. GPIO 引脚分配表

| 功能 | CC2530 引脚 | 方向 | 初始状态 | 备注 |
|------|-------------|------|----------|------|
| LED 1 | P0_0 | 输出 | HIGH (亮) | 反逻辑：继电器 OFF → LED 亮 |
| LED 2 | P0_1 | 输出 | HIGH (亮) | 反逻辑：继电器 OFF → LED 亮 |
| LED 3 | P0_2 | 输出 | HIGH (亮) | 反逻辑：继电器 OFF → LED 亮 |
| LED 4 | P0_3 | 输出 | HIGH (亮) | 反逻辑：继电器 OFF → LED 亮 |
| 触摸输入 1 | P0_4 | 输入 | 上拉 | 低电平有效（触摸拉低） |
| 触摸输入 2 | P0_5 | 输入 | 上拉 | 低电平有效（触摸拉低） |
| 触摸输入 3 | P0_6 | 输入 | 上拉 | 低电平有效（触摸拉低） |
| 触摸输入 4 | P0_7 | 输入 | 上拉 | 低电平有效（触摸拉低） |
| 继电器 1 | P1_0 | 输出 | HIGH (断开) | 低电平触发吸合 |
| 继电器 2 | P1_2 | 输出 | HIGH (断开) | 低电平触发吸合 |
| 继电器 3 | P1_6 | 输出 | HIGH (断开) | 低电平触发吸合 |
| 继电器 4 | P2_0 | 输出 | HIGH (断开) | 低电平触发吸合 |
| 配网按键 S1 | P1_3 | 输入 | 上拉 | 低电平有效 |

> **上电初始状态说明**: Pull-up 使 GPIO 默认 HIGH → 继电器全部断开（OFF）+ LED 全部亮。对应 Zigbee 报文 `OnOff: Off`。

---

## 14. 开发环境

| 工具 | 版本/型号 | 用途 |
|------|----------|------|
| IAR Embedded Workbench | for 8051 | 编译 Z-Stack 固件 |
| Z-Stack 3.0.2 | TI 官方 | Zigbee 协议栈 |
| DIYRuZ_RT 源码 | GitHub | 固件框架基础 |
| SmartRF Flash Programmer | TI 官方 | 烧录固件 |
| CC Debugger | 或兼容工具 | 调试器 |

---

## 15. 功能确认汇总

| 功能 | 状态 | 说明 |
|------|------|------|
| 4 路 OnOff 开关 | ✅ 确认 | 4 Endpoint × genOnOff |
| Router 路由功能 | ✅ 确认 | 保留 DIYRuZ_RT 原有 Router 功能 |
| 断电记忆 | ✅ 确认 | NV 存储 4 路状态，上电恢复 |
| 开关同步 (Binding) | ✅ 确认 | genGroups Cluster + Z2M Binding |
| LED 反逻辑 | ✅ 确认 | 继电器 OFF→LED 亮，继电器 ON→LED 灭 |
| 触摸本地控制 | ✅ 确认 | WTC6106BSI 高低电平输出，轮询检测 |
| 配网按键 | ✅ 确认 | S1 长按 5 秒进入配网 |
| 借壳 TS0004 | ✅ 确认 | Z2M 自动识别，备选 External Converter |
| OTA 升级 | ⏳ 预留 | Flash 分区预留 + Cluster 预留，后续版本实现 |
| 温度传感器 | ❌ 移除 | 本设备无温度传感器，移除 DS18B20 相关代码 |

---

## 16. 参考资料

| 资源 | 链接 |
|------|------|
| DIYRuZ_RT 源码 | https://github.com/diyruz/diyruz_rt |
| zigbee-herdsman-converters | https://github.com/Koenkk/zigbee-herdsman-converters |
| Zigbee2MQTT 文档 | https://www.zigbee2mqtt.io/ |
| TI Z-Stack 文档 | https://www.ti.com/product/CC2530 |
| WTC6106BSI 数据手册 | https://m.elecfans.com/article/1228598.html |
| PTVO 固件配置工具 | https://ptvo.info/ （闭源，仅作参考） |

---

*文档生成时间: 2026-07-17*
*版本: v2.0*
