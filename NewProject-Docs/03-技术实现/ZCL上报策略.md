# ZCL 上报策略

## 1. 概述

86四路智能开关通过 ZCL Report Attributes 机制向协调器（z2m）上报继电器状态。上报策略的核心是**只在必要时上报**，避免 CC2530 仅 3072 字节堆内存被耗尽，同时避免 z2m 生成无意义的 action 事件。

## 2. 什么时候应该上报

### 2.1 触摸操作后：必须上报

z2m 不知道本地触摸事件，必须主动 Report 才能让 z2m 状态同步：

```c
static void zclSampleSw_ToggleRelay(uint8 idx)
{
  zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx];
  zclSampleSw_UpdateRelayOutput(idx);
  zclSampleSw_ReportOnOffState(idx);  // ← 必须上报
  zclSampleSw_NvScheduleSave();
}
```

### 2.2 入网成功后：立即上报所有 4 路状态

入网后立即上报，作为状态同步的起点（修复 BUG-007：断电恢复后 Z2M 状态不同步）：

```c
case ZDO_STATE_CHANGE:
  zclSampleSw_UpdateAllRelayOutputs();
  if ((devStates_t)(MSGpkt->hdr.status) == DEV_ROUTER && zclSampleSw_NwkState != DEV_ROUTER)
  {
    zclSampleSw_StopPairingBlink();
    zclSampleSw_ReportAllOnOffState();  // ← 立即上报所有 4 路
  }
  zclSampleSw_NwkState = (devStates_t)(MSGpkt->hdr.status);
  break;
```

## 3. 什么时候不应该上报

### 3.1 ZCL 命令处理后：不需要主动 Report

z2m 发送 ZCL On/Off/Toggle 命令后，ZCL 层会自动发送 **Default Response** 给 z2m，z2m 据此更新状态。主动 Report 是冗余的，且会耗尽堆内存。

v1.0.9 修复：移除 `HandleOnOffCmd` 中的 `ReportOnOffState`：

```c
/*********************************************************************
 * @fn      zclSampleSw_HandleOnOffCmd
 * @brief   处理ZCL On/Off/Toggle命令, 更新继电器状态和GPIO
 *          v1.0.9: 移除主动ReportOnOffState, z2m通过Default Response已确认状态
 *          原因: CC2530堆仅3072字节, 每次ZCL命令处理产生2条AF消息
 *                (Report + Default Response), 连续操作会耗尽堆导致看门狗复位
 * @param   idx - 继电器索引 0~3
 * @param   cmd - ZCL命令ID (COMMAND_ON / COMMAND_OFF / COMMAND_TOGGLE)
 * @return  none
 */
static void zclSampleSw_HandleOnOffCmd(uint8 idx, uint8 cmd)
{
  switch (cmd)
  {
    case COMMAND_ON:
      zclSampleSw_RelayState[idx] = TRUE;
      break;
    case COMMAND_OFF:
      zclSampleSw_RelayState[idx] = FALSE;
      break;
    case COMMAND_TOGGLE:
      zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx];
      break;
    default:
      return;
  }
  zclSampleSw_UpdateRelayOutput(idx);
  // v1.0.9: 移除 zclSampleSw_ReportOnOffState(idx)
  // ZCL层会自动发送Default Response, z2m据此确认状态, 主动Report是冗余的
  // 触摸操作(ToggleRelay)仍保留主动上报, 因为z2m不知道本地触摸事件
  // 断电记忆: 调度延迟写入 (5秒后合并写入NV)
  zclSampleSw_NvScheduleSave();
}
```

### 3.2 不要周期性上报

v1.0.10 修复：移除 30 秒周期性上报定时器 `SAMPLESW_STATE_REPORT_EVT`。

源码注释：

```c
// BUG-010修复: 移除30秒周期性上报(SAMPLESW_STATE_REPORT_EVT)
// 原因: 周期性上报会触发z2m的state_action选项, 生成无意义的action事件(每30秒4个action)
// 现方案: 仅在状态变化时(触摸/远程操作后)和入网后立即上报, 避免无操作时的action事件
// 状态同步保障: z2m availability检测 + 下次操作时的立即上报
```

```c
// BUG-010修复: 移除SAMPLESW_STATE_REPORT_EVT周期性上报事件处理
// 原因: 30秒周期性上报会触发z2m state_action, 生成无意义action事件
// 状态同步改为依赖: 入网后立即上报 + 操作后立即上报 + z2m availability检测
```

**周期性上报的问题**：
- 触发 z2m 的 `state_action` 选项，每 30 秒生成 4 个无意义 action 事件
- 用户日志被刷屏
- 浪费 CC2530 堆内存

## 4. CC2530 堆限制分析

### 4.1 堆大小

CC2530 SoC 可用堆仅 **3072 字节**，非常紧张。

### 4.2 单次 ZCL On/Off 命令处理的消息开销

每次 ZCL On/Off 命令处理会产生 2 条 AF 消息：
1. **Report Attributes**（若主动上报，约 50 字节）
2. **Default Response**（ZCL 层自动发送，约 30 字节）

### 4.3 连续操作场景估算

z2m 批量操作（如全开/全关）会连续发送 4 路 On/Off 命令，每路间隔可能短至几十毫秒。

假设每条 AF 消息 50 字节，3 轮 × 4 端点 = 12 次操作：

- 主动 Report 开启：每次产生 2 条消息（Report + Default Response）
  - 总消息数：3 × 4 × 2 = **24 条**
  - 堆占用估算：24 × 50 ≈ **1200 字节**
  - 接近 3072 字节上限，连续操作可能耗尽堆
- 主动 Report 关闭：每次仅 1 条消息（Default Response）
  - 总消息数：3 × 4 × 1 = **12 条**
  - 堆占用估算：12 × 50 ≈ **600 字节**
  - 安全余量充足

**结论**：ZCL 命令处理后必须移除主动 Report，仅保留 Default Response。

## 5. 上报函数实现

### 5.1 单路上报（ReportOnOffState）

```c
/*********************************************************************
 * @fn      zclSampleSw_ReportOnOffState
 * @brief   向协调器上报指定通道继电器的OnOff属性状态(ZCL Report Attributes)
 *          仅触摸翻转后调用(z2m不知道本地操作); ZCL命令处理后不再调用
 *          (z2m通过Default Response已确认状态, 主动Report是冗余的)
 * @param   idx - 继电器索引 0~3
 * @return  none
 */
static void zclSampleSw_ReportOnOffState(uint8 idx)
{
  uint8 ep;
  zclReportCmd_t *reportCmd;
  zclReport_t *reportRec;

  switch (idx)
  {
    case 0:  ep = SAMPLESW_ENDPOINT_RELAY1; break;
    case 1:  ep = SAMPLESW_ENDPOINT_RELAY2; break;
    case 2:  ep = SAMPLESW_ENDPOINT_RELAY3; break;
    case 3:  ep = SAMPLESW_ENDPOINT_RELAY4; break;
    default: return;
  }

  reportCmd = (zclReportCmd_t *)osal_msg_allocate(sizeof(zclReportCmd_t) + sizeof(zclReport_t));
  if (reportCmd == NULL) return;

  reportCmd->numAttr = 1;
  reportRec = &(reportCmd->attrList[0]);
  reportRec->attrID = ATTRID_ON_OFF;
  reportRec->dataType = ZCL_DATATYPE_BOOLEAN;
  reportRec->attrData = &zclSampleSw_RelayState[idx];

  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  zclSampleSw_DstAddr.addr.shortAddr = 0;  // 协调器
  zclSampleSw_DstAddr.endPoint = 1;

  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ON_OFF,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);

  osal_msg_deallocate((uint8 *)reportCmd);
}
```

### 5.2 4 路全上报（ReportAllOnOffState）

入网后立即上报所有 4 路，作为状态同步起点：

```c
/*********************************************************************
 * @fn      zclSampleSw_ReportAllOnOffState
 * @brief   向协调器上报所有4路继电器的OnOff状态
 *          用于入网后立即同步状态(BUG-007)和周期性状态同步(BUG-008),
 *          确保Z2M状态与设备实际状态一致, 即使某次Report丢失也能在下次周期恢复。
 * @return  none
 */
static void zclSampleSw_ReportAllOnOffState(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_ReportOnOffState(i);
  }
}
```

### 5.3 输入状态上报（ReportInputState）

触摸状态变化时上报 genAnalogInput.presentValue（1.0f=触摸中，0.0f=未触摸），同步 z2m 的 `input_state_inX`：

```c
static void zclSampleSw_ReportInputState(uint8 idx)
{
  uint8 ep;
  zclReportCmd_t *reportCmd;
  zclReport_t *reportRec;

  switch (idx)
  {
    case 0:  ep = SAMPLESW_ENDPOINT_INPUT1; break;
    case 1:  ep = SAMPLESW_ENDPOINT_INPUT2; break;
    case 2:  ep = SAMPLESW_ENDPOINT_INPUT3; break;
    case 3:  ep = SAMPLESW_ENDPOINT_INPUT4; break;
    default: return;
  }

  reportCmd = (zclReportCmd_t *)osal_msg_allocate(sizeof(zclReportCmd_t) + sizeof(zclReport_t));
  if (reportCmd == NULL) return;

  reportCmd->numAttr = 1;
  reportRec = &(reportCmd->attrList[0]);
  reportRec->attrID = ATTRID_IOV_BASIC_PRESENT_VALUE;
  reportRec->dataType = ZCL_DATATYPE_SINGLE_PREC;
  reportRec->attrData = (uint8 *)&zclSampleSw_InputState[idx];

  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  zclSampleSw_DstAddr.addr.shortAddr = 0;  // 协调器
  zclSampleSw_DstAddr.endPoint = 1;

  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);

  osal_msg_deallocate((uint8 *)reportCmd);
}
```

## 6. 触摸操作上报流程

`ProcessTouchPoll` 中防抖确认上升沿后，依次执行：
1. `ToggleRelay`：翻转继电器并 Report OnOff
2. `ReportInputState`：上报 input_state（1.0f=触摸中）

```c
// 连续确认达到阈值, 更新稳定状态
if (touchDebounce[i] >= TOUCH_DEBOUNCE_COUNTS)
{
  if (curBit)
  {
    touchStableState |= BV(i);
    // 上升沿确认(未触摸→触摸), 触发继电器翻转
    zclSampleSw_ToggleRelay(i);  // ← 内部调用 ReportOnOffState
  }
  else
  {
    touchStableState &= ~BV(i);
  }
  // 更新input_state并上报 (1.0f=触摸中, 0.0f=未触摸)
  zclSampleSw_InputState[i] = curBit ? 1.0f : 0.0f;
  zclSampleSw_ReportInputState(i);  // ← 上报触摸状态
  touchDebounce[i] = 0;
}
```

## 7. 上报策略总结

| 场景 | 是否主动 Report | 原因 |
|------|----------------|------|
| 触摸操作后 | ✅ 是 | z2m 不知道本地触摸事件 |
| 入网成功后 | ✅ 是（4 路全报） | 状态同步起点 |
| ZCL 命令处理后 | ❌ 否 | ZCL 层自动发送 Default Response，z2m 据此更新 |
| 周期性上报 | ❌ 否 | 触发 z2m 无意义 action 事件，浪费堆 |

## 8. 状态同步保障

移除周期性上报后，状态同步依赖三个机制：

```c
// 状态同步保障: z2m availability检测 + 下次操作时的立即上报
```

1. **入网后立即上报**：`ZDO_STATE_CHANGE` 触发 `ReportAllOnOffState`
2. **操作后立即上报**：触摸操作触发 `ReportOnOffState` + `ReportInputState`
3. **z2m availability 检测**：z2m 定期检查设备 availability，发现掉线后主动 re-interview

## 9. 经验教训

### 9.1 CC2530 堆内存是稀缺资源

- 3072 字节堆限制下，每条 AF 消息约 50 字节都很珍贵
- 连续操作场景（z2m 批量控制）必须考虑堆累积
- **正确做法**：能不 Report 就不 Report，依赖 ZCL 层的 Default Response

### 9.2 周期性上报触发 z2m state_action

- z2m 默认开启 `state_action` 选项，会为每个 Report 生成 action 事件
- 30 秒 × 4 路 = 每 30 秒 4 个无意义 action，污染日志
- **正确做法**：仅在状态变化时上报

### 9.3 区分本地操作与远程命令

- 本地操作（触摸）：z2m 不知道，必须主动 Report
- 远程命令（ZCL）：z2m 知道，依赖 Default Response 即可
- **正确做法**：在 `ToggleRelay` 中 Report，在 `HandleOnOffCmd` 中不 Report

## 10. 相关文件

- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.c`
  - `zclSampleSw_ReportOnOffState()`（单路上报）
  - `zclSampleSw_ReportAllOnOffState()`（4 路全上报）
  - `zclSampleSw_ReportInputState()`（输入状态上报）
  - `zclSampleSw_HandleOnOffCmd()`（ZCL 命令处理，已移除 Report）
  - `zclSampleSw_ToggleRelay()`（触摸翻转，含 Report）
  - `zclSampleSw_ProcessTouchPoll()`（触摸轮询，含 ReportInputState）
  - `zclSampleSw_event_loop()` 中 `ZDO_STATE_CHANGE` 处理（入网后 ReportAll）
