# LED 控制

## 1. 概述

86四路智能开关使用 4 颗 LED（P0_0~P0_3）指示 4 路继电器状态及配网/复位状态。由于 CC2530 无硬件 PWM，且软件 PWM 会干扰 Zigbee 协议栈 MAC 时序，本方案采用**直接 GPIO 控制**，所有 LED 控制点统一通过 `LedSetTarget` 接口下发，由 `LedWriteGpio` 底层操作 GPIO。

## 2. LED 引脚与电气特性

| LED | 引脚 | 逻辑 | 用途 |
|-----|------|------|------|
| LED1 | P0_0 | 反逻辑（写 0=亮，写 1=灭） | 继电器 1 状态 + 配网慢闪 + 复位闪烁 |
| LED2 | P0_1 | 反逻辑 | 继电器 2 状态 |
| LED3 | P0_2 | 反逻辑 | 继电器 3 状态 |
| LED4 | P0_3 | 反逻辑 | 继电器 4 状态 |

**LED 与继电器联动**：继电器 OFF → LED 亮（写 0），继电器 ON → LED 灭（写 1）

## 3. 禁止使用软件 PWM

### 3.1 历史教训

v1.0.6~v1.0.7 版本曾引入软件 PWM 调光方案，结果导致 RF 信号严重不稳定：

- **5ms 周期（200Hz）PWM 定时器持续运行**，会干扰 Z-Stack 协议栈 MAC 时序
- 表现：信号 LQI 波动大（10~105），甚至信号归零，配网失败/掉线频繁
- v1.0.7 尝试在未入网时禁用 PWM，仍未解决稳定性问题

### 3.2 v1.0.8 最终方案

**完全移除 PWM 调光功能**，恢复 LED 直接写 GPIO 控制亮度（100%），优先保证 RF 通信稳定性。

源码注释（zcl_samplesw.c）：

```c
// v1.0.8变更: 移除LED软件PWM调光功能
// 原因: PWM定时器(5ms周期)持续运行会占用MCU资源, 影响Zigbee协议栈时序, 导致信号不稳定
// 方案: 恢复LED直接写GPIO控制亮度(100%), 优先保证RF通信稳定性
```

```c
// v1.0.8变更: 移除SAMPLESW_LED_PWM_EVT事件处理 (PWM调光影响RF信号稳定性)
```

## 4. LED 状态层架构

```
应用层 (继电器联动 / 配网慢闪 / 复位闪烁)
    ↓ 调用
zclSampleSw_LedSetTarget(idx, on)   ← 应用层统一接口
    ↓ 调用
zclSampleSw_LedWriteGpio(idx, on)   ← 底层 GPIO 写入
    ↓ 操作
P0_0 / P0_1 / P0_2 / P0_3
```

### 4.1 底层 GPIO 写入（LedWriteGpio）

```c
/*********************************************************************
 * @fn      zclSampleSw_LedWriteGpio
 *
 * @brief   v1.0.6新增: LED底层GPIO写入 (反逻辑: on=TRUE→写0亮, on=FALSE→写1灭)
 * @param   idx - LED索引 0~3 (LED1~4 → P0_0~P0_3)
 * @param   on  - TRUE=亮, FALSE=灭
 * @return  none
 */
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

### 4.2 应用层统一接口（LedSetTarget）

所有 LED 控制点（继电器联动 / 配网慢闪 / 复位闪烁）统一调用本函数。v1.0.8 移除 PWM 后内部直接调用 `LedWriteGpio`：

```c
/*********************************************************************
 * @fn      zclSampleSw_LedSetTarget
 *
 * @brief   v1.0.6新增: 设置LED期望亮灭状态 (替代直接P0_x=val)
 *          所有LED控制点(继电器联动/配网慢闪/复位闪烁)统一调用本函数
 *          v1.0.8变更: 移除PWM调光, 恢复直接写GPIO控制亮度(100%)
 * @param   idx - LED索引 0~3
 * @param   on  - TRUE=期望亮, FALSE=期望灭
 * @return  none
 */
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on)
{
  zclSampleSw_LedWriteGpio(idx, on);
}
```

## 5. 继电器联动控制

### 5.1 单通道更新（UpdateRelayOutput）

根据 `zclSampleSw_RelayState[idx]` 更新继电器 GPIO 与 LED 状态。注意 LED1 在配网慢闪期间被跳过：

```c
void zclSampleSw_UpdateRelayOutput(uint8 idx)
{
  uint8 on = zclSampleSw_RelayState[idx];

  switch (idx)
  {
    case 0:
      P1_0 = on ? 0 : 1;        // 继电器1: ON=低电平
      // v1.0.4: 配网中LED1慢闪激活时, 不刷新LED1, 由慢闪状态机控制
      if (!pairingBlinkActive)
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
```

### 5.2 全通道更新（UpdateAllRelayOutputs）

```c
void zclSampleSw_UpdateAllRelayOutputs(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_UpdateRelayOutput(i);
  }
}
```

## 6. 配网慢闪状态机（1Hz）

### 6.1 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 周期 `PAIRING_BLINK_PERIOD_MS` | 500ms | 每次亮或灭的持续时间（1Hz） |
| 超时 `SAMPLESW_PAIRING_TIMEOUT_MS` | 5 分钟 | 与 BDB NWK_STEERING 超时接近 |
| 启动时机 | `zclSampleSw_Init()` 末尾 | 设备未入网时启动 |
| 停止时机 | ZDO_STATE_CHANGE 收到 DEV_ROUTER 或超时 | 入网成功或超时 |
| 操作对象 | 仅 LED1（P0_0） | 不影响其他 LED 与继电器状态 |

参数定义（zcl_samplesw.h / zcl_samplesw.c）：

```c
// v1.0.4新增: 配网中LED1慢闪状态机 (业界惯例, 提示用户正在配网)
// 启动时机: zclSampleSw_Init()末尾, 设备未入网时启动
// 停止时机: ZDO_STATE_CHANGE收到DEV_ROUTER(已入网) 或 超时(5分钟)
// 闪烁参数: 1Hz, 500ms亮/500ms灭, 仅操作LED1(P0_0), 不影响继电器状态
#define PAIRING_BLINK_PERIOD_MS   500  // 每次亮或灭的持续时间 (1Hz)
static uint8 pairingBlinkActive = FALSE;  // 慢闪是否活跃
static uint8 pairingBlinkLedOn = FALSE;   // 当前LED1是否亮(反逻辑: 0=亮)
static uint16 pairingBlinkTickCount = 0;  // 已闪烁的tick数(每500ms+1, 用于超时判断)
// 配网超时阈值: 5分钟 = 300秒 = 600个500ms tick
#define PAIRING_BLINK_TIMEOUT_TICKS   (SAMPLESW_PAIRING_TIMEOUT_MS / PAIRING_BLINK_PERIOD_MS)
```

```c
// 配网中LED1慢闪超时时间 (毫秒)
// 业界惯例: IKEA 60秒, Aqara 90秒, Tuya 120秒; 此处取5分钟, 与BDB NWK_STEERING超时接近
#define SAMPLESW_PAIRING_TIMEOUT_MS         (5 * 60 * 1000UL)
```

### 6.2 启动慢闪（StartPairingBlink）

```c
/*********************************************************************
 * @fn      zclSampleSw_StartPairingBlink
 * @brief   v1.0.4新增: 启动配网中LED1慢闪状态机
 *          业界惯例: 设备配网中LED1慢闪(1Hz), 提示用户正在配网
 *          停止条件: 1)ZDO_STATE_CHANGE收到DEV_ROUTER(入网成功)
 *                   2)超时5分钟(SAMPLESW_PAIRING_TIMEOUT_MS)
 *          仅操作LED1(P0_0), 不影响继电器状态和其他LED
 * @return  none
 */
static void zclSampleSw_StartPairingBlink(void)
{
  pairingBlinkActive = TRUE;
  pairingBlinkLedOn = FALSE;  // 初始为灭
  pairingBlinkTickCount = 0;
  // LED1亮 (经PWM层, 50%亮度)
  zclSampleSw_LedSetTarget(0, TRUE);
  pairingBlinkLedOn = TRUE;
  // 启动500ms定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}
```

### 6.3 慢闪处理（ProcessPairingBlink）

每 500ms 切换 LED1 状态，超时后停止：

```c
/*********************************************************************
 * @fn      zclSampleSw_ProcessPairingBlink
 * @brief   v1.0.4新增: 配网中LED1慢闪处理
 *          每500ms切换LED1状态(亮/灭), 形成1Hz闪烁
 *          超时5分钟后自动停止, 恢复LED1显示继电器1状态
 * @return  none
 */
static void zclSampleSw_ProcessPairingBlink(void)
{
  if (!pairingBlinkActive) return;

  pairingBlinkTickCount++;

  // 超时检查: 5分钟无入网则停止慢闪
  if (pairingBlinkTickCount >= PAIRING_BLINK_TIMEOUT_TICKS)
  {
    zclSampleSw_StopPairingBlink();
    return;
  }

  // 切换LED1状态 (经PWM层, 亮时50%亮度)
  if (pairingBlinkLedOn)
  {
    zclSampleSw_LedSetTarget(0, FALSE);  // 灭
    pairingBlinkLedOn = FALSE;
  }
  else
  {
    zclSampleSw_LedSetTarget(0, TRUE);   // 亮
    pairingBlinkLedOn = TRUE;
  }

  // 重启500ms定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}
```

### 6.4 停止慢闪（StopPairingBlink）

入网成功或超时后停止，恢复 LED1 显示继电器 1 状态：

```c
/*********************************************************************
 * @fn      zclSampleSw_StopPairingBlink
 * @brief   v1.0.4新增: 停止配网中LED1慢闪, 恢复LED1显示继电器1状态
 *          由ZDO_STATE_CHANGE(DEV_ROUTER)或超时调用
 * @return  none
 */
static void zclSampleSw_StopPairingBlink(void)
{
  if (!pairingBlinkActive) return;

  pairingBlinkActive = FALSE;
  osal_stop_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT);

  // 恢复LED1显示继电器1状态
  zclSampleSw_UpdateRelayOutput(0);
}
```

### 6.5 事件处理

事件 ID 与处理（zcl_samplesw.h / zcl_samplesw.c）：

```c
// v1.0.4新增: 配网中LED1慢闪事件 (业界惯例, 提示用户正在配网)
// 1Hz闪烁(500ms亮/500ms灭), 入网成功(DEV_ROUTER)或超时(5分钟)后停止
// 复用原UI事件号0x0020(SAMPLEAPP_KEY_AUTO_REPEAT_EVT已废弃)
#define SAMPLESW_PAIRING_BLINK_EVT          0x0020
```

```c
// v1.0.4新增: 配网中LED1慢闪事件处理
// 1Hz闪烁(500ms亮/500ms灭), 入网成功或超时(5分钟)后停止
if ( events & SAMPLESW_PAIRING_BLINK_EVT )
{
  zclSampleSw_ProcessPairingBlink();
  return ( events ^ SAMPLESW_PAIRING_BLINK_EVT );
}
```

## 7. 复位闪烁状态机（3 次）

### 7.1 为什么不用 HalLedBlink

**HalLedBlink 会修改 `HalLedState` 全局变量**，与应用层直接 GPIO 操作冲突：
- 应用层通过 `LedWriteGpio` 直接写 P0_0~P0_3，但 `HalLedState` 不会同步更新
- 后续若代码读取 `HalLedState` 判断 LED 状态，会得到错误结果
- 表现：LED1 在复位闪烁后异常，无法正确恢复继电器状态指示

**解决方案**：自定义状态机，仅操作 LED1（P0_0），不影响 LED2~4。

### 7.2 参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 总切换次数 `RESET_BLINK_TOTAL_COUNT` | 6 | 3 次闪烁 = 6 次状态切换（亮/灭/亮/灭/亮/灭） |
| 单次持续时间 `RESET_BLINK_PERIOD_MS` | 300ms | 300ms 亮 / 300ms 灭 |
| 总时长 | 1.8 秒 | 6×300ms |
| 操作对象 | 仅 LED1（P0_0） | 不影响 LED2~4 |
| 完成后动作 | Basic Reset + BDB Reset to FN | 执行复位流程 |

参数定义（zcl_samplesw.c）：

```c
// BUG-011修复: S1复位LED闪烁状态机
// 用直接GPIO操作替代HalLedBlink, 避免HalLedState与实际硬件状态不一致
// 闪烁参数: 3次, 300ms亮/300ms灭, 共1.8秒
#define RESET_BLINK_TOTAL_COUNT   6    // 3次闪烁 = 6次状态切换(亮/灭/亮/灭/亮/灭)
#define RESET_BLINK_PERIOD_MS     300  // 每次亮或灭的持续时间
static uint8 resetBlinkCount = 0;      // 闪烁状态机计数器(0~6)
```

### 7.3 启动闪烁（StartResetBlink）

```c
/*********************************************************************
 * @fn      zclSampleSw_StartResetBlink
 *
 * @brief   BUG-011修复: 启动S1复位LED闪烁状态机
 *          替代HalLedBlink, 避免HalLedState与硬件状态不一致导致LED1异常
 *          闪烁参数: 3次, 300ms亮/300ms灭, 共1.8秒, 完成后执行复位
 *          仅操作LED1(P0_0), 不影响LED2~4(继电器状态指示)
 *
 * @return  none
 */
static void zclSampleSw_StartResetBlink(void)
{
  // 初始化闪烁状态机计数器
  resetBlinkCount = RESET_BLINK_TOTAL_COUNT;

  // 第1次切换: LED1亮 (经PWM层, 50%亮度)
  zclSampleSw_LedSetTarget(0, TRUE);

  // 启动300ms定时器, 触发下一次切换
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
}
```

### 7.4 闪烁处理（ProcessResetBlink）

每次切换 LED1 状态，闪烁完成后执行复位流程：

```c
/*********************************************************************
 * @fn      zclSampleSw_ProcessResetBlink
 *
 * @brief   BUG-011修复: S1复位LED闪烁状态机处理
 *          每次切换LED1状态, 闪烁完成后执行复位流程
 *          闪烁序列: 亮(启动)-灭-亮-灭-亮-灭(完成) = 3次闪烁
 *          复位流程: Basic Reset + BDB Reset to FN(发送NLME_LeaveReq)
 *
 * @return  none
 */
static void zclSampleSw_ProcessResetBlink(void)
{
  resetBlinkCount--;

  if (resetBlinkCount > 0)
  {
    // 切换LED1状态: 奇数count=灭, 偶数count=亮 (经PWM层, 50%亮度)
    // count=5->灭, =4->亮, =3->灭, =2->亮, =1->灭 (共3次闪烁)
    zclSampleSw_LedSetTarget(0, !(resetBlinkCount % 2));

    // 启动下一次切换定时器
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
  }
  else
  {
    // 闪烁完成, 执行复位流程

    // 1. 调用Basic Reset回调, 重置ZCL属性到默认值
    zclSampleSw_BasicResetCB();

    // 2. 调用BDB Reset to FN, 让协议栈处理网络离开请求
    //    bdb_resetLocalAction()内部判断:
    //    - 若设备在网络中: 发送NLME_LeaveReq(z2m日志会显示leave请求)
    //    - 若设备不在网络中: 调用ZDApp_ResetTimerStart(500)直接重启
    bdb_resetLocalAction();

    // 3. 刷新所有继电器/LED状态到默认(继电器OFF, LED亮)
    zclSampleSw_UpdateAllRelayOutputs();
  }
}
```

### 7.5 事件处理

```c
// BUG-011修复: S1复位LED闪烁事件处理
// 用直接GPIO操作替代HalLedBlink, 避免HalLedState与硬件状态不一致
if ( events & SAMPLESW_RESET_BLINK_EVT )
{
  zclSampleSw_ProcessResetBlink();
  return ( events ^ SAMPLESW_RESET_BLINK_EVT );
}
```

## 8. 防御性 LED 刷新

Z-Stack 协议栈残留代码（如 ZDApp.c 中 Router 启动逻辑）可能意外修改 P0_0~P0_3。在 `ProcessTouchPoll` 中每 50ms 防御性刷新一次 LED 状态：

```c
// BUG-011修复: 防御性刷新LED状态 (每50ms, 跟随触摸轮询周期)
// Z-Stack协议栈残留代码可能意外修改P0_0~P0_3, 定期刷新确保LED正确显示继电器状态
zclSampleSw_UpdateAllRelayOutputs();
```

并在 `ZDO_STATE_CHANGE` 事件中刷新一次，恢复正确显示（协议栈 Router 启动时操作过 LED3/LED4）：

```c
case ZDO_STATE_CHANGE:
  // 86开关: 协议栈Router启动时会操作LED3/LED4(ZDApp.c), 覆盖继电器状态灯。
  // 入网状态变化后重新刷新所有继电器/LED输出, 恢复正确显示。
  zclSampleSw_UpdateAllRelayOutputs();
  ...
```

## 9. 经验教训

### 9.1 软件 PWM 不可用

- 5ms 周期（200Hz）PWM 定时器持续运行，干扰 Z-Stack 协议栈 MAC 时序
- 表现：信号 LQI 波动（10~105）、信号归零、配网失败/掉线
- v1.0.7 尝试未入网时禁用 PWM 仍无效
- **正确做法**：直接 GPIO 控制，亮度 100%

### 9.2 HalLedBlink 不可用

- `HalLedBlink` 修改 `HalLedState` 全局变量，与应用层直接 GPIO 操作冲突
- `HalLedState` 与实际硬件状态不一致，导致 LED1 异常
- **正确做法**：自定义状态机，仅操作 LED1，不影响 LED2~4

## 10. 相关文件

- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.c`
  - `zclSampleSw_LedWriteGpio()`（底层 GPIO 写入）
  - `zclSampleSw_LedSetTarget()`（应用层接口）
  - `zclSampleSw_UpdateRelayOutput()` / `zclSampleSw_UpdateAllRelayOutputs()`（继电器联动）
  - `zclSampleSw_StartPairingBlink()` / `zclSampleSw_ProcessPairingBlink()` / `zclSampleSw_StopPairingBlink()`（配网慢闪）
  - `zclSampleSw_StartResetBlink()` / `zclSampleSw_ProcessResetBlink()`（复位闪烁）
- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.h`
  - `SAMPLESW_PAIRING_BLINK_EVT` / `SAMPLESW_RESET_BLINK_EVT`（事件 ID）
  - `SAMPLESW_PAIRING_TIMEOUT_MS`（配网超时）
