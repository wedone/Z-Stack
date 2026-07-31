# BDB 入网

## 1. 概述

86四路智能开关基于 Z-Stack 3.0 的 BDB（Base Device Behavior）规范实现入网流程。v1.0.10 修复了一个关键 BUG：原代码无条件传入 `0x00` 作为 commissioning mode，导致工厂新设备（S1 复位后）无法入网。

正确做法是：读取 NV 中的 `bdbNodeIsOnANetwork` 状态，区分已配网/新设备选择 commissioning 模式。

## 2. commissioning 模式选择（v1.0.10 修复）

### 2.1 错误分析

原代码无条件传入 `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP`（实际值 `=0x00`）：

```c
// 错误代码（已废弃）
bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP);
```

**问题根因**：
- `0x00` 不设置任何 commissioning mode 位
- BDB 检查 `commissioningMode == 0` 后直接 report 失败
- 不会调用 `ZDO_InitDevice`，设备无法入网
- **影响场景**：工厂新设备 / S1 软复位后（`bdbNodeIsOnANetwork=FALSE`）

### 2.2 修复方案

读取 NV `ZCD_NV_BDBNODEISONANETWORK` 状态，区分两种场景：

| `bdbNodeIsOnANetwork` | 场景 | commissioning 模式 |
|------|------|------|
| `TRUE` | 已配网设备（上电恢复） | `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP` |
| `FALSE` | 新设备 / S1 复位后 | `BDB_COMMISSIONING_MODE_NWK_STEERING` |

修复后代码（zcl_samplesw.c `zclSampleSw_Init()`）：

```c
// v1.0.10修复: S1软复位后无法入网的根因
// 原代码无条件传入 BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP(=0x00),
// 该参数不设置任何commissioning mode位, 对工厂新设备(S1复位后bdbNodeIsOnANetwork=FALSE)
// BDB检查commissioningMode==0后直接report失败, 不会调用ZDO_InitDevice, 设备无法入网。
// 修复: 读取NV中的bdbNodeIsOnANetwork状态, 区分已配网/新设备选择commissioning模式
{
  uint8 isOnNetwork = FALSE;
  osal_nv_read(ZCD_NV_BDBNODEISONANETWORK, 0, sizeof(uint8), &isOnNetwork);
  if (isOnNetwork == TRUE)
  {
    // 已配网设备: 尝试rejoin恢复网络
    bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP);
  }
  else
  {
    // 工厂新设备(S1复位后/首次配网): 触发NWK_STEERING发现网络并加入
    bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
  }
}
```

## 3. TX 功率设置（必须在 bdb_StartCommissioning 之前）

### 3.1 调用顺序

TX 功率必须在 `bdb_StartCommissioning()` 之前调用，确保入网时即使用设置后的发射功率：

```c
// v1.0.8: 保持发射功率 4 dBm (TX_PWR_PLUS_4), 与ZNP官方默认一致
// 历史:
//   v1.0.5: 0 dBm -> 4 dBm (信号不稳定, 10+~105波动)
//   v1.0.6: 4 dBm (信号仍不稳定, 排查为PWM调光干扰)
//   v1.0.7: 4 dBm + 未入网时禁用PWM (仍不稳定)
//   v1.0.8: 4 dBm + 完全移除PWM调光 (PWM是信号不稳定的根因)
// 测试结论: 7 dBm导致RF不稳定(信号归零), 5 dBm无改善, 4 dBm + 移除PWM为最佳配置
// 必须在 bdb_StartCommissioning() 之前调用, 确保入网时即使用设置后的发射功率
ZMacSetTransmitPower(TX_PWR_PLUS_4);

// v1.0.10修复: ... commissioning mode 选择代码
{
  uint8 isOnNetwork = FALSE;
  osal_nv_read(ZCD_NV_BDBNODEISONANETWORK, 0, sizeof(uint8), &isOnNetwork);
  if (isOnNetwork == TRUE)
  {
    bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP);
  }
  else
  {
    bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
  }
}
```

### 3.2 功率档位选择

| 功率档位 | 宏 | 测试结果 |
|---------|-----|---------|
| 0 dBm | `TX_PWR_0` | 默认值，信号偏低，不稳定 |
| 4 dBm | `TX_PWR_PLUS_4` | **推荐**，与 TI ZNP 官方默认一致 |
| 7 dBm | `TX_PWR_PLUS_7` | RF 不稳定，信号归零 |

详细分析见《TX功率.md》。

## 4. zclSampleSw_Init 中的 BDB 流程

完整的 `zclSampleSw_Init()` 末尾 BDB commissioning 与 TX 功率部分：

```c
void zclSampleSw_Init( byte task_id )
{
  // ... 端点注册、属性注册、NV 初始化、GPIO 初始化等 ...

  // v1.0.8: 保持发射功率 4 dBm (TX_PWR_PLUS_4), 与ZNP官方默认一致
  // 必须在 bdb_StartCommissioning() 之前调用, 确保入网时即使用设置后的发射功率
  ZMacSetTransmitPower(TX_PWR_PLUS_4);

  // v1.0.10修复: S1软复位后无法入网的根因
  // 读取NV中的bdbNodeIsOnANetwork状态, 区分已配网/新设备选择commissioning模式
  {
    uint8 isOnNetwork = FALSE;
    osal_nv_read(ZCD_NV_BDBNODEISONANETWORK, 0, sizeof(uint8), &isOnNetwork);
    if (isOnNetwork == TRUE)
    {
      // 已配网设备: 尝试rejoin恢复网络
      bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP);
    }
    else
    {
      // 工厂新设备(S1复位后/首次配网): 触发NWK_STEERING发现网络并加入
      bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
    }
  }

  // v1.0.4新增: 启动配网中LED1慢闪, 提示用户设备正在配网
  // 已配网设备: BDB rejoin成功后ZDO_STATE_CHANGE会触发StopPairingBlink
  // 新设备: 持续慢闪直到入网成功或5分钟超时
  zclSampleSw_StartPairingBlink();
}
```

## 5. Commissioning 状态回调

注册 commissioning 状态回调，处理不同阶段的状态：

```c
// 注册回调（zclSampleSw_Init 中）
bdb_RegisterCommissioningStatusCB( zclSampleSw_ProcessCommissioningStatus );
```

```c
static void zclSampleSw_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //After formation, perform nwk steering again plus the remaining commissioning modes that has not been processed yet
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      else
      {
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
        //We are on the nwk, what now?
      }
      else
      {
        //See the possible errors for nwk steering procedure
        //No suitable networks found
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_FINDING_BINDING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //YOUR JOB:
        //retry?, wait for user interaction?
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //已成功恢复网络
      }
      else
      {
        //设备未入网，自动启动网络引导（加入现有网络）+ 发现绑定
        //注意：不请求 NWK_FORMATION，避免路由器创建自己的分布式网络
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING |
                               BDB_COMMISSIONING_MODE_FINDING_BINDING);
      }
    break;
#if ZG_BUILD_ENDDEVICE_TYPE    
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        //We did recover from losing parent
      }
      else
      {
        //Parent not found, attempt to rejoin again after a fixed delay
        osal_start_timerEx(zclSampleSw_TaskID, SAMPLEAPP_END_DEVICE_REJOIN_EVT, SAMPLEAPP_END_DEVICE_REJOIN_DELAY);
      }
    break;
#endif 
  }
}
```

### 5.1 关键点

- **`BDB_COMMISSIONING_INITIALIZATION` 失败**：自动启动 NWK_STEERING + FINDING_BINDING，**不请求 NWK_FORMATION**，避免路由器创建自己的分布式网络
- **`BDB_COMMISSIONING_PARENT_LOST`**（仅 End Device）：延迟 10 秒后重试 rejoin

## 6. ZDO_STATE_CHANGE 处理

入网成功（`DEV_ROUTER`）时执行两个动作：
1. **停止配网慢闪**：`zclSampleSw_StopPairingBlink()`
2. **上报所有 4 路 OnOff 状态**：`zclSampleSw_ReportAllOnOffState()`（状态同步）

```c
case ZDO_STATE_CHANGE:
  // v1.0.0重构: 移除 UI_DeviceStateUpdated (UI模块已剥离)
  // 86开关: 协议栈Router启动时会操作LED3/LED4(ZDApp.c), 覆盖继电器状态灯。
  // 入网状态变化后重新刷新所有继电器/LED输出, 恢复正确显示。
  zclSampleSw_UpdateAllRelayOutputs();
  // 状态同步: 入网成功(DEV_ROUTER)后立即上报当前OnOff状态, 修复断电恢复后Z2M状态不同步(BUG-007)
  // BUG-010修复: 移除30秒周期性上报定时器, 避免无操作时触发z2m state_action生成无意义action事件
  // 状态同步改为依赖: 1)入网后立即上报 2)触摸/远程操作后立即上报 3)z2m availability检测
  if ((devStates_t)(MSGpkt->hdr.status) == DEV_ROUTER && zclSampleSw_NwkState != DEV_ROUTER)
  {
    // v1.0.4新增: 入网成功, 停止配网中LED1慢闪
    zclSampleSw_StopPairingBlink();
    // v1.0.8变更: 移除PWM调光启用 (PWM定时器影响RF信号稳定性)
    zclSampleSw_ReportAllOnOffState();
  }
  zclSampleSw_NwkState = (devStates_t)(MSGpkt->hdr.status);
  break;
```

### 6.1 关键点

- **`UpdateAllRelayOutputs`**：每次状态变化都刷新，因为协议栈 Router 启动时会操作 LED3/LED4，需要恢复正确显示
- **仅当 `DEV_ROUTER` 且 `NwkState != DEV_ROUTER`**：避免重复触发（仅在 DEV_INIT → DEV_ROUTER 跃变时执行）
- **状态同步**：入网后立即上报所有 4 路 OnOff 状态，修复断电恢复后 Z2M 状态不同步（BUG-007）

## 7. 配网 LED 慢闪

入网流程启动时同步启动 LED1 慢闪（1Hz），提示用户正在配网：

```c
// v1.0.4新增: 启动配网中LED1慢闪, 提示用户设备正在配网
// 已配网设备: BDB rejoin成功后ZDO_STATE_CHANGE会触发StopPairingBlink
// 新设备: 持续慢闪直到入网成功或5分钟超时
zclSampleSw_StartPairingBlink();
```

详细 LED 控制逻辑见《LED控制.md》。

## 8. 端点注册（与 BDB 配套）

在 `zclSampleSw_Init` 中注册端点 SimpleDescriptor，BDB commissioning 完成后 z2m 会通过这些端点 interview 设备：

```c
// Register the Simple Descriptor for this application
bdb_RegisterSimpleDescriptor( &zclSampleSw_SimpleDesc );

// 注册4路继电器端点 (EP 1-4, 对应 alab.switch l1-l4)
// 每个端点注册独立的callbacks, 使OnOff命令能定位到正确的继电器索引
{
  uint8 ep;
  static zclGeneral_AppCallbacks_t* relayCBs[SAMPLESW_NUM_RELAYS] = {
    &zclSampleSw_CmdCallbacks_ep1,
    &zclSampleSw_CmdCallbacks_ep2,
    &zclSampleSw_CmdCallbacks_ep3,
    &zclSampleSw_CmdCallbacks_ep4,
  };
  for (ep = 0; ep < SAMPLESW_NUM_RELAYS; ep++)
  {
    bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[ep]);
    zclGeneral_RegisterCmdCallbacks(zclSampleSw_RelaySimpleDesc[ep].EndPoint, relayCBs[ep]);
  }
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY1, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep1);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY2, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep2);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY3, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep3);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY4, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep4);
}

// 注册4路输入状态端点 (EP 5-8, 对应 alab.switch in1-in4)
{
  uint8 ep;
  for (ep = 0; ep < SAMPLESW_NUM_INPUTS; ep++)
  {
    bdb_RegisterSimpleDescriptor(&zclSampleSw_InputSimpleDesc[ep]);
  }
  zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT1, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep5);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT2, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep6);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT3, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep7);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT4, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep8);
}
```

## 9. 经验教训

### 9.1 commissioning mode 不能传 0x00

- `0x00` 不设置任何 mode 位
- BDB 检查到 `commissioningMode == 0` 直接 report 失败
- 影响新设备入网（已配网设备 rejoin 偶然能成功，但不可靠）
- **修复**：根据 `bdbNodeIsOnANetwork` 状态选择 REJOIN 或 NWK_STEERING

### 9.2 TX 功率必须在 commissioning 之前设置

- 否则入网过程使用默认 0 dBm，信号偏低
- 可能导致入网失败或入网后 LQI 不稳定
- **正确顺序**：`ZMacSetTransmitPower` → `bdb_StartCommissioning`

### 9.3 不请求 NWK_FORMATION

- 路由器请求 NWK_FORMATION 会创建自己的分布式网络
- 应仅请求 NWK_STEERING 加入现有协调器网络

## 10. 相关文件

- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.c`
  - `zclSampleSw_Init()`（BDB 启动入口）
  - `zclSampleSw_ProcessCommissioningStatus()`（commissioning 状态回调）
  - `zclSampleSw_event_loop()` 中 `ZDO_STATE_CHANGE` 处理
- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.h`
  - 端点 ID 定义
- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw_data.c`
  - `zclSampleSw_SimpleDesc` / `zclSampleSw_RelaySimpleDesc[]` / `zclSampleSw_InputSimpleDesc[]`
