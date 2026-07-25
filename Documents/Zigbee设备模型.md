# Zigbee 设备模型

## Endpoint 结构

```
Endpoint 1: genOnOff (第1路开关) → 继电器 1 / LED 1
Endpoint 2: genOnOff (第2路开关) → 继电器 2 / LED 2
Endpoint 3: genOnOff (第3路开关) → 继电器 3 / LED 3
Endpoint 4: genOnOff (第4路开关) → 继电器 4 / LED 4
Endpoint 5: genAnalogInput (第1路输入状态) → 触摸按键 1
Endpoint 6: genAnalogInput (第2路输入状态) → 触摸按键 2
Endpoint 7: genAnalogInput (第3路输入状态) → 触摸按键 3
Endpoint 8: genAnalogInput (第4路输入状态) → 触摸按键 4
```

## Cluster 定义

每个 Endpoint 包含:

| Cluster | ID | 方向 | 属性 | 说明 |
|---------|-----|------|------|------|
| genBasic | 0x0000 | In | ModelIdentifier, ManufacturerName, DateCode, SwBuildId | 仅 EP1 包含完整属性 |
| genIdentify | 0x0003 | In | IdentifyTime | |
| genOnOff | 0x0006 | In/Out | onOff, startUpOnOff | 每路独立 (v0.2.2 起 startUpOnOff 4路独立) |
| genGroups | 0x0004 | In/Out | — | 支持 Binding/群组控制 |

> **genBasic 优化**: 仅 Endpoint 1 包含完整的 genBasic 属性（ModelId、ManufacturerName 等），Endpoint 2~4 的 genBasic 仅包含 ClusterRevision 等最小属性，节省 Flash 空间。

## 设备标识

| 属性 | 值 | 说明 |
|------|-----|------|
| ModelIdentifier | `LXN-4S27LX1.0` | 借壳 HGZB-4S (LXN-4S27LX1.0)，Z2M 内置匹配 (长度前缀=14) |
| ManufacturerName | `TexasInstruments` | 保持默认（zigbeeModel 匹配不检查此字段） |
| DeviceID | `ZCL_HA_DEVICEID_ON_OFF_SWITCH` (0x0013) | Zigbee HA 标准开关 |
| SwBuildId | `v0.2.2` | 固件版本号 |
| DateCode | `20260725` | 编译日期 |

ZCL 字符串格式：`[长度字节][字符数据]`，长度前缀必须等于实际字符数，不能用空格填充。

---

## startUpOnOff 属性 (断电记忆, v0.2.0+)

### 属性定义

`genOnOff` Cluster 的 `startUpOnOff` 属性 (ATTRID = 0x4003, ENUM8) 控制 4 路开关断电恢复行为：

| 值 | 名称 | 行为 |
|----|------|------|
| 0x00 | OFF | 上电后该路为 OFF |
| 0x01 | ON | 上电后该路为 ON |
| 0x02 | TOGGLE | 上电后翻转断电前状态 |
| 0xFF | PREVIOUS | 恢复断电前状态 (默认) |

### 4 路独立配置 (v0.2.2 修复 BUG-009)

| 版本 | 实现方式 | 问题 |
|------|----------|------|
| v0.2.0/v0.2.1 | `uint8 zclSampleSw_StartUpOnOff` 单变量, 4 EP 共用 | Z2M 写入 4 路独立配置时互相覆盖, 最后写入的值生效, 4 路全部按同一配置恢复 |
| **v0.2.2+** | `uint8 zclSampleSw_StartUpOnOff[4]` 数组, 4 EP 独立 | 4 路可分别配置 off/on/toggle/previous, 互不影响 |

**4 个 EP 属性表分别指向独立数组元素**:
```c
zclSampleSw_RelayAttrs_ep1[] = { ..., &zclSampleSw_StartUpOnOff[0] };
zclSampleSw_RelayAttrs_ep2[] = { ..., &zclSampleSw_StartUpOnOff[1] };
zclSampleSw_RelayAttrs_ep3[] = { ..., &zclSampleSw_StartUpOnOff[2] };
zclSampleSw_RelayAttrs_ep4[] = { ..., &zclSampleSw_StartUpOnOff[3] };
```

### NV 持久化

| NV ID | 大小 | 内容 | 说明 |
|-------|------|------|------|
| 0x0F10 | 4 字节 | 4 路继电器状态 | 断电前状态, 用于 PREVIOUS/TOGGLE 恢复 |
| 0x0F12 | 4 字节 | 4 路独立 startUpOnOff 配置 | v0.2.2 起 (旧 ID 0x0F11 已废弃) |

详见 [功能设计.md](功能设计.md) 断电记忆章节。

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

---

## 状态同步机制 (v0.2.1+ 修复 BUG-007/BUG-008)

### 问题背景

| BUG | 现象 | 根因 |
|-----|------|------|
| BUG-007 | 断电恢复后设备状态变化, Z2M 状态不同步 | 上电恢复后无主动上报, Z2M 仅靠本地操作触发上报 |
| BUG-008 | 本地操作时信号不好, Z2M 未收到上报, 状态长期不同步 | 单次上报无重试/补偿机制 |

### 实现方案

**双机制保证 Z2M 状态一致性**：

| 机制 | 触发时机 | 实现 |
|------|----------|------|
| 入网立即上报 | ZDO 状态变为 `DEV_ROUTER` (入网成功) | 调度 `SAMPLESW_STATE_REPORT_EVT` 事件, 上报 4 路 OnOff 状态 |
| 周期性上报 | 每 30 秒 | `STATE_REPORT_INTERVAL_MS = 30000ms` 定时器, 周期触发 `SAMPLESW_STATE_REPORT_EVT` |

**事件处理**:
```c
#define SAMPLESW_STATE_REPORT_EVT  0x2000      // 状态上报事件
#define STATE_REPORT_INTERVAL_MS   30000       // 周期 30 秒

// ZDO 状态变化回调: 入网时立即上报 + 启动周期定时器
if (state == DEV_ROUTER) {
    osal_set_event(zclSampleSw_TaskID, SAMPLESW_STATE_REPORT_EVT);
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_STATE_REPORT_EVT, STATE_REPORT_INTERVAL_MS);
}

// 事件处理: 上报 4 路状态 + 重启定时器
if (events & SAMPLESW_STATE_REPORT_EVT) {
    zclSampleSw_ReportAllOnOffState();  // 上报 4 路 OnOff
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_STATE_REPORT_EVT, STATE_REPORT_INTERVAL_MS);
}
```

### 上报函数

```c
static void zclSampleSw_ReportAllOnOffState(void)
{
    uint8 i;
    for (i = 0; i < SAMPLESW_NUM_RELAYS; i++) {
        zclSampleSw_ReportOnOffState(i);  // 依次上报 4 路
    }
}
```

### 效果

- **BUG-007 修复**: 断电恢复后, 设备入网瞬间 Z2M 立即收到 4 路状态, 无需任何操作
- **BUG-008 修复**: 即使某次本地操作时信号不好导致 Z2M 未收到上报, 信号恢复后最多 30 秒内 Z2M 自动同步到正确状态
