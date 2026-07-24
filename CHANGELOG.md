# Changelog - 4路智能开关 Zigbee 固件

本文件记录固件版本变更历史。版本号规则：`v主.次.修订`，主版本号变更表示重大功能里程碑。

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
