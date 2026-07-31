# dev 分支设备内交互参考代码

> 本文档记录 dev 分支最终版本中**设备内交互**的完整实现，供 linxee 分支开发时直接参考。
>
> **核心原则**：设备内交互（GPIO/LED/触摸/继电器/S1 复位）不涉及网络通信，**不受路由问题影响**，可直接移植。涉及向外发送信息的功能（ZCL Report/入网/BDB commissioning）才可能受 PTVO 路由问题影响，需谨慎评估。
>
> **使用方式**：linxee 分支实现设备内交互功能时，**必须先读本文档**，直接参考 dev 最终代码，不要凭描述重新实现。
>
> **代码来源**：`dev` 分支 `Projects/zstack/HomeAutomation/HGZBSwitch/Source/zcl_samplesw.c` / `.h`

---

## 硬件引脚映射

```c
/* ============================================================
 * 86四路智能开关硬件引脚映射
 *   继电器1~4 (低电平触发吸合): P1_0, P1_2, P1_6, P2_0
 *   LED1~4    (反逻辑, ACTIVE_LOW: 写0=亮, 写1=灭): P0_0~P0_3
 *   触摸1~4   (WTC6106BSI输出, 低电平=触摸中): P0_4~P0_7
 *   S1配网按键 (低电平有效): P1_3
 * LED与继电器联动: 继电器OFF→LED亮(写0), 继电器ON→LED灭(写1)
 * ============================================================ */

// 触摸轮询参数
#define TOUCH_POLL_INTERVAL_MS    50        // 触摸轮询周期
#define TOUCH_DEBOUNCE_COUNTS     2        // 连续2次(100ms)确认状态变化, 防抖

// S1配网按键长按复位 (P1_3, 低电平有效)
#define S1_RESET_THRESHOLD        100      // 5秒 = 100 * 50ms轮询周期

// 继电器引脚位掩码 (P1口: P1_0/P1_2/P1_6, P2口: P2_0)
#define RELAY_P1_BV               (BV(0) | BV(2) | BV(6))
#define RELAY_P2_BV               (BV(0))
// 触摸输入引脚位掩码 (P0_4~P0_7)
#define TOUCH_INPUT_BV            (BV(4) | BV(5) | BV(6) | BV(7))

// S1复位LED闪烁参数
#define RESET_BLINK_TOTAL_COUNT   6    // 3次闪烁 = 6次状态切换
#define RESET_BLINK_PERIOD_MS     300  // 每次亮或灭的持续时间

// 配网LED慢闪参数
#define PAIRING_BLINK_PERIOD_MS   500  // 每次亮或灭的持续时间 (1Hz)
// 配网超时: 5分钟
#define SAMPLESW_PAIRING_TIMEOUT_MS         (5 * 60 * 1000UL)
#define PAIRING_BLINK_TIMEOUT_TICKS   (SAMPLESW_PAIRING_TIMEOUT_MS / PAIRING_BLINK_PERIOD_MS)
```

### 全局变量

```c
// 触摸防抖状态
static uint8 touchStableState = 0;
static uint8 touchDebounce[4] = {0, 0, 0, 0};
static uint8 touchPending[4]  = {0, 0, 0, 0};
static uint8 s1HoldCount = 0;              // S1长按复位计数器

// S1复位LED闪烁状态机
static uint8 resetBlinkCount = 0;

// 配网LED1慢闪状态机
static uint8 pairingBlinkActive = FALSE;
static uint8 pairingBlinkLedOn = FALSE;
static uint16 pairingBlinkTickCount = 0;

// 断电记忆: startUpOnOff缓存值
static uint8 startupOnOffCached[SAMPLESW_NUM_RELAYS] = {
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS,
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS
};
```

### 事件定义（zcl_samplesw.h）

```c
#define SAMPLESW_TOUCH_POLL_EVT          0x0010  // 触摸轮询
#define SAMPLESW_PAIRING_BLINK_EVT       0x0020  // 配网LED慢闪
#define SAMPLESW_RESET_BLINK_EVT         0x0040  // S1复位LED闪烁
```

---

## A. GPIO 初始化

**硬件交互**：继电器设为输出默认高电平（断开），触摸设为输入（上拉）。

> **dev 的已知问题**：dev 的 `InitGpio()` 没有显式配置 P0_0~P0_3 为 GPIO 输出（注释说依赖 HalLedInit，但 HalLedInit 只配 P1 口）。linxee v0.2.0 已修复此问题（方案B：hal_board_cfg_linxee.h 的 `HAL_BOARD_INIT` 中 `LEDx_SET_DIR()` 配置 GPIO 方向）。linxee 实现时应保留此修复。

```c
void zclSampleSw_InitGpio(void)
{
  // 继电器引脚设为GPIO功能并配置为输出
  P1SEL &= ~RELAY_P1_BV;        // P1_0/P1_2/P1_6 选为GPIO
  P2SEL &= ~RELAY_P2_BV;        // P2_0 选为GPIO
  P1DIR |= RELAY_P1_BV;         // 设为输出
  P2DIR |= RELAY_P2_BV;         // 设为输出

  // 触摸输入引脚设为GPIO功能并配置为输入(默认上拉)
  P0SEL &= ~TOUCH_INPUT_BV;     // P0_4~P0_7 选为GPIO
  P0DIR &= ~TOUCH_INPUT_BV;     // 设为输入

  touchStableState = 0;

  // linxee 修复: 显式配置 LED 引脚 P0_0~P0_3 为 GPIO 输出
  // (dev 依赖 HalLedInit 只配 P1 口, 导致 LED1/3/4 不亮)
  // P0SEL &= ~0x0F;            // P0_0~P0_3 选为 GPIO
  // P0DIR |= 0x0F;             // 设为输出
}
```

---

## B. LED 底层控制

### B.1 LED GPIO 写入

**硬件交互**：直接写 P0_0~P0_3，反逻辑（on=TRUE 写 0 亮，on=FALSE 写 1 灭）。

```c
static void zclSampleSw_LedWriteGpio(uint8 idx, uint8 on)
{
  uint8 val = on ? 0 : 1;   // 反逻辑: 亮=0, 灭=1
  switch (idx)
  {
    case 0: P0_0 = val; break;
    case 1: P0_1 = val; break;
    case 2: P0_2 = val; break;
    case 3: P0_3 = val; break;
    default: break;
  }
}
```

### B.2 LED 设置入口

**说明**：v1.0.8 移除 PWM 后直接调用 LedWriteGpio，100% 亮度。所有 LED 控制点统一经此函数。

```c
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on)
{
  zclSampleSw_LedWriteGpio(idx, on);
}
```

### B.3 继电器 + LED 联动

**硬件交互**：根据 `zclSampleSw_RelayState[idx]` 同步写继电器 GPIO（ON=低电平吸合）和 LED（反逻辑：OFF→亮，ON→灭）。LED1 受 `pairingBlinkActive` 屏蔽，配网慢闪期间不由继电器联动刷新。

```c
void zclSampleSw_UpdateRelayOutput(uint8 idx)
{
  uint8 on = zclSampleSw_RelayState[idx];

  switch (idx)
  {
    case 0:
      P1_0 = on ? 0 : 1;        // 继电器1: ON=低电平
      if (!pairingBlinkActive)   // 配网慢闪时不刷新LED1
      {
        zclSampleSw_LedSetTarget(0, !on);   // LED1: 反逻辑 OFF→亮, ON→灭
      }
      break;
    case 1:
      P1_2 = on ? 0 : 1;        // 继电器2
      zclSampleSw_LedSetTarget(1, !on);     // LED2
      break;
    case 2:
      P1_6 = on ? 0 : 1;        // 继电器3
      zclSampleSw_LedSetTarget(2, !on);     // LED3
      break;
    case 3:
      P2_0 = on ? 0 : 1;        // 继电器4
      zclSampleSw_LedSetTarget(3, !on);     // LED4
      break;
    default:
      break;
  }
}

void zclSampleSw_UpdateAllRelayOutputs(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_UpdateRelayOutput(i);
  }
}
```

---

## C. 触摸检测 + S1 长按复位

### C.1 读取触摸输入

**硬件交互**：读取 P0 整口，解析 P0_4~P0_7（低电平=触摸中）。

```c
static uint8 zclSampleSw_ReadTouchInputs(void)
{
  uint8 port = P0;
  uint8 val = 0;
  if (!(port & BV(4))) val |= BV(0);  // 通道1: P0_4=低=触摸
  if (!(port & BV(5))) val |= BV(1);  // 通道2: P0_5
  if (!(port & BV(6))) val |= BV(2);  // 通道3: P0_6
  if (!(port & BV(7))) val |= BV(3);  // 通道4: P0_7
  return val;
}
```

### C.2 触摸轮询主处理

**硬件交互**：
1. 4 路触摸防抖（连续 2 次=100ms 确认），仅上升沿触发翻转
2. S1 长按检测（P1_3 低电平，5 秒触发复位闪烁）
3. 防御性刷新 LED 状态（每 50ms）
4. 检测 Z2M 远程修改的 startUpOnOff 属性

```c
void zclSampleSw_ProcessTouchPoll(void)
{
  uint8 cur = zclSampleSw_ReadTouchInputs();
  uint8 i;

  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    uint8 curBit    = (cur >> i) & 1;
    uint8 stableBit = (touchStableState >> i) & 1;

    if (curBit != stableBit)
    {
      if (touchPending[i] != curBit)
      {
        touchPending[i] = curBit;
        touchDebounce[i] = 1;
      }
      else
      {
        touchDebounce[i]++;
      }

      if (touchDebounce[i] >= TOUCH_DEBOUNCE_COUNTS)
      {
        if (curBit)
        {
          touchStableState |= BV(i);
          zclSampleSw_ToggleRelay(i);  // 上升沿触发翻转
        }
        else
        {
          touchStableState &= ~BV(i);
        }
        zclSampleSw_InputState[i] = curBit ? 1.0f : 0.0f;
        zclSampleSw_ReportInputState(i);  // ⚠️ 对外通信, 可能受路由影响
        touchDebounce[i] = 0;
      }
    }
    else
    {
      touchDebounce[i] = 0;
      touchPending[i] = stableBit;
    }
  }

  // S1长按检测 (P1_3, 低电平有效)
  if (!P1_3)
  {
    s1HoldCount++;
    if (s1HoldCount >= S1_RESET_THRESHOLD)
    {
      s1HoldCount = 0;
      zclSampleSw_StartResetBlink();  // 启动复位LED闪烁
      osal_stop_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT);
      return;  // 直接返回, 不重启轮询(由闪烁状态机接管)
    }
  }
  else
  {
    s1HoldCount = 0;
  }

  // 检测Z2M远程修改的startUpOnOff属性
  if (osal_memcmp(zclSampleSw_StartUpOnOff, startupOnOffCached, SAMPLESW_NUM_RELAYS) == FALSE)
  {
    osal_memcpy(startupOnOffCached, zclSampleSw_StartUpOnOff, SAMPLESW_NUM_RELAYS);
    zclSampleSw_NvScheduleSave();
  }

  // 防御性刷新LED状态 (每50ms)
  zclSampleSw_UpdateAllRelayOutputs();

  // 重新启动下一次轮询
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT, TOUCH_POLL_INTERVAL_MS);
}
```

---

## D. S1 复位 LED 闪烁

**交互流程**：S1 长按 5 秒 → LED1 闪烁 3 次（300ms 亮/300ms 灭，共 1.8 秒）→ 执行 Basic Reset + BDB Reset

```c
static void zclSampleSw_StartResetBlink(void)
{
  resetBlinkCount = RESET_BLINK_TOTAL_COUNT;  // 6
  zclSampleSw_LedSetTarget(0, TRUE);          // LED1亮
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
}

static void zclSampleSw_ProcessResetBlink(void)
{
  resetBlinkCount--;

  if (resetBlinkCount > 0)
  {
    // 切换LED1: 奇数count=灭, 偶数count=亮
    // count=5→灭, 4→亮, 3→灭, 2→亮, 1→灭 (共3次闪烁)
    zclSampleSw_LedSetTarget(0, !(resetBlinkCount % 2));
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
  }
  else
  {
    // 闪烁完成, 执行复位流程
    zclSampleSw_BasicResetCB();           // 1. 重置ZCL属性
    bdb_resetLocalAction();               // 2. 网络离开/重启 ⚠️ 对外通信
    zclSampleSw_UpdateAllRelayOutputs();  // 3. 刷新继电器/LED到默认
  }
}

static void zclSampleSw_BasicResetCB(void)
{
  zclSampleSw_ResetAttributesToDefaultValues();
}
```

---

## E. 配网 LED 慢闪

**交互流程**：设备启动未入网 → LED1 1Hz 慢闪（500ms 亮/500ms 灭）→ 入网成功或 5 分钟超时停止

```c
static void zclSampleSw_StartPairingBlink(void)
{
  pairingBlinkActive = TRUE;
  pairingBlinkLedOn = FALSE;
  pairingBlinkTickCount = 0;
  zclSampleSw_LedSetTarget(0, TRUE);   // LED1亮
  pairingBlinkLedOn = TRUE;
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}

static void zclSampleSw_ProcessPairingBlink(void)
{
  if (!pairingBlinkActive) return;

  pairingBlinkTickCount++;

  if (pairingBlinkTickCount >= PAIRING_BLINK_TIMEOUT_TICKS)  // 5分钟超时
  {
    zclSampleSw_StopPairingBlink();
    return;
  }

  // 切换LED1状态
  if (pairingBlinkLedOn)
  {
    zclSampleSw_LedSetTarget(0, FALSE);
    pairingBlinkLedOn = FALSE;
  }
  else
  {
    zclSampleSw_LedSetTarget(0, TRUE);
    pairingBlinkLedOn = TRUE;
  }

  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}

static void zclSampleSw_StopPairingBlink(void)
{
  if (!pairingBlinkActive) return;
  pairingBlinkActive = FALSE;
  osal_stop_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT);
  zclSampleSw_UpdateRelayOutput(0);  // 恢复LED1显示继电器1状态
}
```

**调用时机**：
- `zclSampleSw_Init()` 末尾调用 `StartPairingBlink()`
- `ZDO_STATE_CHANGE` 收到 `DEV_ROUTER`（入网成功）调用 `StopPairingBlink()`

---

## F. 继电器控制

### F.1 触摸翻转继电器

**说明**：触摸操作翻转继电器，需主动上报（z2m 不知道本地事件）。

```c
static void zclSampleSw_ToggleRelay(uint8 idx)
{
  zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx];
  zclSampleSw_UpdateRelayOutput(idx);
  zclSampleSw_ReportOnOffState(idx);    // ⚠️ 对外通信, 可能受路由影响
  zclSampleSw_NvScheduleSave();         // 延迟写入NV
}
```

### F.2 ZCL 命令处理

**说明**：v1.0.9 移除主动 Report（z2m 通过 Default Response 确认，避免 3072 字节堆耗尽）。

```c
static void zclSampleSw_HandleOnOffCmd(uint8 idx, uint8 cmd)
{
  switch (cmd)
  {
    case COMMAND_ON:     zclSampleSw_RelayState[idx] = TRUE;   break;
    case COMMAND_OFF:    zclSampleSw_RelayState[idx] = FALSE;  break;
    case COMMAND_TOGGLE: zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx]; break;
    default: return;
  }
  zclSampleSw_UpdateRelayOutput(idx);
  // v1.0.9: 移除 zclSampleSw_ReportOnOffState(idx)
  // ZCL层自动发Default Response, z2m据此确认状态, 主动Report是冗余的
  zclSampleSw_NvScheduleSave();
}
```

### F.3 端点回调宏

```c
#define DEFINE_RELAY_ONOFF_CB(IDX, EP)                                  \
  static void zclSampleSw_OnOffCB_ep##EP(uint8 cmd)                     \
  {                                                                      \
    zclSampleSw_HandleOnOffCmd((IDX), cmd);                              \
  }                                                                      \
  static zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep##EP =    \
  {                                                                      \
    zclSampleSw_BasicResetCB,   /* pfnBasicReset */                     \
    NULL,                       /* pfnIdentifyTriggerEffect */          \
    zclSampleSw_OnOffCB_ep##EP, /* pfnOnOff */                         \
    NULL,                       /* pfnOnOff_OffWithEffect */            \
    NULL,                       /* pfnOnOff_OnWithRecallGlobalScene */ \
    NULL,                       /* pfnOnOff_OnWithTimedOff */          \
    NULL,                       /* pfnGroupRsp */                       \
    NULL,                       /* pfnLocation */                       \
    NULL                        /* pfnLocationRsp */                   \
  };

DEFINE_RELAY_ONOFF_CB(0, 1)
DEFINE_RELAY_ONOFF_CB(1, 2)
DEFINE_RELAY_ONOFF_CB(2, 3)
DEFINE_RELAY_ONOFF_CB(3, 4)
```

---

## 关键设计要点

1. **LED 反逻辑**：ACTIVE_LOW，写 0=亮、写 1=灭；继电器 ON 时 LED 灭、OFF 时 LED 亮
2. **LED1 与配网慢闪互斥**：`UpdateRelayOutput(0)` 中用 `pairingBlinkActive` 屏蔽，慢闪期间 LED1 由慢闪状态机独占
3. **防御性刷新**：触摸轮询每 50ms 调 `UpdateAllRelayOutputs()`，防止 Z-Stack 残留代码改写 P0_0~P0_3
4. **弃用 HalLedBlink**：直接 GPIO 操作，避免 HalLedState 与硬件不一致
5. **弃用软件 PWM**：v1.0.8 移除 5ms PWM 定时器，避免干扰协议栈时序
6. **触摸防抖**：连续 2 次（100ms）确认，仅上升沿触发一次翻转，长按不重复
7. **S1 长按 5s 复位**：闪烁 3 次（1.8s）后执行 Basic Reset + `bdb_resetLocalAction()`
8. **ZCL 命令不主动上报**：v1.0.9 移除 `HandleOnOffCmd` 中的 ReportOnOffState；仅本地触摸保留主动上报

---

## 对外通信功能（可能受路由影响）

以下功能涉及网络通信，在 linxee 分支实现时需谨慎评估，**不直接照搬 dev 代码**：

| 功能 | dev 实现 | 风险评估 |
|------|----------|----------|
| `zclSampleSw_ReportOnOffState()` | 触摸后上报 OnOff | 低风险：单次上报，不影响堆 |
| `zclSampleSw_ReportAllOnOffState()` | 入网后全量上报 | 低风险：入网后一次性 |
| `zclSampleSw_ReportInputState()` | 触摸后上报 input_state | 低风险：单次上报 |
| `HandleOnOffCmd` 中 Report | v1.0.9 已移除 | 已验证：移除是正确的 |
| BDB commissioning 模式选择 | 按 NV 状态选择 | 需验证：可能受路由影响 |
| `bdb_resetLocalAction()` | S1 复位时调用 | 低风险：本地发起的离开 |
