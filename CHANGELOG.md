# 更新日志

## v0.1.0 (2026-07-31)

### 新增
- 基于Z-Stack 3.0.2 SampleSwitch示例创建全新HGZBSwitch工程
- 4路继电器控制 (EP1-4, genOnOff server)
- 4路触摸检测 (WTC6106BSI, 50ms轮询, 2次防抖)
- S1长按5秒工厂复位 (LED闪烁3次提示)
- 断电恢复 (4路独立startUpOnOff, NV延迟写入+对比写入)
- BDB入网 (NV状态选择rejoin/nwk_steering, TX功率4dBm)
- 借壳HGZB-4S (ModelId=LXN-4S27LX1.0, 免External Converter)
- LED控制 (直接GPIO, 配网1Hz慢闪, 复位3次闪烁)
- ZCL上报 (触摸后上报, 入网后全量上报, 不周期性上报)
- 4路输入状态端点 (EP5-8, genAnalogInput server)

### 移除
- MT/GP/Touchlink/OTA (条件编译保留, 不启用)
- LED软件PWM (干扰MAC时序)
- LED防御性刷新 (画蛇添足)
- 30秒周期性上报 (触发z2m无意义action事件)
