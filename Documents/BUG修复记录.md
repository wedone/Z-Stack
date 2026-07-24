# BUG 修复记录

记录项目开发过程中遇到的BUG及其修复方案，便于新AI快速了解历史问题。

---

## BUG-001: 触摸输入逐渐失效

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 前 (dev 分支) |
| **commit** | `23c47cc` |
| **严重度** | 高 - 设备几乎不可用 |

### 现象

1. 入网初期触摸能操作几次（LED状态正确），后面完全无反应
2. 重启设备后重复上述现象
3. z2m 页面远程控制正常，仅触摸失效

### 根因

`zclSampleSw_ReadTouchInputs()` 使用**自适应基线采样**逻辑：上电时读取 P0_4~P0_7 初始电平作为 `touchBaseline`，之后以"偏离基准"判断触摸。

问题在于 WTC6106BSI 输出电平在运行过程中会漂移，导致基线被逐步"学习"到触摸电平，最终触摸检测完全失效。

### 修复方案

移除自适应基线逻辑，改为**直接低电平检测**：

```c
// 修复前: 与上电基准比较
if ((port & BV(4)) != (touchBaseline & BV(4))) val |= BV(0);

// 修复后: 直接低电平=触摸
if (!(port & BV(4))) val |= BV(0);
```

WTC6106BSI 输出极性固定（未触摸=高，触摸=低），无需自适应。

### 涉及文件

- `zcl_samplesw.c`: `zclSampleSw_ReadTouchInputs()` 函数, 移除 `touchBaseline` 变量

---

## BUG-002: z2m 收不到触摸触发的状态变化

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 前 (dev 分支) |
| **commit** | `23c47cc` |
| **严重度** | 高 - 触摸操作与z2m不同步 |

### 现象

1. 触摸按键后继电器动作、LED正确变化
2. z2m 页面状态不更新，仍显示旧状态
3. 远程控制（z2m→设备）正常，仅本地操作不上报

### 根因

固件在触摸触发继电器翻转后，未通过 ZCL 主动上报 OnOff 属性变化。Z-Stack 默认不自动上报，需应用层调用 `zcl_SendReportCmd()`。

### 修复方案

1. 新增 `zclSampleSw_ReportOnOffState()` 函数，向协调器(短地址0)上报 OnOff 属性
2. 在 `zclSampleSw_ToggleRelay()` 和 `zclSampleSw_HandleOnOffCmd()` 中调用
3. IAR 工程添加 `ZCL_REPORTING_DEVICE` 宏定义以启用 ZCL 上报功能

### 涉及文件

- `zcl_samplesw.c`: 新增 `zclSampleSw_ReportOnOffState()` 函数
- `SampleSwitch.ewp`: 添加 `ZCL_REPORTING_DEVICE` 预定义宏

---

## BUG-003: 入网后操作延迟波动大

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 前 (dev 分支) |
| **commit** | `23c47cc` (部分缓解) |
| **严重度** | 中 |
| **状态** | 待验证 |

### 现象

刚入网时 z2m 操作正常，一段时间后操作反应延迟波动非常大，有时 timeout。

### 根因分析

可能与 ZCL 上报缺失有关（BUG-002），设备状态不一致导致 z2m 反复重试。也可能与 Router 的 poll rate 或网络环境有关。

### 修复方案

修复 BUG-002 后观察是否缓解。如仍有问题，需检查：
- Router poll rate 配置
- 网络信号强度
- 是否存在网络拥塞

---

## BUG-004: 编译错误 - zcl_SendReportCmd 隐式声明

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 编译阶段 |
| **commit** | `23c47cc` |
| **严重度** | 低 - 编译阻塞 |

### 现象

添加 ZCL 上报代码后编译报错：`Function "zcl_SendReportCmd" declared implicitly`

### 根因

`zcl_SendReportCmd()` 函数受 `ZCL_REPORTING_DEVICE` 宏控制，未定义该宏时函数声明被条件编译排除。

### 修复方案

在 IAR 工程配置 `SampleSwitch.ewp` 的 RouterEB 配置中添加 `ZCL_REPORTING_DEVICE` 预定义宏。

### 涉及文件

- `SampleSwitch.ewp`: RouterEB configuration 添加 `<state>ZCL_REPORTING_DEVICE</state>`

---

## BUG-005: DateCode 仍为框架默认值

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 |
| **commit** | `2c2e55a` |
| **严重度** | 低 - 版本标识不准确 |

### 现象

固件 DateCode 属性仍为 DIYRuZ_RT 框架默认值 `20060831`，与实际编译日期不符，无法区分固件版本时间。

### 根因

DIYRuZ_RT 原始代码中 `zclSampleSw_DateCode` 硬编码为 `20060831`，移植时未同步修改。

### 修复方案

将 DateCode 改为实际编译日期 `20260724`，并建立版本更新规则（见 [固件版本更新规则.md](固件版本更新规则.md)），每次发版必须同步更新。

### 涉及文件

- `zcl_samplesw_data.c`: `zclSampleSw_DateCode` 从 `20060831` 改为 `20260724`

---

## BUG-006: 触摸物理按键无任何反应

| 项 | 内容 |
|----|------|
| **日期** | 2026-07-24 |
| **版本** | v0.1.0 |
| **commit** | 待提交 |
| **严重度** | 高 - 触摸功能完全不可用 |

### 现象

1. 烧录v0.1.0固件后，触摸4路按键无任何反应
2. LED灯无变化，z2m页面也无任何日志
3. z2m远程控制正常，仅本地触摸失效

### 根因

此前BUG-001修复（commit `23c47cc`）移除了自适应基线采样改为直接低电平检测，但实际硬件仍无反应。需要确认WTC6106BSI触摸芯片的实际输出极性是否与代码假设一致。

### 诊断方法

采用**4通道分方案诊断固件**，一次烧录验证所有假设：

| 通道 | 引脚 | 方案 | 验证目标 |
|------|------|------|----------|
| CH0 | P0_4 | 方案A: 低电平=触摸，上升沿触发 | 当前逻辑 |
| CH1 | P0_5 | 方案B: 高电平=触摸，上升沿触发 | 极性相反 |
| CH2 | P0_6 | 方案C: 任意电平变化都触发 | 不依赖极性 |
| CH3 | P0_7 | 方案D: P0_7电平直接映射LED4 | 引脚是否可读 |

### 诊断结果

- **方案A(CH0)和方案B(CH1)都正确**：LED亮灭和z2m状态都对得上
  - 原因：WTC6106BSI实际输出为触摸=低电平，方案A在按下瞬间触发，方案B在松开瞬间触发，两者都翻转一次
  - 方案A响应更及时（按下即触发），方案B有延迟感（松开才触发）
- **方案C(CH2)不正确**：一次完整触摸(按下+松开)翻转两次，状态变回原点，看似无反应
- **方案D(CH3)不正确**：P0_3(LED4)被UpdateRelayOutput覆盖

### 修复方案

确认方案A（低电平=触摸，上升沿触发）为正确方案，恢复4通道统一使用方案A：

```c
// ReadTouchInputs: 4通道统一低电平=触摸
if (!(port & BV(4))) val |= BV(0);  // P0_4
if (!(port & BV(5))) val |= BV(1);  // P0_5
if (!(port & BV(6))) val |= BV(2);  // P0_6
if (!(port & BV(7))) val |= BV(3);  // P0_7

// ProcessTouchPoll: 上升沿(未触摸→触摸)触发翻转
if (curBit) {
  touchStableState |= BV(i);
  zclSampleSw_ToggleRelay(i);
}
```

移除诊断代码中的通道差异化逻辑，恢复为4通道统一处理。

### 涉及文件

- `zcl_samplesw.c`: 恢复`ReadTouchInputs()`、`ProcessTouchPoll()`、`InitGpio()`为方案A正式版
