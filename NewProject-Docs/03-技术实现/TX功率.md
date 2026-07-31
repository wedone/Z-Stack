# TX 功率

## 1. 概述

CC2530 SoC 内置 RF 发射功率可配置，通过 `ZMacSetTransmitPower` 设置。86四路智能开关最终选择 **4 dBm**（`TX_PWR_PLUS_4`），与 TI ZNP 官方默认一致。该设置必须在 `bdb_StartCommissioning()` 之前调用。

## 2. 功率档位对比

| 功率档位 | 宏 | 测试结果 | 备注 |
|---------|-----|---------|------|
| 0 dBm | `TX_PWR_0`（默认） | 信号偏低，不稳定 | Z-Stack 默认值 |
| 4 dBm | `TX_PWR_PLUS_4` | **稳定，推荐** | 与 TI ZNP 官方默认一致 |
| 7 dBm | `TX_PWR_PLUS_7` | RF 不稳定，信号归零 | 不推荐 |

## 3. 历史演进

源码注释中记录了 TX 功率调试的完整历程（zcl_samplesw.c）：

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
```

### 3.1 调试过程

| 版本 | TX 功率 | 其他变更 | 测试结果 |
|------|---------|---------|---------|
| v1.0.5 | 0 dBm → 4 dBm | - | 信号不稳定，LQI 10~105 波动 |
| v1.0.6 | 4 dBm | 引入 PWM 调光 | 信号仍不稳定（误判为功率问题） |
| v1.0.7 | 4 dBm | 未入网时禁用 PWM | 仍不稳定（PWM 是根因，非功率） |
| v1.0.8 | 4 dBm | **完全移除 PWM** | 信号稳定，问题解决 |

### 3.2 关键结论

- **7 dBm 导致 RF 不稳定**：信号归零，不可用
- **5 dBm 无改善**：与 4 dBm 相比无明显差异
- **4 dBm 是最佳配置**：与 TI ZNP 官方默认一致，稳定可靠
- **信号不稳定的真正根因是软件 PWM**：5ms 周期 PWM 定时器干扰协议栈 MAC 时序，详见《LED控制.md》

## 4. 代码示例

### 4.1 设置调用

`zclSampleSw_Init()` 中调用：

```c
void zclSampleSw_Init( byte task_id )
{
  // ... 端点注册、属性注册、NV 初始化、GPIO 初始化等 ...

  // v1.0.8: 保持发射功率 4 dBm (TX_PWR_PLUS_4), 与ZNP官方默认一致
  // 必须在 bdb_StartCommissioning() 之前调用, 确保入网时即使用设置后的发射功率
  ZMacSetTransmitPower(TX_PWR_PLUS_4);

  // v1.0.10修复: commissioning mode 选择
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

  zclSampleSw_StartPairingBlink();
}
```

## 5. 调用顺序约束

**必须在 `bdb_StartCommissioning()` 之前调用**：

```c
ZMacSetTransmitPower(TX_PWR_PLUS_4);   // ← 先设置功率
// ... 然后启动 commissioning
bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
```

### 5.1 原因

- 入网过程（NWK_STEERING / REJOIN）会立即开始 RF 通信
- 若功率设置在 commissioning 之后，入网过程使用默认 0 dBm
- 默认 0 dBm 信号偏低，可能导致入网失败或入网后 LQI 不稳定

## 6. 与 TI ZNP 官方默认一致

TI 官方 ZNP（ZigBee Network Processor）示例固件默认使用 4 dBm 发射功率。本方案保持一致，确保：
- 与 TI 官方测试环境一致，便于对比验证
- 与协调器（通常也是 4 dBm）功率匹配，链路对称
- 经过 TI 大规模量产验证，稳定可靠

## 7. 经验教训

### 7.1 不要盲目提高功率

- 7 dBm 反而导致信号归零
- CC2530 内置 PA 在高功率档位下线性度差，可能产生谐波干扰
- **正确做法**：使用与官方默认一致的 4 dBm

### 7.2 信号不稳定时优先排查干扰源

- v1.0.5~v1.0.7 误判为功率问题，反复调整 TX 功率无效
- 真正根因是软件 PWM 干扰协议栈时序
- **正确做法**：信号不稳定时，先排查是否有定时器/GPIO 操作干扰 MAC 时序

### 7.3 设置时机很重要

- TX 功率设置必须在 commissioning 之前
- 否则入网过程使用默认 0 dBm，可能入网失败

## 8. 相关文件

- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.c`
  - `zclSampleSw_Init()` 中 `ZMacSetTransmitPower(TX_PWR_PLUS_4)` 调用
- 相关头文件
  - `ZMacSetTransmitPower` 函数声明（ZMac API）
  - `TX_PWR_PLUS_4` 宏定义（ZMac API）

## 9. 关联文档

- 《LED控制.md》——软件 PWM 是信号不稳定的根因
- 《BDB入网.md》——TX 功率设置在 commissioning 之前
