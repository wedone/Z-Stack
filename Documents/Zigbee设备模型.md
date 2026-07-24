# Zigbee 设备模型

## Endpoint 结构

```
Endpoint 1: genOnOff (第1路开关) → 继电器 1 / LED 1
Endpoint 2: genOnOff (第2路开关) → 继电器 2 / LED 2
Endpoint 3: genOnOff (第3路开关) → 继电器 3 / LED 3
Endpoint 4: genOnOff (第4路开关) → 继电器 4 / LED 4
```

> 预留：Endpoint 5-8 可用于 genAnalogInput（触摸按键状态上报），匹配 alab.switch 完整功能，后续版本实现。

## Cluster 定义

每个 Endpoint 包含:

| Cluster | ID | 方向 | 属性 | 说明 |
|---------|-----|------|------|------|
| genBasic | 0x0000 | In | ModelIdentifier, ManufacturerName, DateCode, SwBuildId | 仅 EP1 包含完整属性 |
| genIdentify | 0x0003 | In | IdentifyTime | |
| genOnOff | 0x0006 | In/Out | onOff | 每路独立 |
| genGroups | 0x0004 | In/Out | — | 支持 Binding/群组控制 |

> **genBasic 优化**: 仅 Endpoint 1 包含完整的 genBasic 属性（ModelId、ManufacturerName 等），Endpoint 2~4 的 genBasic 仅包含 ClusterRevision 等最小属性，节省 Flash 空间。

## 设备标识

| 属性 | 值 | 说明 |
|------|-----|------|
| ModelIdentifier | `alab.switch` | 借壳 Alab 4路继电器板（长度前缀=11，无空格填充） |
| ManufacturerName | `TexasInstruments` | 保持默认（zigbeeModel 匹配不检查此字段） |
| DeviceID | `ZCL_HA_DEVICEID_ON_OFF_SWITCH` (0x0013) | Zigbee HA 标准开关 |
| SwBuildId | `v0.1.0` | 固件版本号 |
| DateCode | `20260724` | 编译日期 |

ZCL 字符串格式：`[长度字节][字符数据]`，长度前缀必须等于实际字符数，不能用空格填充。

---

## OnOff 回调与多 Endpoint 路由

### 问题背景

Z-Stack 3.0 的 OnOff 回调机制：

1. **回调注册绑定 Endpoint**: `zclGeneral_RegisterCmdCallbacks(endpoint, &callbacks)` 注册时指定了 Endpoint 编号
2. **ZCL 层自动路由**: Z-Stack 收到 OnOff 命令后，根据目标 Endpoint 查找对应的回调注册表，**只调用匹配 Endpoint 的回调**
3. **回调签名不含 Endpoint**: `void (*OnOffCB)(uint8 cmd)` — 不传 Endpoint 参数

### 实现方案：为每个 Endpoint 注册独立的回调函数

```c
// 4个独立的 OnOff 回调，每个回调"知道"自己属于哪个 Endpoint
static void zclSampleSw_OnOffCB_EP1(uint8 cmd) { zclSampleSw_HandleOnOffCmd(0, cmd); }
static void zclSampleSw_OnOffCB_EP2(uint8 cmd) { zclSampleSw_HandleOnOffCmd(1, cmd); }
static void zclSampleSw_OnOffCB_EP3(uint8 cmd) { zclSampleSw_HandleOnOffCmd(2, cmd); }
static void zclSampleSw_OnOffCB_EP4(uint8 cmd) { zclSampleSw_HandleOnOffCmd(3, cmd); }

// 4组回调结构体
static zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep1 = { ... };
// ... EP2, EP3, EP4 类似

// 初始化时分别注册
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY1, &zclSampleSw_CmdCallbacks_ep1);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY2, &zclSampleSw_CmdCallbacks_ep2);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY3, &zclSampleSw_CmdCallbacks_ep3);
zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY4, &zclSampleSw_CmdCallbacks_ep4);
```

### ZCL 状态上报

触摸翻转或 ZCL 命令处理后，调用 `zclSampleSw_ReportOnOffState(idx)` 主动向协调器上报 OnOff 属性：

```c
static void zclSampleSw_ReportOnOffState(uint8 idx)
{
  // 根据 idx 选择对应 Endpoint
  // 构造 zclReportCmd_t，属性=ATTRID_ON_OFF，数据类型=BOOLEAN
  // 发送到协调器(短地址0)，ZCL_FRAME_SERVER_CLIENT_DIR
  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ON_OFF,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);
}
```

> 需要 `ZCL_REPORTING_DEVICE` 宏定义才能启用 `zcl_SendReportCmd` 函数。
