# Zigbee 设备模型

## 概述

本项目借壳 HGZB-4S（Nue / 3A）接入 Zigbee2MQTT（Z2M），通过 `ModelIdentifier = LXN-4S27LX1.0` 匹配 Z2M 内置转换器，**免 External Converter**。设备作为 Router 角色加入网络，提供 4 路开关端点（genOnOff）和 4 路输入状态端点（genAnalogInput）。

## 借壳信息

| 项目 | 值 | 说明 |
|------|-----|------|
| 借壳型号 | HGZB-4S | Z2M 设备 model |
| 厂商 | Nue / 3A | Z2M 设备 vendor |
| zigbeeModel | `LXN-4S27LX1.0` | Z2M 匹配键（ModelIdentifier） |
| 描述 | Smart light switch - 4 gang v2.0 | Z2M 设备描述 |
| External Converter | 不需要 | Z2M 内置匹配 |

### Z2M 设备定义（nue_3a.ts）

```javascript
{
    zigbeeModel: ["LXN-4S27LX1.0"],
    model: "HGZB-4S",
    vendor: "Nue / 3A",
    description: "Smart light switch - 4 gang v2.0",
    extend: [m.deviceEndpoints({endpoints: {l1: 1, l2: 2, l3: 3, l4: 4}}),
             m.onOff({endpointNames: ["l1", "l2", "l3", "l4"]})],
}
```

## Endpoint 端点分配

| Endpoint | 功能 | Cluster | DeviceID | Z2M 识别 | 说明 |
|----------|------|---------|----------|:---:|------|
| EP 1 (l1) | 继电器 1 | genOnOff (0x0006) | ZCL_HA_DEVICEID_ON_OFF_LIGHT | ✅ | 对应 z2m l1 |
| EP 2 (l2) | 继电器 2 | genOnOff (0x0006) | ZCL_HA_DEVICEID_ON_OFF_LIGHT | ✅ | 对应 z2m l2 |
| EP 3 (l3) | 继电器 3 | genOnOff (0x0006) | ZCL_HA_DEVICEID_ON_OFF_LIGHT | ✅ | 对应 z2m l3 |
| EP 4 (l4) | 继电器 4 | genOnOff (0x0006) | ZCL_HA_DEVICEID_ON_OFF_LIGHT | ✅ | 对应 z2m l4 |
| EP 5 | 输入状态 1 | genAnalogInput (0x000C) | ZCL_HA_DEVICEID_SIMPLE_SENSOR | ❌ Z2M 忽略 | 触摸按键 1 状态 |
| EP 6 | 输入状态 2 | genAnalogInput (0x000C) | ZCL_HA_DEVICEID_SIMPLE_SENSOR | ❌ Z2M 忽略 | 触摸按键 2 状态 |
| EP 7 | 输入状态 3 | genAnalogInput (0x000C) | ZCL_HA_DEVICEID_SIMPLE_SENSOR | ❌ Z2M 忽略 | 触摸按键 3 状态 |
| EP 8 | 输入状态 4 | genAnalogInput (0x000C) | ZCL_HA_DEVICEID_SIMPLE_SENSOR | ❌ Z2M 忽略 | 触摸按键 4 状态 |
| EP 11 | genBasic 端点 | genBasic (0x0000) 等 | ZCL_HA_DEVICEID_ON_OFF_LIGHT_SWITCH | ✅ | ModelId/ManufacturerName 等 |

> **EP11 由来（v1.0.3 修复 BUG-013）**: 原 `SAMPLESW_ENDPOINT=8` 与 `SAMPLESW_ENDPOINT_INPUT4=8` 冲突，导致 EP8 的 SimpleDescriptor 被 input4 覆盖，Z2M 找不到 genBasic cluster，interview 失败。改为 EP11 避开 EP1-8（继电器 EP1-4 + 输入状态 EP5-8），Z2M 在 EP11 上读取 genBasic 完成 interview。

### Endpoint 宏定义（zcl_samplesw.h）

```c
// genBasic 端点 (EP11, 避开 EP1-8 冲突)
#define SAMPLESW_ENDPOINT               11

// 4路继电器端点 (EP 1-4, 对应 z2m 的 l1-l4)
#define SAMPLESW_ENDPOINT_RELAY1        1
#define SAMPLESW_ENDPOINT_RELAY2        2
#define SAMPLESW_ENDPOINT_RELAY3        3
#define SAMPLESW_ENDPOINT_RELAY4        4
#define SAMPLESW_NUM_RELAYS             4

// 4路输入状态端点 (EP 5-8, 对应 alab.switch 的 in1-in4)
#define SAMPLESW_ENDPOINT_INPUT1        5
#define SAMPLESW_ENDPOINT_INPUT2        6
#define SAMPLESW_ENDPOINT_INPUT3        7
#define SAMPLESW_ENDPOINT_INPUT4        8
#define SAMPLESW_NUM_INPUTS             4
```

## Cluster 结构

### EP1-4（继电器端点）

| Cluster | ID | 方向 | 属性 | 说明 |
|---------|-----|------|------|------|
| genOnOff | 0x0006 | In/Out | onOff, startUpOnOff | 每路独立 |

### EP5-8（输入状态端点）

| Cluster | ID | 方向 | 属性 | 类型 | 说明 |
|---------|-----|------|------|------|------|
| genAnalogInput | 0x000C | In | presentValue | single_float (0x39) | 1.0f=触摸中, 0.0f=未触摸 |

### EP11（genBasic 端点）

| Cluster | ID | 方向 | 属性 | 说明 |
|---------|-----|------|------|------|
| genBasic | 0x0000 | In | ZCLVersion, HWVersion, ManufacturerName, ModelId, DateCode, SwBuildId, PowerSource 等 | 完整属性 |
| genIdentify | 0x0003 | In | IdentifyTime | — |
| genOnOffSwitchConfig | 0x0007 | In | SwitchType, SwitchActions | — |
| genOnOff | 0x0006 | Out | — | OutCluster |
| genGroups | 0x0004 | Out | — | OutCluster，支持 Binding |

> **genBasic 优化**: 仅 EP11 包含完整的 genBasic 属性，EP1-4 的 genBasic 仅包含 ClusterRevision 等最小属性，节省 Flash 空间。

## ZCL 属性定义

### genBasic 属性（EP11 完整属性）

| 属性 | AttrID | 变量名 | 类型 | 当前值 | 长度前缀 | 可改? |
|------|--------|--------|------|--------|----------|-------|
| ZCLVersion | 0x0000 | `zclSampleSw_ZCLVersion` | uint8 | 0 | 无 | ✅ |
| HWVersion | 0x0003 | `zclSampleSw_HWRevision` | uint8 | 2 | 无 | ✅ |
| ManufacturerName | 0x0004 | `zclSampleSw_ManufacturerName` | ZCL字符串 | `Linxee` | 6 | ✅ |
| ModelIdentifier | 0x0005 | `zclSampleSw_ModelId` | ZCL字符串 | `LXN-4S27LX1.0` | 13 | ❌ 不可改 |
| DateCode | 0x0006 | `zclSampleSw_DateCode` | ZCL字符串 | `20260731` | 8 | ✅ 每次发版更新 |
| SwBuildId | 0x4000 | `zclSampleSw_SwBuildId` | ZCL字符串 | `HA-SPA4C1-1.0.10` | 16 | ✅ 每次发版更新 |
| PowerSource | 0x0007 | `zclSampleSw_PowerSource` | ENUM8 | 1（单相交流电） | 无 | ✅ |

### 属性代码定义（zcl_samplesw_data.c）

```c
// 硬件版本 (uint8, 无长度前缀)
#define SAMPLESW_HWVERSION          2
const uint8 zclSampleSw_HWRevision = SAMPLESW_HWVERSION;

// ManufacturerName: 真实厂商名 (ZCL字符串, 6字符)
const uint8 zclSampleSw_ManufacturerName[] = { 6, 'L','i','n','x','e','e' };

// ModelId: 借壳匹配键, 不可改 (ZCL字符串, 13字符)
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };

// DateCode: 固件编译日期 (ZCL字符串, 8字符, YYYYMMDD格式)
const uint8 zclSampleSw_DateCode[] = { 8, '2','0','2','6','0','7','3','1' };

// SwBuildId: 型号+版本号 (ZCL字符串, 16字符)
// v1.0.10: 去掉 V 前缀, 避免 ZCL Read Attrs Rsp 超 MTU 被丢弃
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','1','.','0','.','1','0' };

// PowerSource: 单相交流电
const uint8 zclSampleSw_PowerSource = POWER_SOURCE_MAINS_1_PHASE;
```

### 转换器覆盖项（固件无法控制）

以下字段由 `zigbee-herdsman-converters/src/devices/nue_3a.ts` 的转换器定义，固件上报的 ZCL 属性值被忽略：

| 显示字段 | 固件属性 | Z2M 显示值 | 转换器来源 |
|----------|----------|-----------|-----------|
| 型号 | ModelIdentifier | `HGZB-4S` | `nue_3a.ts` `model` |
| 制造商（HA 侧） | ManufacturerName | `Nue / 3A` | `nue_3a.ts` `vendor` |
| 描述 | — | `Smart light switch - 4 gang v2.0` | `nue_3a.ts` `description` |

## ZCL 字符串格式

> **ZCL 字符串格式**：第一字节为长度前缀，后接字符数据，**不含终止符 `\0`**。

### 格式说明

```
[长度字节][字符1][字符2]...[字符N]
```

- 长度字节**必须等于**实际字符数
- 不能用空格填充
- 不含 C 字符串终止符 `\0`

### 示例

```c
// 正确: 长度=6, 6个字符
const uint8 zclSampleSw_ManufacturerName[] = { 6, 'L','i','n','x','e','e' };

// 正确: 长度=16, 16个字符
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','1','.','0','.','1','0' };

// 正确: 长度=13, 13个字符 (ModelId, 借壳匹配键, 不可改)
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };

// 错误: 长度与字符数不符
const uint8 zclSampleSw_ManufacturerName[] = { 8, 'L','i','n','x','e','e' };  // 长度8但只有6字符
```

### 各字符串长度规范

| 属性 | 字符数 | 长度前缀 | ZCL 限制 |
|------|--------|----------|----------|
| ManufacturerName | 6 | 6 | ≤32 字符 |
| ModelId | 13 | 13 | ≤32 字符 |
| DateCode | 8 | 8 | ≤16 字符 |
| SwBuildId | 16 | 16 | ≤16 字符 |

> **SwBuildId 长度限制**: v1.0.10 起从 17 字符缩为 16 字符（去掉版本号前的 `V` 前缀），避免 ZCL Read Attributes Response 超 MTU 被丢弃。格式为 `HA-SPA4C1-X.Y.Z`（共 16 字符）。

## startUpOnOff 属性（断电记忆）

### 属性定义

`genOnOff` Cluster 的 `startUpOnOff` 属性（ATTRID = 0x4003，ENUM8）控制 4 路开关断电恢复行为：

| 值 | 名称 | 行为 |
|----|------|------|
| 0x00 | OFF | 上电后该路为 OFF |
| 0x01 | ON | 上电后该路为 ON |
| 0x02 | TOGGLE | 上电后翻转断电前状态 |
| 0xFF | PREVIOUS | 恢复断电前状态（默认） |

### 4 路独立配置（v0.2.2 修复 BUG-009）

```c
// 4路独立 startUpOnOff 配置 (默认恢复之前状态)
uint8 zclSampleSw_StartUpOnOff[SAMPLESW_NUM_RELAYS] = {
    STARTUP_ONOFF_PREVIOUS,
    STARTUP_ONOFF_PREVIOUS,
    STARTUP_ONOFF_PREVIOUS,
    STARTUP_ONOFF_PREVIOUS
};

// 4个 EP 属性表分别指向独立数组元素
CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep1[] = {
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_ON_OFF, ZCL_DATATYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_RelayState[0] } },
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_STARTUP_ON_OFF, ZCL_DATATYPE_ENUM8, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_StartUpOnOff[0] } },
};
// ep2/ep3/ep4 类似, 分别指向 [1]/[2]/[3]
```

### NV 持久化

| NV ID | 大小 | 内容 | 说明 |
|-------|------|------|------|
| 0x0F10 | 4 字节 | 4 路继电器状态 | 断电前状态，用于 PREVIOUS/TOGGLE 恢复 |
| 0x0F12 | 4 字节 | 4 路独立 startUpOnOff 配置 | v0.2.2 起（旧 ID 0x0F11 已废弃） |

```c
// NV ID 定义 (zcl_samplesw.h)
#define SAMPLESW_NV_ID_RELAY_STATE          0x0F10  // 4路继电器状态
#define SAMPLESW_NV_ID_STARTUP_ONOFF        0x0F12  // 4路独立startUpOnOff配置

// startUpOnOff 属性值
#define STARTUP_ONOFF_OFF                   0x00
#define STARTUP_ONOFF_ON                    0x01
#define STARTUP_ONOFF_TOGGLE                0x02
#define STARTUP_ONOFF_PREVIOUS              0xFF

// 属性ID
#define ATTRID_STARTUP_ON_OFF               0x4003
```

> **避坑（BUG-009）**: v0.2.0/v0.2.1 误用 4 路共用单变量 `zclSampleSw_StartUpOnOff`，Z2M 写入 4 路独立配置时互相覆盖。v0.2.2 改为 `uint8[4]` 数组，4 EP 属性表分别指向独立元素。

## OnOff 回调与多 Endpoint 路由

Z-Stack 3.0 的 OnOff 回调机制要求为每个 Endpoint 注册独立的回调函数：

```c
// 4个独立的 OnOff 回调, 每个回调"知道"自己属于哪个 Endpoint
static void zclSampleSw_OnOffCB_EP1(uint8 cmd) { zclSampleSw_HandleOnOffCmd(0, cmd); }
static void zclSampleSw_OnOffCB_EP2(uint8 cmd) { zclSampleSw_HandleOnOffCmd(1, cmd); }
static void zclSampleSw_OnOffCB_EP3(uint8 cmd) { zclSampleSw_HandleOnOffCmd(2, cmd); }
static void zclSampleSw_OnOffCB_EP4(uint8 cmd) { zclSampleSw_HandleOnOffCmd(3, cmd); }

// 初始化时分别注册
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY1, &zclSampleSw_CmdCallbacks_ep1);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY2, &zclSampleSw_CmdCallbacks_ep2);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY3, &zclSampleSw_CmdCallbacks_ep3);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY4, &zclSampleSw_CmdCallbacks_ep4);
```

## ZCL 状态上报

触摸翻转或 ZCL 命令处理后，调用 `zclSampleSw_ReportOnOffState(idx)` 主动向协调器上报 OnOff 属性：

```c
static void zclSampleSw_ReportOnOffState(uint8 idx)
{
  // 根据 idx 选择对应 Endpoint
  // 构造 zclReportCmd_t, 属性=ATTRID_ON_OFF, 数据类型=BOOLEAN
  // 发送到协调器(短地址0), ZCL_FRAME_SERVER_CLIENT_DIR
  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ON_OFF,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);
}
```

> 需要 `ZCL_REPORTING_DEVICE` 宏定义才能启用 `zcl_SendReportCmd` 函数。

## 状态同步机制

| 机制 | 触发时机 | 实现 |
|------|----------|------|
| 入网立即上报 | ZDO 状态变为 `DEV_ROUTER`（入网成功） | 调度 `SAMPLESW_STATE_REPORT_EVT` 事件，上报 4 路 OnOff 状态 |
| 周期性上报 | 每 30 秒 | `STATE_REPORT_INTERVAL_MS = 30000ms` 定时器，周期触发上报 |

```c
#define SAMPLESW_STATE_REPORT_EVT  0x2000      // 状态上报事件
#define STATE_REPORT_INTERVAL_MS   30000       // 周期 30 秒

// ZDO 状态变化回调: 入网时立即上报 + 启动周期定时器
if (state == DEV_ROUTER) {
    osal_set_event(zclSampleSw_TaskID, SAMPLESW_STATE_REPORT_EVT);
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_STATE_REPORT_EVT, STATE_REPORT_INTERVAL_MS);
}

// 上报 4 路状态
static void zclSampleSw_ReportAllOnOffState(void)
{
    uint8 i;
    for (i = 0; i < SAMPLESW_NUM_RELAYS; i++) {
        zclSampleSw_ReportOnOffState(i);
    }
}
```

## 参考文件

- `Documents/Zigbee设备模型.md` — 完整设备模型说明
- `Documents/Z2M接入方案.md` — 借壳方案与 Z2M 接入
- `Projects/zstack/HomeAutomation/HGZBSwitch/Source/zcl_samplesw.h` — Endpoint 与属性宏定义
- `Projects/zstack/HomeAutomation/HGZBSwitch/Source/zcl_samplesw_data.c` — ZCL 属性表与 SimpleDescriptor
