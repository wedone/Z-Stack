# Changelog - 4路智能开关 Zigbee 固件

本文件记录固件版本变更历史。版本号规则：`v主.次.修订`，主版本号变更表示重大功能里程碑。

---

## v1.0.10 - 2026-07-29

修复S1软复位后无法入网问题，区分已配网/新设备选择BDB commissioning模式。

### 问题背景
用户反馈S1长按5秒复位后，设备无法重新入网（z2m无任何日志），必须断电再上电才能入网。有时3米无遮挡入网困难，5米两堵墙反而能入网。

### 根因分析
`zclSampleSw_Init()` 中无条件调用 `bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP)`，而该常量值为 `0x00`，不设置任何 commissioning mode 位。

BDB commissioning 流程（[bdb.c:817](file:///d:/VC/Z-Stack/Components/stack/bdb/bdb.c#L817)）对新设备（`bdbNodeIsOnANetwork == FALSE`）的处理：
1. S1复位调用 `bdb_resetLocalAction()` → `bdb_setFN()` 清除 `bdbNodeIsOnANetwork` 和 NV 网络状态
2. 软复位后 `zclSampleSw_Init()` 传入 mode=0x00
3. BDB 读取 NV，`bdbNodeIsOnANetwork == FALSE`
4. BDB 检查 `bdbCommissioningMode == 0`（确实为0，无任何 mode 位）
5. BDB 直接 report INITIALIZATION 失败并 return
6. **不会调用 `ZDO_InitDevice()`，不会触发 NWK_STEERING**
7. 设备无法发现网络，无法入网

原代码注释"新设备: 触发BDB initialization后启动NWK_STEERING commissioning"是错误的。

### 修复方案
在 `zclSampleSw_Init()` 中读取 NV `ZCD_NV_BDBNODEISONANETWORK` 状态，根据状态选择 commissioning 模式：
- `TRUE`（已配网设备）→ `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP`（恢复网络）
- `FALSE`（工厂新设备/S1复位后）→ `BDB_COMMISSIONING_MODE_NWK_STEERING`（发现网络并加入）

### Changed
- `zcl_samplesw.c`: `zclSampleSw_Init()` 根据 NV 状态选择 commissioning 模式
- `zcl_samplesw_data.c`: SwBuildId v1.0.9 → v1.0.10, DateCode 20260728 → 20260729

---

## v1.0.9 - 2026-07-28

移除ZCL命令处理后的主动Report，解决连续z2m操作3轮后设备卡死问题。

### 问题背景
用户反馈连续通过z2m操作设备开关（约3轮来回，12次ZCL命令）后，设备完全无响应（linkquality=0），约4分钟后自动恢复（看门狗复位后重启）。

### 根因分析
- CC2530 Router堆大小仅3072字节（[OnBoard.h:216](file:///d:/VC/Z-Stack/Projects/zstack/ZMain/TI2530DB/OnBoard.h#L216)）
- 每次ZCL OnOff命令处理产生2条AF消息：主动Report + Default Response
- 3轮×4端点×2消息 = 24条AF消息，占用约1200字节堆
- 加上协议栈自身使用，总计接近3072字节上限
- 堆耗尽后OSAL调度器卡死，看门狗超时复位

### 修复方案
移除 `zclSampleSw_HandleOnOffCmd()` 中的 `zclSampleSw_ReportOnOffState(idx)` 调用。

**理由**：
- ZCL层会自动发送Default Response，z2m据此确认命令已接收并更新状态
- 主动Report是冗余的，白白浪费堆内存
- 触摸操作（`ToggleRelay`）仍保留主动上报，因为z2m不知道本地触摸事件

### 消息量对比
| 场景 | v1.0.8 (修复前) | v1.0.9 (修复后) |
|------|-----------------|-----------------|
| 单次ZCL OnOff命令 | 2条AF消息 (Report + DefaultRsp) | 1条AF消息 (DefaultRsp) |
| 3轮×4端点 | 24条AF消息 | 12条AF消息 |
| 堆占用 | ~1200字节 | ~600字节 |

### Changed
- `zcl_samplesw.c`: `zclSampleSw_HandleOnOffCmd()` 移除 `zclSampleSw_ReportOnOffState(idx)` 调用
- `zcl_samplesw_data.c`: SwBuildId v1.0.8 → v1.0.9
- 新增 `Projects/zstack/HomeAutomation/HGZBSwitch/CC2530DB/burn.py`: 一键烧录脚本（hex→bin→上传→烧录→校验自动化）

### 文档更新
- `Documents/烧录说明.md`: 新增"一键烧录"章节，原手动流程降级为"手动烧录流程"
- `Documents/编译说明.md`: 工程路径从 SampleSwitch 更新为 HGZBSwitch，版本号示例更新到 v1.0.9
- `Documents/项目进展.md`: 当前版本 v1.0.6 → v1.0.9，补充 v1.0.7~v1.0.9 里程碑和测试要点
- `Documents/项目文档索引.md`: 当前版本号更新，快速定位新增 burn.py 入口

### 验证要点
- 连续z2m操作开关10轮以上不应卡死
- 触摸操作后z2m状态应正常更新（保留主动上报）
- 入网后z2m状态应正常同步（保留ReportAllOnOffState）
- linkquality应稳定，不再出现归零现象

---

## v1.0.8 - 2026-07-28

移除LED软件PWM调光功能（影响RF信号稳定性），保持发射功率4dBm。

### 问题背景
用户反馈v1.0.6/v1.0.7引入LED软件PWM调光后，即使入网成功后PWM定时器持续运行仍会影响开关的Zigbee信号稳定性。推测原因是PWM定时器（5ms周期）持续运行占用MCU资源，干扰协议栈MAC时序。

### 修复方案
1. **移除PWM调光功能**：删除所有PWM相关代码（事件、变量、函数），LED恢复直接写GPIO控制亮度（100%）
2. **发射功率**：保持 4 dBm（TX_PWR_PLUS_4），测试 5/7 dBm 均无改善，根因是 PWM 干扰而非功率不足

### 移除的代码
- `zcl_samplesw.h`: `SAMPLESW_LED_PWM_EVT`、`SAMPLESW_LED_PWM_PERIOD_MS` 宏定义
- `zcl_samplesw.c`: `ledTargetOn[]`、`ledPwmCounter`、`ledPwmEnabled` 变量
- `zcl_samplesw.c`: `zclSampleSw_LedPwmApply()`、`zclSampleSw_LedPwmEnable()` 函数
- `zcl_samplesw.c`: `SAMPLESW_LED_PWM_EVT` 事件处理逻辑
- `zcl_samplesw.c`: `zclSampleSw_Init()` 中无条件启动PWM定时器的代码
- `zcl_samplesw.c`: `ZDO_STATE_CHANGE` 中 `zclSampleSw_LedPwmEnable()` 调用

### 简化后的LED控制
- `zclSampleSw_LedSetTarget(idx, on)` 简化为直接调用 `zclSampleSw_LedWriteGpio(idx, on)`
- LED亮度恢复为100%（无PWM调光）

### 发射功率对比
| 版本 | 发射功率 | 信号稳定性 |
|------|---------|-----------|
| v1.0.4及以前 | 0 dBm (1mW) | 不稳定 (10+~105波动) |
| v1.0.5~v1.0.7 | 4 dBm (2.5mW) | 不稳定 (PWM干扰) |
| v1.0.8 | 4 dBm (2.5mW) + 移除PWM | 待验证 |

### Changed
- `zcl_samplesw.c`: 移除PWM相关代码，保持 `ZMacSetTransmitPower(TX_PWR_PLUS_4)`
- `zcl_samplesw.h`: 移除 `SAMPLESW_LED_PWM_EVT` 和 `SAMPLESW_LED_PWM_PERIOD_MS` 定义
- `zcl_samplesw_data.c`: SwBuildId v1.0.7 → v1.0.8, HW_VERSION 0 → 2

### 验证要点
- LED亮度为100%（比v1.0.7亮，无PWM调光）
- 信号稳定性应优于v1.0.7（无PWM定时器干扰）
- 入网速度应正常（无PWM干扰BDB commissioning）
- z2m中linkquality应稳定，不再大幅波动

---

## v1.0.7 - 2026-07-28

未入网时禁用LED PWM调光，避免PWM定时器干扰BDB commissioning入网；HW_VERSION升级到2。

### 问题背景
用户反馈v1.0.6引入LED软件PWM调光后，可能影响设备入网（虽然文档中提到PWM定时器会干扰协议栈MAC时序，但未100%确认根因）。为安全起见，未入网时不启用PWM。

### 修复方案
1. **HW_VERSION升级**：0 → 2（区分硬件版本）
2. **PWM启用逻辑优化**：
   - 未入网时：LED直接写GPIO（100%亮度），PWM定时器不运行，零CPU开销
   - 入网成功后（ZDO_STATE_CHANGE=DEV_ROUTER）：调用`zclSampleSw_LedPwmEnable()`启用PWM调光（50%亮度）

### 行为对比
| 状态 | v1.0.6 (修复前) | v1.0.7 (修复后) |
|------|-----------------|-----------------|
| 上电启动 | PWM立即启用，可能干扰入网 | LED直接写GPIO，无PWM干扰 |
| 配网中（BDB commissioning） | PWM运行，5ms周期定时器 | PWM不运行，零CPU开销 |
| 配网慢闪期间 | LED1以50%亮度慢闪 | LED1以100%亮度慢闪 |
| 入网成功（DEV_ROUTER） | PWM已运行 | 启用PWM，LED切换到50%亮度 |
| 正常工作 | LED 50%亮度 | LED 50%亮度 |

### 实现要点
- 新增 `ledPwmEnabled` 标志位（初始FALSE）
- 新增 `zclSampleSw_LedPwmEnable()` 函数：设置标志位为TRUE并启动PWM定时器
- 修改 `zclSampleSw_LedSetTarget()`：未启用PWM时直接调用`zclSampleSw_LedWriteGpio()`（100%亮度）
- 修改 `zclSampleSw_Init()`：移除无条件启动PWM定时器的代码
- 修改 `ZDO_STATE_CHANGE` 处理：收到DEV_ROUTER时调用`zclSampleSw_LedPwmEnable()`

### 边界情况
- **已配网设备上电**：BDB rejoin期间LED为100%亮度，rejoin成功后切换到50%亮度（有视觉过渡，但可接受）
- **配网慢闪期间**：LED1以100%亮度慢闪，比v1.0.6的50%亮度更醒目，提示效果更好
- **S1复位闪烁**：未入网时以100%亮度闪烁，复位后入网成功切换到50%亮度

### Changed
- `zcl_samplesw_data.c`: HW_VERSION 0 → 2, SwBuildId v1.0.6 → v1.0.7, DateCode 20260727 → 20260728
- `zcl_samplesw.c`: 新增 `ledPwmEnabled` 标志位和 `zclSampleSw_LedPwmEnable()` 函数
- `zcl_samplesw.c`: `zclSampleSw_LedSetTarget()` 添加PWM启用分支
- `zcl_samplesw.c`: `zclSampleSw_Init()` 移除无条件启动PWM定时器
- `zcl_samplesw.c`: `ZDO_STATE_CHANGE` 入网成功时调用 `zclSampleSw_LedPwmEnable()`

### 验证要点
- 未入网时LED亮度为100%（比v1.0.6亮），入网后切换到50%
- 配网慢闪期间LED1亮度为100%，更醒目
- 入网速度应恢复到v1.0.5水平（无PWM干扰）
- 入网后LED亮度稳定在50%

---

## v1.0.6 - 2026-07-27

降低LED状态灯亮度至50%，并真实化设备信息（借壳约束下显示真实厂商/型号/硬件版本）。

### 问题背景
1. **LED偏亮**: 实际使用中LED状态灯亮度偏高，刺眼，需降低到50%
2. **设备信息不真实**: 借壳HGZB-4S后，Z2M显示厂商为`TexasInstruments`、硬件版本为`0`，与真实设备不符

### LED亮度方案 (软件PWM)
- LED1~4 在 P0_0~P0_3，CC2530 的 P0_0/P0_1 无硬件PWM能力
- 采用软件PWM: **5ms周期(200Hz)**，50%占空比，人眼完全不可见闪烁
- 引入`ledTargetOn[]`期望状态层，所有原直接`P0_x=val`写入改为`zclSampleSw_LedSetTarget()`调用
- PWM定时器持续运行(200Hz)，配网慢闪/复位闪烁/继电器联动均经PWM层，亮度统一降至50%

### ⚠️ 重要教训: OSAL定时器频率上限
**初始方案用 2ms 周期(500Hz) 软件PWM，导致设备无法入网（z2m 无日志无设备信息）。**

- **根因**: Z-Stack OSAL 是协作式调度，2ms 高频事件流过载 OSAL 队列，干扰 MAC 层时序（CSMA-CA 退避、ACK 等待、rejoin 流程），导致协议栈无法完成入网
- **诊断过程**: 禁用 PWM 定时器后秒入网，确认是 PWM 频率问题
- **频率测试**: 500Hz(2ms) 无法入网 → 200Hz(5ms) 秒入网，确认 5ms 为安全频率上限
- **经验**: Z-Stack OSAL 应用层定时器周期**不建议 < 5ms**，否则会干扰协议栈 MAC 时序。200Hz PWM 人眼完全不可见闪烁，是软件 PWM 的安全频率

### 设备信息真实化
| 属性 | 旧值 | 新值 | 说明 |
|------|------|------|------|
| ManufacturerName | TexasInstruments | Linxee | 真实厂商 (ZCL字符串, 6字符) |
| HWVersion | 0 | 1 | PCB v1.0 (uint8) |
| SwBuildId | v1.0.5 | HA-SPA4C1-V1.0.6 | 型号+版本格式 (ZCL字符串, 16字符) |

### Changed
- `zcl_samplesw.h`: 新增 `SAMPLESW_LED_PWM_EVT`(0x0080) 和 `SAMPLESW_LED_PWM_PERIOD_MS`(5ms) 定义
- `zcl_samplesw.c`: 新增 LED 软件 PWM 层 (`LedWriteGpio`/`LedSetTarget`/`LedPwmApply`)，启动 PWM 定时器
- `zcl_samplesw.c`: `UpdateRelayOutput`/`StartResetBlink`/`ProcessResetBlink`/`StartPairingBlink`/`ProcessPairingBlink` 中直接 `P0_x` 写入改为 `LedSetTarget` 调用
- `zcl_samplesw_data.c`: `SAMPLESW_HWVERSION` 0→1，`zclSampleSw_ManufacturerName` 改为 `Linxee`，`zclSampleSw_SwBuildId` 改为 `HA-SPA4C1-V1.0.6`

### 验证要点
- LED亮度明显降低，4路均匀，200Hz完全不可见闪烁
- **秒入网**（500Hz 版本会无法入网，200Hz 版本正常）
- 配网慢闪/复位闪烁仍正常工作，亮度50%
- Z2M设备页: 厂商显示 `Linxee`，固件ID显示 `HA-SPA4C1-V1.0.6`，硬件显示 `1`
- Z2M仍识别为 HGZB-4S，4路onOff正常控制
- 触摸响应延迟无明显变化

---

## v1.0.5 - 2026-07-27

设置CC2530发射功率为4dBm，提升信号稳定性。

### 问题背景
用户反馈z2m中信号质量（linkquality）极不稳定，从10+到105大幅波动。

### 根因分析
- HGZBSwitch项目此前未显式设置发射功率，使用MAC PIB默认值 `phyTransmitPower=0`（即0 dBm, 1mW）
- 0 dBm发射功率偏低，加上CC2530裸片（无PA/LNA）天线和环境因素影响，导致接收端信号强度不稳定
- 对比ZNP项目在 [znp_app.c#L411](file:///d:/VC/Z-Stack/Projects/zstack/ZNP/Source/znp_app.c#L411) 中显式设置 `TX_PWR_PLUS_4`（4 dBm），HGZBSwitch缺少此设置

### 修复方案
在 `zclSampleSw_Init()` 中 `bdb_StartCommissioning()` 调用之前，添加 `ZMacSetTransmitPower(TX_PWR_PLUS_4)` 调用，将发射功率设置为4 dBm（约2.5mW）。

### 功率档位选择说明
| 档位 | dBm | 寄存器值 | 测试结果 |
|------|-----|---------|---------|
| TX_PWR_PLUS_7 | 7 | 0xFF | ❌ 信号经常归零，RF工作不稳定 |
| TX_PWR_PLUS_4 | 4 | 0xED | ✅ 与ZNP官方默认一致，稳定性最佳 |
| 默认值 | 0 | 0xB6 | ⚠️ 信号偏低，10+到105波动 |

**为什么不用最大功率 7 dBm：**
- CC2530 datasheet 标称最大 7 dBm，但 7 dBm 档位（寄存器值 0xFF）在某些模块上会导致 RF 工作不稳定
- TI 官方 ZNP 项目默认使用 4 dBm 而非 7 dBm，说明 4 dBm 是更稳妥的选择
- 7 dBm 电流消耗约 34mA（比 0 dBm 多 40%），若模块电源供电余量不足会导致 RF 不稳定
- 4 dBm 相比 0 dBm 信号强度提升约 2.5 倍，同时保持工作稳定性

### Changed
- `zcl_samplesw.c`: `zclSampleSw_Init()` 在 `bdb_StartCommissioning()` 之前新增 `ZMacSetTransmitPower(TX_PWR_PLUS_4)`
- `zcl_samplesw_data.c`: 版本号 v1.0.4 → v1.0.5, DateCode 20260726 → 20260727

### 验证要点
- z2m中linkquality数值应稳定，不再出现10+到105的大幅波动
- 信号不再出现归零现象
- 通信距离应有明显提升

---

## v1.0.4 - 2026-07-26

新增配网中LED1慢闪功能，遵循业界惯例提示用户设备正在配网。

### 功能描述
设备上电启动BDB commissioning后，LED1以1Hz频率慢闪（500ms亮/500ms灭），提示用户正在配网。入网成功或超时（5分钟）后停止慢闪，恢复LED1显示继电器1状态。

### 行为定义
| 阶段 | LED1行为 |
|------|----------|
| 上电启动 | 开始1Hz慢闪 |
| 配网中（未入网） | 持续1Hz慢闪 |
| 入网成功（DEV_ROUTER） | 立即停止慢闪，恢复继电器1状态显示 |
| 配网超时（5分钟无入网） | 停止慢闪，恢复继电器1状态显示 |

### 设计参考
| 厂商 | 配网中LED行为 | 超时 |
|------|-------------|------|
| IKEA Tradfri | 慢闪（1Hz）| 60秒 |
| Aqara | 快闪（2Hz）| 90秒 |
| Tuya | 双闪 | 120秒 |
| **本项目** | **慢闪（1Hz, IKEA风格）** | **5分钟** |

选择1Hz慢闪（IKEA风格）而非快闪，是因为86开关使用场景下慢闪更柔和，且5分钟超时与BDB NWK_STEERING协议栈超时接近。

### 实现要点
- 新增事件 `SAMPLESW_PAIRING_BLINK_EVT` (0x0020)，复用已废弃的UI事件号
- 新增状态机变量：`pairingBlinkActive`/`pairingBlinkLedOn`/`pairingBlinkTickCount`
- 启动时机：`zclSampleSw_Init()` 末尾调用 `zclSampleSw_StartPairingBlink()`
- 停止时机：`ZDO_STATE_CHANGE` 收到 `DEV_ROUTER` 时调用 `zclSampleSw_StopPairingBlink()`
- 防干扰：`zclSampleSw_UpdateRelayOutput(0)` 在配网慢闪激活时跳过LED1刷新，避免防御性刷新干扰闪烁

### Changed
- `zcl_samplesw.h`: 新增 `SAMPLESW_PAIRING_BLINK_EVT` 和 `SAMPLESW_PAIRING_TIMEOUT_MS` 定义
- `zcl_samplesw.c`: 新增 `zclSampleSw_StartPairingBlink`/`ProcessPairingBlink`/`StopPairingBlink` 三个函数
- `zcl_samplesw.c`: `zclSampleSw_Init()` 末尾启动配网慢闪
- `zcl_samplesw.c`: `ZDO_STATE_CHANGE` 处理中入网成功时停止配网慢闪
- `zcl_samplesw.c`: `zclSampleSw_UpdateRelayOutput(0)` 配网慢闪激活时跳过LED1刷新
- `zcl_samplesw_data.c`: 版本号 v1.0.3 → v1.0.4

---

## v1.0.3 - 2026-07-26

修复端点冲突导致 z2m interview 失败和 linkquality 日志风暴（BUG-013）。

### 根因
`SAMPLESW_ENDPOINT` 原值为 8，与 `SAMPLESW_ENDPOINT_INPUT4` (=8) 冲突。init 时 EP8 被注册两次：
1. 先注册为 switch（含 genBasic/genIdentify/genOnOffSwitchConfig cluster）
2. 后注册为 input4（genAnalogInput cluster），覆盖了 switch 的 SimpleDescriptor

z2m interview 时通过 SimpleDescriptor 查找 genBasic server cluster，EP8 已被覆盖为 input4，找不到 genBasic → interview 失败 → 反复重试 → 每秒 5-10 次 linkquality 更新风暴。

### 修复
将 `SAMPLESW_ENDPOINT` 从 8 改为 11，避开 EP1-8（继电器 EP1-4 + 输入状态 EP5-8）。

### Changed
- `SAMPLESW_ENDPOINT`: 8 → 11（避免与 `SAMPLESW_ENDPOINT_INPUT4` 冲突）

### 影响
- z2m 能在 EP11 上读取 genBasic 属性（ModelId/ManufacturerName/DateCode/SwBuildId 等），完成 interview
- EP1-4 继电器端点（genOnOff）和 EP5-8 输入状态端点（genAnalogInput）不受影响
- EP11 不在借壳 HGZB-4S 的 endpoints 映射（l1:1, l2:2, l3:3, l4:4）中，z2m 仅在 EP11 上读取 genBasic，不影响继电器控制

### 历史背景
此问题自 v0.2.x 起就存在。之前 z2m 偶尔能 interview 成功（时序相关），成功后风暴停止；失败时持续风暴。v1.0.3 通过修复端点冲突确保 interview 始终成功。

---

## v1.0.2 - 2026-07-26

优化触摸响应速度，将触摸轮询周期从 100ms 缩短到 50ms，触发延迟从 ~200ms 降到 ~100ms（接近 WTC6106BSI 触摸芯片 50-150ms 硬件响应极限），改善用户触摸体验。

### Changed
- `TOUCH_POLL_INTERVAL_MS`: 100ms → 50ms (触摸轮询周期)
- `S1_RESET_THRESHOLD`: 50 → 100 (同步调整以保持 5 秒长按复位触发时间不变)
- 同步更新相关注释中的周期说明 (100ms → 50ms)

### 性能数据
| 项 | v1.0.1 | v1.0.2 | 改善 |
|----|--------|--------|------|
| 触摸触发延迟 | ~200ms | ~100ms | ↓ 50% |
| 防抖确认时间 | 200ms (2×100ms) | 100ms (2×50ms) | ↓ 50% |
| S1 长按触发时间 | 5 秒 (50×100ms) | 5 秒 (100×50ms) | 不变 |
| LED 防御性刷新周期 | 100ms | 50ms | ↑ 2x (更稳定) |
| CPU 占用增量 | - | +<1% | 可忽略 |

### 设计决策
- **为何选择 50ms 而非更短**: WTC6106BSI 内置 16bit CDC + RISC 处理器，触摸检测本身需要 50-150ms 才能输出有效低电平。50ms 轮询已接近硬件响应极限，更短周期无显著收益。
- **为何保持 2 次防抖**: 抗干扰能力不变（仍需 2 次连续确认），避免误触发。1 次防抖虽然延迟降到 50ms，但抗干扰能力下降。
- **为何不采用中断方案**: 中断方案虽然表面数据优秀（<10ms 响应），但实际仍受 WTC6106BSI 硬件延迟瓶颈限制（实际改善仅 50-100ms），且需要重构整个触摸模块、引入中断相关风险，性价比低。
- **CPU 开销可忽略**: 4 路 GPIO 读取 + 简单位运算，每次 < 10us，50ms 周期下 CPU 占用 < 0.02%。

### Migration
- 烧录 v1.0.2 后触摸响应明显改善，无需"稍作停留"即可触发
- S1 长按 5 秒复位功能时间不变
- 其他功能与 v1.0.1 完全一致

---

## v1.0.1 - 2026-07-26

修复 v1.0.0 重构导致的设备无法入网问题。根因是移除 UI 模块时未补充 BDB commissioning 启动调用，导致 Zigbee 协议栈永不启动网络加入流程。

### Fixed
- BUG-012: 设备上电后无法入网，z2m 无任何日志。根因是 v1.0.0 重构时移除了 `UI_Init()` 调用，而 `UI_Init()` 内部第 1846 行隐式调用了 `bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP)` 启动 Zigbee commissioning。移除 UI 模块后没有补回这个调用，导致应用层正常运行（触摸/继电器工作正常）但 Zigbee 协议栈永不启动。修复：在 `zclSampleSw_Init()` 末尾显式调用 `bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP)`。

### Changed
- `zcl_samplesw.c`: 在 `zclSampleSw_Init()` 函数末尾补充 `bdb_StartCommissioning()` 调用
- 版本号递增: v1.0.0 → v1.0.1 (BUG修复, 修订号递增)

### 设计决策
- **为何使用 `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP` (0x00) 而非 `BDB_COMMISSIONING_MODE_NWK_STEERING`**: 与原 `UI_Init()` 行为保持一致。参数 0x00 让 BDB 自动判断：已配网设备尝试 rejoin 恢复网络，新设备触发 initialization 后启动 NWK_STEERING commissioning。
- **为何不在 OSAL 启动后立即调用**: 必须在 `bdb_Init()` 完成后调用，`zclSampleSw_Init()` 是应用层最后一个初始化的任务，此时 bdb 已完成初始化，调用安全。

### Migration
- 烧录 v1.0.1 后设备能正常入网
- 其他功能与 v1.0.0 设计一致（HAL_KEY 已禁用，无引脚冲突干扰）

---

## v1.0.0 - 2026-07-26

完全重构固件工程，从 Z-Stack 3.0.2 官方 SampleSwitch 示例中剥离残留代码，创建独立的 HGZBSwitch 工程。根除 hal_key.c 的 P2.0(继电器4)/P0.6(触摸输入3) 引脚冲突干扰，移除 UI/LCD/MT/Touchlink/GP 等无用模块。

### Changed - 架构重构
- **新建独立工程**: `Projects/zstack/HomeAutomation/HGZBSwitch/`，与原 SampleSwitch 工程分离
- **新工程文件**: `HGZBSwitch.ewp` / `HGZBSwitch.eww`，只保留 RouterEB 配置（移除 CoordinatorEB/EndDeviceEB/OTAClient）
- **版本号升至 v1.0.0**: 主版本号变更表示重大架构重构

### Removed - 移除的模块（共28个.c文件）
- **UI 模块**: 移除 zcl_sampleapps_ui.c 及所有 UI_Init/UI_UpdateLcd/UI_MainStateMachine/UI_DeviceStateUpdated/UI_UpdateComissioningStatus 调用
- **MT 模块**: 移除 15个 MT_*.c 文件（DebugTrace/MT/MT_AF/MT_APP/MT_APP_CONFIG/MT_DEBUG/MT_GP/MT_NWK/MT_SAPI/MT_SYS/MT_TASK/MT_UART/MT_UTIL/MT_VERSION/MT_ZDO）
- **Touchlink 模块**: 移除 bdb_touchlink.c/bdb_touchlink_initiator.c/bdb_touchlink_target.c/bdb_tlCommissioning.c
- **Green Power 模块**: 移除 gp_common.c/gp_proxyTbl.c/zcl_green_power.c（定义 DISABLE_GREENPOWER_BASIC_PROXY）
- **HA Profile**: 移除 zcl_ha.c（仅保留 zcl_ha.h 头文件）
- **HAL_UART**: 移除 hal_uart.c（HAL_UART 已为 FALSE）

### Fixed - 通过宏禁用的模块
- **HAL_KEY=FALSE**: 禁用按键模块，根除 P2.0(继电器4)/P0.6(触摸输入3) 引脚冲突。hal_key.c 保留编译（提供空实现），因 OnBoard.c 直接调用 HalKeyConfig
- **HAL_LCD=FALSE + 移除 LCD_SUPPORTED=DEBUG**: 禁用 LCD 模块。hal_lcd.c 已从工程移除
- **HAL_ADC=FALSE**: 禁用 ADC 读取功能。hal_adc.c 保留编译，因 ZMain.c 调用 HalAdcCheckVdd 检查电压
- **HAL_KEY=FALSE 时 hal_key.c 编译为空实现**: HalKeyInit/HalKeyConfig/HalKeyPoll 均为空函数，不再干扰 GPIO

### Removed - 事件定义
- 移除 SAMPLEAPP_LCD_AUTO_UPDATE_EVT (0x0010) 事件定义
- 移除 SAMPLEAPP_KEY_AUTO_REPEAT_EVT (0x0020) 事件定义

### 设计决策
1. **为何保留 hal_key.c/hal_adc.c/hal_sleep.c**: 协议栈核心文件（OnBoard.c/ZMain.c/mac_mcu.c）直接调用 HalKeyConfig/HalAdcCheckVdd/halSetMaxSleepLoopTime，无条件编译保护。保留这些文件但通过宏禁用功能，函数编译为空实现或仅保留电压检查功能
2. **为何移除 UI 模块**: 设备无 LCD、无物理按键，UI 模块的 UI_Init/UI_UpdateLcd/UI_MainStateMachine 调用完全无用。移除后减少 Flash 占用和编译时间
3. **为何移除 GP 模块**: Green Power Basic Proxy 是 HA 1.2 Router 可选特性，本项目不需要。移除后释放 Flash 和 NV 空间
4. **为何保留 hal_led.c**: 继电器控制通过 LED 引脚（P0_0~P0_3），hal_led.c 提供必要的 GPIO 操作宏

### Migration
- 烧录 v1.0.0 后，设备功能与 v0.2.4 完全一致
- LED1 待机稳定性应更好（hal_key.c 已禁用，不再干扰 GPIO）
- 编译时间减少约 30%（移除 28 个 .c 文件）
- Flash 占用减少（移除 UI/LCD/MT/Touchlink/GP 模块代码）

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
