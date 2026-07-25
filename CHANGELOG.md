# Changelog - 4路智能开关 Zigbee 固件

本文件记录固件版本变更历史。版本号规则：`v主.次.修订`，主版本号变更表示重大功能里程碑。

---

## v0.2.4 - 2026-07-25

修复 S1 长按复位流程中的 LED 异常问题。根因是应用层直接操作 P0_0 绕过 HalLed 层导致 HalLedState 与硬件状态不一致，以及 HalLedBlink 闪烁参数过快（50ms on/200ms off，1秒内完成5次闪烁，视觉上只剩3次）。同时排查到 z2m 日志无离网请求是因 bdb_resetLocalAction() 在设备不在网络时直接重启不发送 NLME_LeaveReq。

### Fixed
- BUG-011-1: LED1 在无操作一段时间后自动熄灭（z2m 显示 OFF 时 LED1 应亮）。根因是应用层直接操作 P0_0 绕过 HalLed 层，HalLedState 与硬件状态不一致。修复：在触摸轮询中每 100ms 调用 `zclSampleSw_UpdateAllRelayOutputs()` 进行防御性刷新，确保即使被协议栈残留代码干扰也能快速恢复正确状态。
- BUG-011-2: 长按 S1 5秒复位时 LED 闪烁频率过快（5次闪烁看起来只有3次）。根因是 `HalLedBlink(HAL_LED_ALL, 5, 50, 200)` 在 1 秒内完成。修复：实现自定义闪烁状态机 `zclSampleSw_StartResetBlink()` + `zclSampleSw_ProcessResetBlink()`，3次闪烁（6次状态切换），300ms亮/300ms灭，共 1.8 秒，直接操作 GPIO 绕过 HalLed 层。
- BUG-011-3: 长按 S1 复位时 z2m 日志未显示 Zigbee 网络离开请求。根因是设备不在网络时 `bdb_resetLocalAction()` 直接 `ZDApp_ResetTimerStart(500)` 不发送 NLME_LeaveReq。修复：闪烁完成后显式调用 `bdb_resetLocalAction()`，由协议栈根据网络状态自动决定发送 NLME_LeaveReq 或直接重启。

### Changed
- 新增 `SAMPLESW_RESET_BLINK_EVT` (0x0040) 事件定义（zcl_samplesw.h）
- 新增 `RESET_BLINK_TOTAL_COUNT` (6) 和 `RESET_BLINK_PERIOD_MS` (300) 宏定义
- 新增 `resetBlinkCount` 状态机计数器
- 新增 `zclSampleSw_StartResetBlink()` 和 `zclSampleSw_ProcessResetBlink()` 函数实现
- 修改 S1 长按检测：检测到 5 秒长按后启动闪烁状态机，停止触摸轮询避免干扰
- 新增每 100ms 防御性刷新 LED 状态（在 `zclSampleSw_ProcessTouchPoll()` 末尾）
- 版本号递增: v0.2.3 → v0.2.4 (BUG修复, 修订号递增)

### 设计决策
- **为何不用 HalLedBlink**: HalLedBlink 会修改 HalLedState 全局变量，与应用层直接 GPIO 操作冲突，导致状态不一致。自定义状态机直接操作 P0_0，完全绕过 HalLed 层。
- **为何保留 bdb_resetLocalAction() 调用**: 让协议栈自动判断设备网络状态，统一处理离网请求和重启流程，避免应用层重复实现协议栈逻辑。
- **防御性刷新的必要性**: Z-Stack 官方示例的 hal_key.c 残留代码存在 P2.0（继电器4）和 P0.6（触摸输入3）引脚冲突，会周期性干扰 GPIO。防御性刷新确保 LED 状态在被干扰后能快速恢复。

### 已知限制
- 本次修复采用防御性刷新缓解 LED 异常，未根除 hal_key.c 残留代码干扰（将在 v1.0.0 深度重构中彻底清理）

### Migration
- 烧录 v0.2.4 后，LED1 在待机状态下应保持稳定显示继电器状态
- 长按 S1 5 秒后会看到清晰的 3 次 LED1 闪烁（约 1.8 秒），然后设备执行复位
- 若设备在网络中，z2m 日志应显示 Zigbee 网络离开请求

---

## v0.2.3 - 2026-07-25

修复无操作时 Z2M 周期性收到 action 事件的 BUG。根因是 v0.2.1 引入的 30 秒周期性上报机制触发 z2m 的 `state_action` 选项，每次上报都生成无意义的 action 事件。

### Fixed
- BUG-010: 无操作时 Z2M 每 30 秒收到 4 个 action 事件 (on_l1/on_l2/on_l3/on_l4)。v0.2.1 为修复信号丢失导致状态失同步 (BUG-008) 引入了 `SAMPLESW_STATE_REPORT_EVT` 30 秒周期性上报。但 z2m 的 `fz.on_off` 转换器在 `state_action: true` 选项启用时, 会把每次 OnOff 属性上报转换为 action 事件。即使状态未变化, 周期性上报也会触发无意义的 action, 干扰 HA 自动化。

### Changed
- 移除 `SAMPLESW_STATE_REPORT_EVT` (0x2000) 事件定义和事件处理
- 移除 `STATE_REPORT_INTERVAL_MS` (30000ms) 宏定义
- 移除 `ZDO_STATE_CHANGE` 入网成功后启动周期定时器的代码
- 保留 `ZDO_STATE_CHANGE` 入网成功后立即上报 (BUG-007 修复)
- 保留触摸/远程操作后的立即上报 (BUG-002 修复)
- 版本号递增: v0.2.2 → v0.2.3 (BUG修复, 修订号递增)

### Trade-offs
- 信号瞬时不好导致 Report 丢失时, 不再有 30 秒周期性上报自动恢复状态
- 状态同步保障改为: 1) 入网后立即上报 2) 触摸/远程操作后立即上报 3) z2m availability 检测
- 实际影响: 若 Report 丢失, 下次操作时会重新上报恢复同步; 信号持续不好时设备亦无法响应 Z2M 命令, 周期性上报也无法解决

### Migration
- 烧录 v0.2.3 后, 无操作时 Z2M 不再收到 action 事件
- 若用户依赖周期性 action 触发 HA 自动化, 需改为基于 state 变化或操作触发的 action
- z2m 的 `state_action` 选项仍可保留启用, 仅在状态变化时才会生成 action

---

## v0.2.2 - 2026-07-25

修复断电恢复时 4 路开关全部为 ON 的严重 BUG。根因是 startUpOnOff 配置 4 路共用一个全局变量，Z2M 写入 4 路独立配置时被互相覆盖。

### Fixed
- BUG-009: 断电恢复时 4 路开关全部为 ON。Z2M HGZB-4S 定义中 4 路有独立 `power_on_behavior` 配置, 但固件中 `zclSampleSw_StartUpOnOff` 是单变量, 4 个 EP 的 startUpOnOff 属性都指向它。Z2M 写入 l1=off→l2=off→l3=on→l4=on 时, 最后写入的 on(0x01) 覆盖所有, 4 路全部按 ON 恢复。

### Changed
- `zclSampleSw_StartUpOnOff`: 单变量 → `uint8[4]` 数组, 4 路独立配置
- `startupOnOffCached`: 单变量 → `uint8[4]` 数组, 用于检测 4 路独立变化
- 4 个 EP 属性表 (`zclSampleSw_RelayAttrs_ep1~4`) 的 startUpOnOff 属性分别指向 `&zclSampleSw_StartUpOnOff[0~3]`
- `zclSampleSw_NvLoadPowerOnState`: 改为按每路独立 startUpOnOff 策略恢复 (每路可独立 off/on/toggle/previous)
- `zclSampleSw_NvProcessSave`: NV 写入长度从 1 字节改为 4 字节
- `zclSampleSw_ProcessTouchPoll`: 用 `osal_memcmp` 检测 4 路数组变化
- NV ID: `SAMPLESW_NV_ID_STARTUP_ONOFF` 从 0x0F11 改为 0x0F12, 避开旧的 1 字节不兼容数据 (旧 NV 项遗留但不使用, 无副作用)
- 版本号递增: v0.2.1 → v0.2.2 (BUG修复, 修订号递增)

### Migration
- 用户烧录 v0.2.2 后, 第一次启动 NV ID 0x0F12 不存在, 会用默认值 `PREVIOUS` 创建 4 路
- 首次启动 4 路都按 `PREVIOUS` (恢复断电前状态) 恢复
- 用户需要在 Z2M 重新配置每路 `power_on_behavior` (此前 Z2M 端显示的配置不会真正下发到设备)

---

## v0.2.1 - 2026-07-25

修复 Z2M 状态与设备实际状态不同步的两个 BUG。新增入网后立即上报和周期性上报机制。

### Fixed
- BUG-007: 断电恢复后 Z2M 状态不同步。上电按 startUpOnOff 策略恢复继电器状态后未主动上报, Z2M 保持断电前记录的旧状态。新增 `ZDO_STATE_CHANGE` 转 `DEV_ROUTER` 时立即上报所有 4 路 OnOff 状态。
- BUG-008: 信号瞬时不好导致 Z2M 状态永久失同步。ZCL Report 无 APS ACK 单向发送, 丢失后无重传, 信号恢复也无重新同步事件。新增 30 秒周期性上报 `SAMPLESW_STATE_REPORT_EVT`, 即使某次 Report 丢失也能在下次周期恢复。

### Added
- `zclSampleSw_ReportAllOnOffState()`: 一次性上报 4 路 OnOff 状态
- `SAMPLESW_STATE_REPORT_EVT` (0x2000): 30 秒周期性状态上报事件
- `ZDO_STATE_CHANGE` 入网成功后立即上报 + 启动周期定时器

### Changed
- 启用 `zclSampleSw_NwkState` 全局变量跟踪网络状态 (此前仅声明未赋值), 避免重复入网触发多次上报
- 版本号递增: v0.2.0 → v0.2.1 (BUG修复, 修订号递增)

---

## v0.2.0 - 2026-07-25

新增断电记忆功能，设备断电后可恢复之前的继电器状态。换壳到 HGZB-4S 以获得 Z2M 内置 powerOnBehavior 支持，免 External Converter。

### Added
- 断电记忆功能: 基于ZCL标准startUpOnOff属性(0x4003), 支持4种上电策略(off/on/toggle/previous)
- NV持久化存储: 继电器状态和startUpOnOff配置写入Flash, 断电不丢失
- Flash寿命优化: 延迟写入(Write Coalescing, 5秒合并) + 对比写入(Compare-Before-Write), 避免无意义擦写
- startUpOnOff属性变更检测: 100ms周期轮询Z2M远程修改, 自动持久化新配置

### Changed
- **换壳 alab.switch → HGZB-4S (Nue/3A)**: ModelId 改为 `LXN-4S27LX1.0`, 获得Z2M内置powerOnBehavior支持, 免External Converter
- NV ID选型: 使用0x0F10/0x0F11, 避开Z-Stack系统区(0x0001~0x0097)和ZNP保留区(0x0F01~0x0F07)
- 版本号递增: v0.1.2 → v0.2.0 (新增功能, 次版本号递增)

### Removed
- alab_switch_poweron.js: 换壳到HGZB-4S后无需External Converter (HGZB-4S的m.onOff默认启用powerOnBehavior)

### Trade-offs
- 换壳到HGZB-4S后, Z2M不再识别EP5-8的input_state触摸按键状态显示
- 触摸操作仍能通过继电器状态变化反映到Z2M (触摸→继电器翻转→ZCL上报→Z2M显示开关状态变化)
- 固件保留EP5-8端点代码, 未来可通过向Z2M上游提PR恢复input_state功能

---

## v0.1.2 - 2026-07-25

新增EP5-8输入状态端点(genAnalogInput)，修复z2m input_state功能。

### Added
- EP5-8 genAnalogInput cluster: 4路触摸输入状态上报(presentValue, single_float, 1.0f=触摸/0.0f=未触摸)
- 触摸状态变化时主动上报input_state到z2m

---

## v0.1.1 - 2026-07-24

触摸检测BUG修复版本。

### Fixed
- 触摸物理按键无反应：经4通道分方案诊断固件验证，确认方案A(低电平=触摸)正确，恢复4通道统一方案A (待提交)

---

## v0.1.0 - 2026-07-24

首个可测试版本，核心功能基本完整。

### Added
- S1 长按5秒工厂复位功能（复用触摸轮询定时器检测 P1_3） (`9119886`)
- ZCL 属性上报功能 `zclSampleSw_ReportOnOffState()`，确保 z2m 状态同步 (`23c47cc`)
- 继电器控制与触摸输入检测，LED 专用于继电器状态指示 (`d982fa5`)
- 4路 On/Off 端点（EP1-4）适配 alab.switch 借壳型号 (`f420908`)
- 86 四路智能开关硬件引脚配置适配 (`6d99754`)

### Fixed
- 触摸输入检测失效：移除自适应基线采样，改用直接低电平检测（WTC6106BSI 固定极性） (`23c47cc`)
- z2m 收不到触摸触发的状态变化：新增 ZCL Report Attributes 主动上报 (`23c47cc`)
- 触摸物理按键无反应：经4通道分方案诊断固件验证，确认方案A(低电平=触摸)正确，恢复4通道统一方案A (待提交)

### Changed
- 恢复 CC2592 禁用状态（真实设备无放大芯片） (`1fb0d2a`)
- 借壳型号确定为 alab.switch (`b866492`)

---

## v0.0.1 - 2026-07-21

项目初始化阶段。

### Added
- 添加 CodeWiki.md 文档 (`53d0369`)
- CC2530+CC2592 验证可入网 (`03b5150`)
- 项目初始化，基于 Z-Stack 3.0 SampleSwitch (`3e92482`)
