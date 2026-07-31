# Z-Stack 限制

> 记录 Z-Stack 3.0.2 协议栈的硬限制（不可绕过的约束），开发时必须遵守，避免重复踩坑。
>
> **原则**：这些限制是协议栈和硬件的固有线约束，不能通过软件绕过，只能规避。

## 1. OSAL 定时器周期不建议 < 5ms

| 项 | 内容 |
|----|------|
| **限制** | Z-Stack OSAL 应用层定时器周期不建议 < 5ms |
| **原因** | Z-Stack OSAL 是协作式调度，< 5ms 的高频事件流过载 OSAL 队列，干扰 MAC 层时序（CSMA-CA 退避、ACK 等待、rejoin 流程） |
| **后果** | 协议栈无法完成入网，z2m 无日志无设备信息 |
| **验证** | v1.0.6 测试 500Hz(2ms) 无法入网 → 200Hz(5ms) 秒入网，确认 5ms 为安全频率上限 |
| **规避** | 应用层定时器周期 ≥ 5ms；LED 调光使用硬件 PWM 或外部电路，不用软件 PWM |

## 2. CC2530 堆大小仅 3072 字节

| 项 | 内容 |
|----|------|
| **限制** | CC2530 Router 堆大小仅 3072 字节 |
| **定义位置** | `Projects/zstack/ZMain/TI2530DB/OnBoard.h:216` |
| **后果** | ZCL 消息处理需注意堆内存，连续操作多端点会堆耗尽导致 OSAL 调度器卡死 |
| **验证** | v1.0.9 修复连续 z2m 操作 3 轮后设备卡死问题 |
| **规避** | 避免在 ZCL 命令处理后主动 Report（Default Response 已足够）；触摸操作仍可主动上报 |

**堆占用估算**：
- 每次 ZCL OnOff 命令产生 2 条 AF 消息（Report + Default Response）≈ 100 字节
- 3 轮×4 端点×2 消息 = 24 条 AF 消息 ≈ 1200 字节
- 加上协议栈自身使用，总计接近 3072 字节上限

## 3. HAL_KEY 引脚冲突

| 项 | 内容 |
|----|------|
| **限制** | `hal_key.c` 会干扰 P2.0(继电器4) 和 P0.6(触摸输入3) |
| **原因** | `hal_key.c` 将 P2.0 定义为摇杆移动输入，`HalKeyPoll()` 每 100ms 读取 P2.0 状态；当继电器4 OFF (P2.0=1) 时，触发 `halGetJoyKeyInput()` 调用 `HalAdcRead(HAL_KEY_JOY_CHN, ...)` 读取 P0.6 的 ADC 值，临时修改 `ADCCFG` 寄存器，通过电气干扰影响 GPIO 状态 |
| **后果** | LED1 自动熄灭、GPIO 状态异常 |
| **规避** | 必须设置 `HAL_KEY=FALSE`，禁用按键模块 |

## 4. HAL_KEY=FALSE 后仍需保留 hal_key.c 编译

| 项 | 内容 |
|----|------|
| **限制** | `HAL_KEY=FALSE` 后仍需保留 `hal_key.c` 编译 |
| **原因** | `OnBoard.c` 直接调用 `HalKeyConfig`，无条件编译保护 |
| **后果** | 移除 `hal_key.c` 会导致链接错误 |
| **规避** | 保留 `hal_key.c` 编译但通过 `HAL_KEY=FALSE` 禁用功能，`HalKeyInit` / `HalKeyConfig` / `HalKeyPoll` 编译为空函数 |

> 同样的限制适用于 `hal_adc.c`（`ZMain.c` 调用 `HalAdcCheckVdd`）和 `hal_sleep.c`（`mac_mcu.c` 调用 `halSetMaxSleepLoopTime`）。

## 5. bdb_StartCommissioning(0x00) 无效

| 项 | 内容 |
|----|------|
| **限制** | `bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP)` 对新设备无效 |
| **原因** | `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP = 0x00`（见 `bdb.h:175`），不设置任何 commissioning mode 位 |
| **后果** | BDB 检查 `bdbCommissioningMode == 0` 时直接 report INITIALIZATION 失败并 return，不会调用 `ZDO_InitDevice()`，不会触发 NWK_STEERING，设备无法发现网络 |
| **验证** | v1.0.10 修复 S1 软复位后无法入网问题 |
| **规避** | 读取 NV `ZCD_NV_BDBNODEISONANETWORK` 状态选择 commissioning 模式：TRUE 用 `BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP`，FALSE 用 `BDB_COMMISSIONING_MODE_NWK_STEERING` |

## 6. ZCL 字符串格式

| 项 | 内容 |
|----|------|
| **限制** | ZCL 字符串第一字节为长度前缀，不含 `\0` 终止符 |
| **原因** | ZCL 规范定义 |
| **后果** | 长度与字符数不符会导致 z2m 解析异常；用空格填充会导致显示带尾部空格 |
| **规避** | 长度前缀必须等于实际字符数，不填充不补零 |

```c
// 正确: 长度=8, 8 个字符
const uint8 zclSampleSw_DateCode[] = { 8, '2','0','2','6','0','7','2','8' };

// 错误: 长度与字符数不符
const uint8 zclSampleSw_ManufacturerName[] = { 8, 'L','i','n','x','e','e' };  // 长度 8 但只有 6 字符
```

> Z-Stack SampleSwitch 原始代码用空格填充到 16 字节，导致 z2m 显示 `20260728        `（带尾部空格）。本项目已修正为精确长度。

## 7. NV ID 选型

| 项 | 内容 |
|----|------|
| **限制** | NV ID 必须避开系统区和 ZNP 保留区 |
| **系统区** | `0x0001 ~ 0x0097`（Z-Stack 系统使用） |
| **ZNP 保留区** | `0x0F01 ~ 0x0F07` |
| **规避** | 使用 `0x0F10 ~ 0x0F1F` 等空闲区间 |

本项目使用的 NV ID：
- `0x0F10`：继电器状态持久化
- `0x0F12`：startUpOnOff 配置（4 路独立，避开旧的 0x0F11 单字节不兼容数据）

## 8. SwBuildId 长度限制

| 项 | 内容 |
|----|------|
| **限制** | SwBuildId 字符串内容最多 16 字符（不含长度前缀字节） |
| **原因** | ZCL 属性上报受 MTU 限制，超长导致 MTU 超限 |
| **后果** | z2m 读取固件 ID 时丢失，固件 ID 字段为空 |
| **验证** | v1.0.10 的 `HA-SPA4C1-V1.0.10`（17 字符）导致固件 ID 缺失 |
| **规避** | 字符串内容 ≤ 16 字符；版本号位数增加时缩短型号前缀（如 `SPA4C1-V1.0.10`） |

```c
// 正确: 16 个字符（HA-SPA4C1-V1.0.9）
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','V','1','.','0','.','9' };
```

## 9. DateCode 必须修改

| 项 | 内容 |
|----|------|
| **限制** | DateCode 必须改为实际编译日期，不能沿用 Z-Stack 默认值 |
| **默认值** | Z-Stack SampleSwitch 默认 `20060831` |
| **后果** | 无法区分固件版本时间，z2m 显示错误的编译日期 |
| **规避** | 每次发版时更新为实际编译日期 `YYYYMMDD` |

## 10. hal_led.c 的 HalLedState

| 项 | 内容 |
|----|------|
| **限制** | `HalLedBlink` 会修改 `HalLedState` 全局变量 |
| **原因** | `hal_led.c` 内部维护 `HalLedState` 跟踪 LED 状态，`HalLedBlink` 会修改它 |
| **后果** | 与应用层直接 GPIO 操作冲突，导致状态不一致（如 LED1 自动熄灭、闪烁频率异常） |
| **验证** | v0.2.4 自定义复位闪烁状态机后，闪烁清晰可见 |
| **规避** | 应用层直接操作 GPIO 时，不能使用 `HalLedBlink`，需自定义状态机直接操作 GPIO 绕过 HalLed 层 |

**冲突场景**：
- 应用层 `zclSampleSw_UpdateRelayOutput()` 直接操作 P0_0 控制 LED1，不更新 `HalLedState`
- `HalLedBlink` 读取/修改 `HalLedState`，与应用层实际硬件状态不一致
- 后续 `HalLedUpdate()` 调用时根据 `HalLedState` 错误地修改 GPIO，导致 LED 状态异常
