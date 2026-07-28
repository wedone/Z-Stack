---
name: "firmware-build"
description: "编译 HGZBSwitch 固件 (IAR for 8051)。当用户要求'编译''build''构建'固件时调用。执行 IAR 命令行编译 RouterEB 配置，检查 .hex 产物，报告成功/失败。"
---

# 编译 HGZBSwitch 固件

## 用途

编译 4 路智能开关 Zigbee 固件 (CC2530F256, Z-Stack 3.0.2, RouterEB 配置)。

## 触发词

编译、build、构建、compile、IAR

## 环境

- **IAR 路径**: `C:\Program Files (x86)\IAR Systems\Embedded Workbench\common\bin\IarBuild.exe`
- **工程文件**: `d:\vc\Z-Stack\Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\HGZBSwitch.ewp`
- **编译配置**: `RouterEB` (唯一使用, 非 OTA)
- **产物路径**: `Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\RouterEB\Exe\`
  - `HGZBSwitch.hex` — Intel HEX 格式 (IAR 输出)
  - `HGZBSwitch.bin` — BIN 格式 (由 burn.py 或 hex2bin.py 转换, 烧录用)

## 执行步骤

1. 运行 IAR 命令行编译:

```powershell
$iar = "C:\Program Files (x86)\IAR Systems\Embedded Workbench\common\bin\IarBuild.exe"
& $iar "d:\vc\Z-Stack\Projects\zstack\HomeAutomation\HGZBSwitch\CC2530DB\HGZBSwitch.ewp" -build "RouterEB"
```

2. **检查退出码**:
   - 退出码 0 = 编译成功
   - 非零 = 编译失败, 输出错误信息供用户排查

3. **验证产物**: 确认 `HGZBSwitch.hex` 文件存在且非空

4. **报告结果**:
   - 成功: 简短确认 + 产物路径
   - 失败: 错误摘要 + 可能修复方向

## 常见编译错误

| 错误 | 原因 | 修复 |
|------|------|------|
| `Function "zcl_SendReportCmd" declared implicitly` | 缺 `ZCL_REPORTING_DEVICE` 宏 | 在 `.ewp` RouterEB 的 `CCDefines` 添加 `<state>ZCL_REPORTING_DEVICE</state>` |
| 编译成功但无 .hex 输出 | ExtraOutputFormat 配置错误 | `.ewp` 中 `ExtraOutputFile=HGZBSwitch.hex`, `ExtraOutputFormat=23` (intel-extended) |

## 关键预定义宏 (RouterEB)

| 宏 | 说明 |
|----|------|
| `ZCL_REPORTING_DEVICE` | 启用 ZCL 属性上报 |
| `ZG_BUILD_RTR_TYPE` | 设备类型 Router |
| `CC2530` | 目标芯片 |
| `HAL_KEY=FALSE` | 禁用 HAL_KEY 模块 (v1.0.0 起避免 P2.0/P0_6 引脚冲突) |

## 后续

编译成功后, 用户通常要求"烧录" → 触发 `firmware-burn` skill。
