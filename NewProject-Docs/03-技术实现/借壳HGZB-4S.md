# 借壳 HGZB-4S

## 1. 概述

为使 86四路智能开关能被 z2m（Zigbee2MQTT）自动识别并集成，采用"借壳"策略：将设备的 `ModelId` 修改为 z2m 内置支持的 `LXN-4S27LX1.0`，匹配 z2m 的 HGZB-4S 转换规则。这样无需在 z2m 中编写自定义 external converter，开箱即用。

## 2. 借壳原理

### 2.1 z2m 内置 HGZB-4S 转换规则

z2m 内置了 Linxee HGZB-4S（4 路智能开关）的转换规则，关键匹配字段是 ZCL Basic Cluster 的 `ModelId` 属性：

- **ModelId 值**：`LXN-4S27LX1.0`
- **预期端点映射**：l1=EP1, l2=EP2, l3=EP3, l4=EP4
- **预期功能**：4 路独立 OnOff 开关

### 2.2 借壳策略

修改设备的 `ModelId` 为 `LXN-4S27LX1.0`，让 z2m 误以为是 Linxee 官方 HGZB-4S 设备，自动套用其转换规则。本设备的端点布局（EP1~EP4 = 4 路继电器）与 HGZB-4S 完全一致，借壳可行。

## 3. 端点映射

| z2m 实体 | 端点 | ZCL Cluster | 用途 |
|---------|------|-------------|------|
| `l1`（state_l1） | EP1 | genOnOff server | 继电器 1 开关 |
| `l2`（state_l2） | EP2 | genOnOff server | 继电器 2 开关 |
| `l3`（state_l3） | EP3 | genOnOff server | 继电器 3 开关 |
| `l4`（state_l4） | EP4 | genOnOff server | 继电器 4 开关 |
| `in1`（input_state_in1） | EP5 | genAnalogInput server | 触摸输入 1 状态 |
| `in2`（input_state_in2） | EP6 | genAnalogInput server | 触摸输入 2 状态 |
| `in3`（input_state_in3） | EP7 | genAnalogInput server | 触摸输入 3 状态 |
| `in4`（input_state_in4） | EP8 | genAnalogInput server | 触摸输入 4 状态 |

端点 ID 定义（zcl_samplesw.h）：

```c
// 4路继电器端点 (EP 1-4, 对应 alab.switch 的 l1-l4)
#define SAMPLESW_ENDPOINT_RELAY1        1
#define SAMPLESW_ENDPOINT_RELAY2        2
#define SAMPLESW_ENDPOINT_RELAY3        3
#define SAMPLESW_ENDPOINT_RELAY4        4
#define SAMPLESW_NUM_RELAYS             4

// 4路输入状态端点 (EP 5-8, 对应 alab.switch 的 in1-in4)
#define SAMPLESW_ENDPOINT_INPUT1        5
#define SAMPLESW_ENDPOINT_INPUT2        6
#define SAMPLESW_ENDPOINT_INPUT3        7
#define SAMPLESW_ENDPOINT_INPUT4        8
#define SAMPLESW_NUM_INPUTS             4
```

SimpleDescriptor 定义（zcl_samplesw_data.c）：

```c
// 4个端点的 SimpleDescriptor
SimpleDescriptionFormat_t zclSampleSw_RelaySimpleDesc[SAMPLESW_NUM_RELAYS] =
{
  { SAMPLESW_ENDPOINT_RELAY1, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT, SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY2, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT, SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY3, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT, SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY4, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT, SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
};
```

## 4. ZCL 属性定义

### 4.1 可自定义属性

下列属性可按需修改，不影响借壳：

| 属性 | 值 | 说明 |
|------|-----|------|
| `ManufacturerName` | `Linxee` | 厂商名（6 字符） |
| `SwBuildId` | `HA-SPA4C1-1.0.10` | 固件 ID（型号+版本号，15 字符） |
| `HWVersion` | `2` | 硬件版本号 |

属性定义（zcl_samplesw_data.c）：

```c
// v1.0.7: HW_VERSION 从 0 改为 2 (硬件版本号, ZCL ATTRID_BASIC_HW_VERSION)
// 0=初始原型, 1=未使用, 2=当前量产硬件版本
// v1.0.8: HW_VERSION 保持 2 不变
#define SAMPLESW_HWVERSION          2
#define SAMPLESW_ZCLVERSION         0

// Basic Cluster
const uint8 zclSampleSw_HWRevision = SAMPLESW_HWVERSION;
const uint8 zclSampleSw_ZCLVersion = SAMPLESW_ZCLVERSION;
const uint8 zclSampleSw_ManufacturerName[] = { 6, 'L','i','n','x','e','e' };
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };
// DateCode: 固件编译日期 (ZCL DateCode 属性, 格式: YYYYMMDD)
// SwBuildId: 型号+版本号 (ZCL SwBuildId 属性)
//   v1.0.10: 长度从 17 缩为 16 字节 (去掉 V 前缀), 避免 ZCL Read Attrs Rsp 超 MTU 被丢弃
const uint8 zclSampleSw_DateCode[] = { 8, '2','0','2','6','0','7','3','1' };
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','1','.','0','.','1','0' };
const uint8 zclSampleSw_PowerSource = POWER_SOURCE_MAINS_1_PHASE;
```

### 4.2 不可自定义属性

| 属性 | 值 | 说明 |
|------|-----|------|
| `ModelId` | `LXN-4S27LX1.0` | **借壳必需**，修改后无法被 z2m 识别 |
| `DateCode` | `20260731` | 编译日期，自动生成 |

### 4.3 属性注册

属性注册到 EP11（z2m interview 端点）（zcl_samplesw_data.c）：

```c
CONST zclAttrRec_t zclSampleSw_Attrs[] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_ZCL_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_ZCLVersion
    }
  },  
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    {  // Attribute record
      ATTRID_BASIC_HW_VERSION,
      ZCL_DATATYPE_UINT8,
      ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_HWRevision
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MANUFACTURER_NAME,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSampleSw_ManufacturerName
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_MODEL_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSampleSw_ModelId
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_DATE_CODE,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSampleSw_DateCode
    }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { // Attribute record
      ATTRID_BASIC_SW_BUILD_ID,
      ZCL_DATATYPE_CHAR_STR,
      ACCESS_CONTROL_READ,
      (void *)zclSampleSw_SwBuildId
    }
  },
  // ... 其他属性 ...
};
```

## 5. ZCL 字符串格式

### 5.1 格式规范

ZCL 字符串属性采用 **第一字节长度前缀** 格式，**不含 `\0` 终止符**：

| 字段 | 字节 0 | 字节 1~N | 总长度 |
|------|--------|---------|--------|
| `ManufacturerName` | `6`（长度） | `L i n x e e` | 7 字节 |
| `ModelId` | `13`（长度） | `L X N - 4 S 2 7 L X 1 . 0` | 14 字节 |
| `DateCode` | `8`（长度） | `2 0 2 6 0 7 3 1` | 9 字节 |
| `SwBuildId` | `16`（长度） | `H A - S P A 4 C 1 - 1 . 0 . 1 0` | 17 字节 |

代码示例：

```c
const uint8 zclSampleSw_ManufacturerName[] = { 6, 'L','i','n','x','e','e' };
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };
const uint8 zclSampleSw_DateCode[] = { 8, '2','0','2','6','0','7','3','1' };
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','1','.','0','.','1','0' };
```

### 5.2 与 C 字符串的区别

- C 字符串：`"Linxee"` 占 7 字节（含 `\0`），无长度前缀
- ZCL 字符串：`{ 6, 'L','i','n','x','e','e' }` 占 7 字节（含长度前缀 6），无 `\0`
- **切勿混淆**，否则 z2m 解析会出错

## 6. SwBuildId 长度限制

### 6.1 MTU 超限问题

v1.0.10 修复了一个关键问题：SwBuildId 长度从 17 字节缩为 16 字节。

源码注释：

```c
// SwBuildId: 型号+版本号 (ZCL SwBuildId 属性)
//   v1.0.10: 长度从 17 缩为 16 字节 (去掉 V 前缀), 避免 ZCL Read Attrs Rsp 超 MTU 被丢弃
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','1','.','0','.','1','0' };
```

### 6.2 问题根因

- z2m interview 时会发送 `ZCL Read Attributes` 请求读取所有 Basic Cluster 属性
- 设备返回 `ZCL Read Attributes Response`，包含所有属性值
- ZCL Read Attributes Response 是单条 AF 消息，受 ZCL MTU 限制
- 若 SwBuildId 过长，整个 Response 超 MTU，**消息被丢弃**
- 表现：z2m interview 失败，固件 ID（SwBuildId）丢失，无法识别设备版本

### 6.3 长度计算

| 字段 | 内容 | 长度 |
|------|------|------|
| 长度前缀 | 1 字节 | 1 |
| SwBuildId 内容 | `HA-SPA4C1-1.0.10` | 15 |
| **总计** | | **16 字节** |

v1.0.9 之前（含 `V` 前缀，17 字节，超限）：

| 字段 | 内容 | 长度 |
|------|------|------|
| 长度前缀 | 1 字节 | 1 |
| SwBuildId 内容 | `VHA-SPA4C1-1.0.10`（错误示例） | 16 |
| **总计** | | **17 字节**（超 MTU） |

v1.0.10 去掉 `V` 前缀，缩为 16 字节，问题解决。

### 6.4 设计约束

设计 SwBuildId 时必须控制总长度（含长度前缀）在 16 字节以内，避免 ZCL Read Attributes Response 超 MTU。

## 7. EP11 用于 z2m interview

### 7.1 端点冲突修复（BUG-013）

v1.0.3 修复了一个端点冲突问题：

```c
// v1.0.3修复BUG-013: 原SAMPLESW_ENDPOINT=8与SAMPLESW_ENDPOINT_INPUT4=8冲突,
// 导致EP8的SimpleDescriptor被input4覆盖,z2m找不到genBasic cluster,interview失败.
// 改为EP11避开EP1-8(继电器EP1-4 + 输入状态EP5-8),z2m在EP11上读取genBasic完成interview.
#define SAMPLESW_ENDPOINT               11
```

### 7.2 设计要点

- EP1~EP4：继电器端点（genOnOff server，仅含 OnOff 属性）
- EP5~EP8：输入状态端点（genAnalogInput server，仅含 presentValue 属性）
- **EP11**：Basic Cluster 端点，包含所有 Basic Cluster 属性（ModelId、ManufacturerName、SwBuildId 等），供 z2m interview

这样避免了 Basic Cluster 属性分散在 4 个继电器端点上导致的重复定义和 z2m interview 失败。

## 8. 借壳验证

### 8.1 z2m 识别流程

1. 设备入网后，z2m 发送 `ZCL Read Attributes` 请求到 EP11
2. 设备返回 ModelId = `LXN-4S27LX1.0`
3. z2m 匹配内置 HGZB-4S 转换规则
4. z2m 自动创建 4 个 switch 实体（l1~l4）和 4 个 input 实体（in1~in4）
5. z2m 通过 EP1~EP4 控制 4 路继电器，通过 EP5~EP8 监听触摸状态

### 8.2 验证清单

- [ ] z2m 日志显示 `LXN-4S27LX1.0` 被识别为 HGZB-4S
- [ ] z2m 自动创建 4 个 switch 实体（state_l1~state_l4）
- [ ] z2m 自动创建 4 个 input 实体（input_state_in1~in4）
- [ ] z2m interview 不报错，固件 ID 显示为 `HA-SPA4C1-1.0.10`
- [ ] 通过 z2m 控制 4 路继电器，状态正确同步
- [ ] 触摸操作后，z2m 状态正确更新

## 9. 经验教训

### 9.1 ModelId 是借壳的关键

- z2m 通过 ModelId 匹配转换规则
- 修改 ModelId 后，所有属性（ManufacturerName、SwBuildId 等）可自定义
- **切勿修改 ModelId**，否则 z2m 无法识别

### 9.2 ZCL 字符串不含 \0

- 第一字节是长度前缀，后面是字符内容
- 不要在末尾加 `\0`，否则 z2m 解析出错
- 长度前缀的值是字符数，不含长度前缀本身

### 9.3 SwBuildId 长度影响 MTU

- ZCL Read Attributes Response 是单条 AF 消息
- 属性过多或过长会导致超 MTU，消息被丢弃
- SwBuildId 总长度（含长度前缀）建议 ≤ 16 字节

### 9.4 端点布局要避免冲突

- 继电器端点（EP1~EP4）只放 genOnOff
- 输入端点（EP5~EP8）只放 genAnalogInput
- Basic Cluster 集中放在 EP11，避免端点冲突

## 10. 相关文件

- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw_data.c`
  - `zclSampleSw_ModelId[]`（借壳必需）
  - `zclSampleSw_ManufacturerName[]`（可自定义）
  - `zclSampleSw_SwBuildId[]`（可自定义，长度 ≤ 16）
  - `zclSampleSw_DateCode[]`（编译日期）
  - `zclSampleSw_HWRevision`（硬件版本）
  - `zclSampleSw_Attrs[]`（属性注册表）
  - `zclSampleSw_RelaySimpleDesc[]`（4 路继电器端点描述符）
  - `zclSampleSw_InputSimpleDesc[]`（4 路输入端点描述符）
  - `zclSampleSw_SimpleDesc`（EP11 Basic Cluster 端点描述符）
- `Projects\zstack\HomeAutomation\HGZBSwitch\Source\zcl_samplesw.h`
  - `SAMPLESW_ENDPOINT`（EP11，z2m interview 端点）
  - 端点 ID 定义（EP1~EP8）
