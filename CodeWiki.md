# Z-Stack 3.0.2 Code Wiki 文档

> **项目**: Z-Stack 3.0.2 (ZigBee 3.0 Compliant Platform)
> **厂商**: Texas Instruments (TI)
> **内核版本**: Z-Stack Core 2.7.2
> **发布日期**: 2018-06-15
> **目标硬件**: CC2530 (8051) / CC2531 (USB) / CC2538 (ARM Cortex-M3)

---

## 目录

1. [项目概述](#1-项目概述)
2. [项目目录结构](#2-项目目录结构)
3. [整体架构](#3-整体架构)
4. [核心模块详解](#4-核心模块详解)
   - 4.1 [OSAL — 操作系统抽象层](#41-osal--操作系统抽象层)
   - 4.2 [HAL — 硬件抽象层](#42-hal--硬件抽象层)
   - 4.3 [MAC — 媒体访问控制层](#43-mac--媒体访问控制层)
   - 4.4 [NWK — 网络层](#44-nwk--网络层)
   - 4.5 [APS — 应用支持子层](#45-aps--应用支持子层)
   - 4.6 [AF — 应用框架](#46-af--应用框架)
   - 4.7 [ZCL — Zigbee 集群库](#47-zcl--zigbee-集群库)
   - 4.8 [ZDO — Zigbee 设备对象](#48-zdo--zigbee-设备对象)
   - 4.9 [BDB — 基础设备行为](#49-bdb--基础设备行为)
   - 4.10 [MT — 监视与测试接口](#410-mt--监视与测试接口)
   - 4.11 [ZNP — Zigbee 网络处理器](#411-znp--zigbee-网络处理器)
   - 4.12 [ZMAC — Zigbee MAC 层适配](#412-zmac--zigbee-mac-层适配)
   - 4.13 [Services — 基础服务层](#413-services--基础服务层)
   - 4.14 [GP — Green Power 功能](#414-gp--green-power-功能)
5. [示例应用程序](#5-示例应用程序)
6. [依赖关系](#6-依赖关系)
7. [项目构建与运行](#7-项目构建与运行)
8. [关键配置说明](#8-关键配置说明)

---

## 1. 项目概述

Z-Stack 3.0.2 是 TI 提供的 **Zigbee 3.0 全兼容协议栈**，基于 Z-Stack Core 2.7.2 构建。它实现了 Zigbee 3.0 标准的所有核心要求，包括：

- **Base Device Behaviour (BDB) 1.0** — 标准化的设备入网和行为规范
- **GreenPower Basic Proxy** — 支持 Green Power 低功耗设备代理
- **ZCL 6 (Zigbee Cluster Library)** — 标准集群库，v6 版本
- **Zigbee PRO** — 支持 Zigbee PRO 网络特性（安全、多跳路由等）

Z-Stack 提供了一套完整的 Zigbee 嵌入式协议栈，适用于智能家居、照明、安防等物联网应用场景。

---

## 2. 项目目录结构

```
Z-Stack/
├── Components/               # 协议栈组件源码
│   ├── bsp/                  # 板级支持包 (SRF06EB for CC2538)
│   ├── driverlib/            # CC2538 驱动库 (寄存器级)
│   ├── hal/                  # 硬件抽象层
│   │   ├── common/           # 通用 HAL 代码 (断言、驱动框架)
│   │   ├── include/          # HAL 公共头文件
│   │   └── target/           # 平台目标代码
│   │       ├── CC2530EB/     # CC2530 开发板
│   │       ├── CC2530USB/    # CC2531 USB 加密狗
│   │       ├── CC2530ZNP/    # CC2530 ZNP 模式
│   │       ├── CC2538/       # CC2538 ARM 平台
│   │       └── CC2538ZNP/    # CC2538 ZNP 模式
│   ├── mac/                  # IEEE 802.15.4 MAC 层
│   │   ├── high_level/       # MAC 高层 (PIB、配置、安全)
│   │   ├── include/          # MAC API 头文件
│   │   └── low_level/        # MAC 底层实现
│   │       ├── srf04/        # SRF04 平台 (CC2430)
│   │       └── srf05/        # SRF05 平台 (CC2530)
│   ├── mt/                   # 监视与测试 (Monitor Test) 接口
│   ├── osal/                 # 操作系统抽象层
│   │   ├── common/           # OSAL 核心 (任务调度、内存、时钟、定时器、电源管理)
│   │   ├── include/          # OSAL 头文件
│   │   └── mcu/              # MCU 相关 (NV 存储)
│   │       ├── cc2530/       # CC2530 平台 NV 和数学库
│   │       └── cc2538/       # CC2538 平台 NV
│   ├── services/             # 基础服务
│   │   ├── ecc/              # 椭圆曲线加密 (ECC)
│   │   ├── saddr/            # 地址操作
│   │   └── sdata/            # 安全数据
│   ├── stack/                # 协议栈核心
│   │   ├── af/               # 应用框架 (Application Framework)
│   │   ├── bdb/              # 基础设备行为 (Base Device Behaviour)
│   │   ├── gp/               # Green Power
│   │   ├── nwk/              # 网络层 (Network Layer)
│   │   ├── sapi/             # 简单应用接口 (Simple API)
│   │   ├── sec/              # 安全模块
│   │   ├── sys/              # 系统全局 (ZGlobals, ZDiags)
│   │   ├── zcl/              # Zigbee 集群库 (ZCL)
│   │   └── zdo/              # Zigbee 设备对象 (ZDO)
│   ├── usblib/               # USB 库 (CC2538)
│   └── zmac/                 # ZMAC 层 (MAC 层适配)
│
├── Projects/                 # 工程项目
│   └── zstack/
│       ├── HomeAutomation/   # 智能家居示例应用
│       │   ├── GenericApp/               # 通用应用
│       │   ├── SampleDoorLock/           # 门锁示例
│       │   ├── SampleDoorLockController/ # 门锁控制器示例
│       │   ├── SampleLight/              # 灯示例
│       │   ├── SampleSwitch/             # 开关示例
│       │   ├── SampleTemperatureSensor/  # 温度传感器示例
│       │   ├── SampleThermostat/         # 恒温器示例
│       │   └── Source/                   # 共享源文件
│       │       ├── zcl_ha.c/h            # 智能家居 ZCL 配置
│       │       └── zcl_sampleapps_ui.c/h # 通用 UI
│       ├── Libraries/        # 预编译库
│       │   ├── CC2538/       # CC2538 库 (.a)
│       │   ├── TI2530DB/     # CC2530 库 (.lib)
│       │   └── TIMAC/        # 定时 MAC 库
│       ├── OTA/              # OTA 升级
│       │   ├── Boot/         # Bootloader (CC2530/CC2538)
│       │   ├── Dongle/       # OTA Dongle 应用
│       │   └── Source/       # OTA 公共源码
│       ├── Tools/            # 配置文件
│       │   ├── CC2530DB/     # CC2530 链接脚本/配置
│       │   └── CC2538DB/     # CC2538 链接脚本/配置
│       ├── Utilities/        # 工具
│       │   └── BootLoad/     # 串口 Bootloader
│       ├── ZMain/            # 主入口
│       │   ├── TI2530DB/     # CC2530 主函数
│       │   ├── TI2530ZNP/    # CC2530 ZNP 主函数
│       │   ├── TI2538DB/     # CC2538 主函数
│       │   └── TI2538ZNP/    # CC2538 ZNP 主函数
│       └── ZNP/              # ZNP 项目
│           ├── CC253x/       # CC2530/CC2531 ZNP
│           ├── CC2538/       # CC2538 ZNP
│           └── Source/       # ZNP 公共源码
│
├── Documents/                # 文档
│   ├── API/                  # API 文档 (PDF)
│   ├── CC2530/               # CC2530 相关文档
│   └── CC2538/               # CC2538 相关文档
│
├── Tools/                    # 上位机工具
│   └── Z-Tool/               # Z-Tool 调试工具
│
├── Accessories/              # 辅助工具安装包
│   ├── OtaServer/            # OTA 服务器
│   └── SerialBootTool/       # 串口 Boot 工具
│
├── _iss/                     # 安装程序相关
├── Z-Stack Core Release Notes.txt
├── Z-Stack 3.0 Release Notes.txt
├── Z-Stack_3.0.2_Manifest.html
├── Getting Started Guide - CC2530.pdf
└── Getting Started Guide - CC2538.pdf
```

---

## 3. 整体架构

Z-Stack 采用**分层架构**，遵循 Zigbee 协议标准的分层模型。从下到上依次为：

```
┌─────────────────────────────────────────────────────────┐
│                    应用层 (Application)                    │
│  ┌─────────────────────────────────────────────────────┐ │
│  │  ZCL (Zigbee Cluster Library)  /  自定义应用         │ │
│  │  ZDO (Zigbee Device Object)                         │ │
│  └──────────────────────┬──────────────────────────────┘ │
│                         │ AF (应用框架)                   │
│  ┌──────────────────────┴──────────────────────────────┐ │
│  │  APS (应用支持子层)                                   │ │
│  └──────────────────────┬──────────────────────────────┘ │
│                         │ NWK (网络层)                    │
│  ┌──────────────────────┴──────────────────────────────┐ │
│  │  MAC (媒体访问控制层) - IEEE 802.15.4                │ │
│  └──────────────────────┬──────────────────────────────┘ │
│                         │                                 │
│  ┌──────────────────────┴──────────────────────────────┐ │
│  │  HAL (硬件抽象层) / PHY (物理层 - RF)                │ │
│  └─────────────────────────────────────────────────────┘ │
│                                                           │
│  ┌─────────────────────────────────────────────────────┐ │
│  │  OSAL (操作系统抽象层) - 任务调度核心                  │ │
│  └─────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

**系统启动流程**:

1. **硬件复位** → `main()` (ZMain.c)
2. 硬件初始化（时钟、GPIO、UART、定时器等）
3. `osal_init_system()` — 初始化 OSAL 系统
4. `osalInitTasks()` — 按优先级初始化所有任务（MAC → NWK → HAL → MT → APS → ZDO → ZCL → BDB → 应用）
5. `osal_start_system()` — 进入无限循环，轮询任务事件

**任务调度机制**:

OSAL 采用**优先级轮询调度**。任务在 `tasksArr[]` 数组中按优先级排布，索引越小优先级越高。每个任务由 `pTaskEventHandlerFn` 回调函数处理事件。典型任务顺序：

```
MAC > NWK > GP > HAL > MT > APS > APSF > ZDO > ZDNwkMgr >
StubAPS > TouchLink Initiator > TouchLink Target > ZCL > BDB > 应用
```

---

## 4. 核心模块详解

### 4.1 OSAL — 操作系统抽象层

**路径**: `Components/osal/`

OSAL 是 Z-Stack 的核心调度引擎，为上层组件提供与操作系统无关的运行时环境。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `OSAL.c` | 核心调度循环：`osal_start_system()` 轮询各任务事件 |
| `OSAL_Task.c` | 静态任务事件表，`osal_set_event()` 设置任务事件 |
| `OSAL_Timers.c` | 软件定时器管理，`osal_start_timerEx()` 等 |
| `OSAL_Memory.c` | 动态内存分配/释放（固定池分配器） |
| `OSAL_Clock.c` | 系统时钟管理 |
| `OSAL_PwrMgr.c` | 电源管理（睡眠模式控制） |

#### 关键数据结构

```c
// 任务事件处理函数指针类型
typedef unsigned short (*pTaskEventHandlerFn)( unsigned char task_id, unsigned short event );

// 任务事件表（在 OSAL_xxx.c 中定义，每个应用不同）
const pTaskEventHandlerFn tasksArr[] = { ... };
const uint8 tasksCnt;  // 任务总数
uint16 *tasksEvents;   // 任务事件标志位数组
```

#### 核心函数

| 函数 | 说明 |
|------|------|
| `osal_init_system()` | 初始化 OSAL 系统（内存、定时器、电源管理、任务） |
| `osal_start_system()` | 启动调度循环（永不返回） |
| `osal_set_event(taskID, event)` | 为指定任务设置事件标志 |
| `osal_start_timerEx(taskID, eventID, timeout)` | 启动一次性软件定时器 |
| `osal_start_reload_timer()` | 启动周期性软件定时器 |
| `osal_msg_allocate(len)` | 分配消息缓冲区 |
| `osal_msg_send(dest_task, msg_ptr)` | 发送消息到目标任务 |
| `osal_mem_alloc(size)` | 动态内存分配 |
| `osal_mem_free(ptr)` | 动态内存释放 |

#### 任务初始化流程

在 `osalInitTasks()` 中，按优先级顺序为每个任务分配 taskID，并调用各自的初始化函数：

```c
void osalInitTasks( void ) {
    uint8 taskID = 0;
    tasksEvents = (uint16 *)osal_mem_alloc(sizeof(uint16) * tasksCnt);
    osal_memset(tasksEvents, 0, sizeof(uint16) * tasksCnt);

    macTaskInit(taskID++);       // 0 - MAC
    nwk_init(taskID++);          // 1 - NWK
    gp_Init(taskID++);           // 2 - GreenPower
    Hal_Init(taskID++);          // 3 - HAL
    MT_TaskInit(taskID++);       // 4 - MT
    APS_Init(taskID++);          // 5 - APS
    APSF_Init(taskID++);         // 6 - APS Fragmentation
    ZDApp_Init(taskID++);        // 7 - ZDO
    ZDNwkMgr_Init(taskID++);     // 8 - ZDO Network Manager
    zcl_Init(taskID++);          // 9 - ZCL
    bdb_Init(taskID++);          // 10 - BDB
    zclSampleLight_Init(taskID++); // 11 - 应用
}
```

---

### 4.2 HAL — 硬件抽象层

**路径**: `Components/hal/`

HAL 封装了所有 MCU 外设驱动，提供统一的 API 接口，使上层代码与具体硬件平台解耦。

#### 支持的平台

| 目标目录 | 芯片 | 描述 |
|----------|------|------|
| `CC2530EB/` | CC2530 (8051) | 标准开发板，支持 UART(DMA/ISR)、定时器、ADC、Flash、DMA、LCD、LED、按键、睡眠、OAD |
| `CC2530USB/` | CC2531 (8051+USB) | USB 加密狗，额外支持 USB CDC 驱动 |
| `CC2530ZNP/` | CC2530 (8051) | ZNP 模式，额外支持 SPI 接口 |
| `CC2538/` | CC2538 (ARM) | ARM Cortex-M3 平台，额外支持 AES、CCM、SHA、SRNG、SysTick |
| `CC2538ZNP/` | CC2538 (ARM) | CC2538 ZNP 模式 |

#### 关键 HAL API

| 模块 | 头文件 | 功能 |
|------|--------|------|
| GPIO/LED | `hal_led.h` | LED 控制：`HalLedInit()`, `HalLedSet()`, `HalLedBlink()` |
| 按键 | `hal_key.h` | 按键扫描：`HalKeyInit()`, `HalKeyConfig()`, `HalKeyRead()` |
| UART | `hal_uart.h` | 串口通信：`HalUARTInit()`, `HalUARTOpen()`, `HalUARTRead()`, `HalUARTWrite()` |
| 定时器 | `hal_timer.h` | 硬件定时器：`HalTimerInit()`, `HalTimerConfig()`, `HalTimerStart()` |
| ADC | `hal_adc.h` | 模数转换：`HalAdcInit()`, `HalAdcRead()` |
| Flash | `hal_flash.h` | Flash 读写：`HalFlashRead()`, `HalFlashWrite()`, `HalFlashErase()` |
| LCD | `hal_lcd.h` | 液晶显示：`HalLcdInit()`, `HalLcdWriteString()`, `HalLcdDisplayImage()` |
| 睡眠 | `hal_sleep.h` | 低功耗：`HalSleep()`, `HalSleepCheck()`, `HalMcuSetLowPowerMode()` |
| DMA | `hal_dma.h` | DMA 传输：`HalDmaInit()`, `HalDmaStart()` |
| SPI | `hal_spi.h` | SPI 通信：`SpiInit()`, `SpiRead()`, `SpiWrite()` |
| 加密 | `hal_aes.h` | AES 加密：`HalAesInit()`, `HalAesCrypto()` |
| CCM | `hal_ccm.h` | CCM* 加密认证：`HalCcmInit()`, `HalCcmCrypto()` |
| 随机数 | `hal_srng.h` | 安全随机数：`HalSrngInit()`, `HalSrngRead()` |
| OAD/OTA | `hal_oad.h` / `hal_ota.h` | 无线升级接口 |

---

### 4.3 MAC — 媒体访问控制层

**路径**: `Components/mac/`

实现 IEEE 802.15.4 MAC 层协议，负责无线信道的访问控制、帧收发、CSMA-CA、信标管理、GTS 管理等。

#### 子目录结构

```
mac/
├── high_level/      # MAC 高层协议
│   ├── mac_cfg.c    # MAC 配置
│   ├── mac_pib.c    # MAC PIB (PAN Information Base) 管理
│   ├── mac_pib.h
│   ├── mac_security.h  # MAC 安全
│   └── mac_spec.h   # MAC 规范常量
├── include/
│   └── mac_api.h    # 对外 MAC API
└── low_level/       # MAC 底层实现
    ├── srf04/       # 老平台 (CC2430)
    └── srf05/       # 新平台 (CC2530)
        ├── single_chip/
        │   ├── mac_csp_tx.c       # 命令选通处理器发送
        │   ├── mac_mcu.c          # MCU 接口
        │   ├── mac_mem.c          # MAC 内存管理
        │   ├── mac_radio_defs.c   # 射频寄存器定义
        │   └── mac_rffrontend.c   # 射频前端控制
        ├── mac_autopend.c         # 自动挂起
        ├── mac_backoff_timer.c    # 退避定时器
        ├── mac_low_level.c        # MAC 底层主控
        ├── mac_radio.c            # 射频控制
        ├── mac_rx.c               # 接收处理
        ├── mac_rx_onoff.c         # 接收开关控制
        ├── mac_sleep.c            # MAC 睡眠
        └── mac_tx.c               # 发送处理
```

#### 关键函数

| 函数 | 说明 |
|------|------|
| `macTaskInit()` | MAC 任务初始化 |
| `macEventLoop()` | MAC 事件处理循环 |
| `MAC_Init()` | MAC 层初始化 |
| `MAC_ScanReq()` | 扫描请求（信道扫描） |
| `MAC_AssociateReq()` | 关联请求 |
| `MAC_StartReq()` | 启动 PAN 请求 |
| `MAC_DataReq()` | 数据发送请求 |
| `MAC_PollReq()` | 轮询请求（休眠设备从父节点取数据） |
| `MAC_RadioSetPower()` | 设置射频发射功率 |

---

### 4.4 NWK — 网络层

**路径**: `Components/stack/nwk/`

实现 Zigbee 网络层协议，负责网络组建、路由、设备加入/离开、地址管理、绑定等。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `nwk_globals.c/h` | 网络层全局变量和配置 |
| `nwk.h` | 网络层 API 头文件 |
| `APS.h` | 应用支持子层定义 |
| `BindingTable.c/h` | 绑定表管理 |
| `AddrMgr.h` | 地址管理（16位短地址 ↔ 64位 IEEE 地址映射） |
| `AssocList.h` | 关联表管理 |
| `NLMEDE.h` | 网络层管理实体接口 |
| `APSMEDE.h` | APS 管理实体接口 |
| `stub_aps.c/h` | 用于 Inter-PAN 通信的 Stub APS |
| `rtg.h` | 路由表 |
| `reflecttrack.h` | 反射跟踪 |
| `aps_frag.h` | APS 分片 |
| `aps_groups.h` | 组管理 |
| `aps_util.h` | APS 工具函数 |

#### 网络层关键概念

- **网络拓扑**: 星型、树型、网状 (Mesh)
- **设备类型**: Coordinator (协调器), Router (路由器), EndDevice (终端设备)
- **路由协议**: Zigbee 按需距离向量路由 (AODV)
- **地址分配**: 分布式地址分配机制 (DAM) 或随机地址分配
- **安全**: 网络层安全 (AES-CCM*)

---

### 4.5 APS — 应用支持子层

APS 层位于 NWK 层之上，提供：

- 应用层数据包的拆分与重组
- 端到端确认重传
- 绑定表查找与转发
- 组地址管理
- 应用层安全

---

### 4.6 AF — 应用框架

**路径**: `Components/stack/af/`

AF 是应用层与协议栈之间的桥梁，定义和实现应用框架。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `AF.h` | AF 接口定义，包含端点描述符、发送选项等 |
| `AF.c` | AF 实现 |

#### 关键数据结构

```c
// 简单描述符 - 定义端点支持的设备配置
typedef struct {
    uint8  EndPoint;           // 端点号 (1-240)
    uint16 AppProfId;          // Profile ID (如 HA=0x0104)
    uint16 AppDeviceId;        // 设备 ID
    uint8  AppDevVer:4;        // 设备版本
    uint8  Reserved:4;         // 保留
    uint8  AppNumInClusters;   // 输入集群数量
    cId_t *pAppInClusterList;  // 输入集群列表指针
    uint8  AppNumOutClusters;  // 输出集群数量
    cId_t *pAppOutClusterList; // 输出集群列表指针
} SimpleDescriptionFormat_t;

// AF 数据发送请求
typedef struct {
    osal_event_hdr_t hdr;      // 事件头
    uint8 group_id;            // 组 ID
    uint16 cluster_id;         // 集群 ID
    afAddrType_t *dstAddr;     // 目标地址
    uint16 sendTimeout;        // 发送超时
    uint8 *pData;              // 数据指针
    uint8 data_len;            // 数据长度
    uint8 *key;                // 密钥
    uint8 link_key;            // 链路密钥
} afDataConfirm_t;
```

#### 关键函数

| 函数 | 说明 |
|------|------|
| `afRegister()` | 注册应用端点 |
| `afDataRequest()` | 发送 AF 数据请求 |
| `afStatus()` | 获取 AF 状态 |

---

### 4.7 ZCL — Zigbee 集群库

**路径**: `Components/stack/zcl/`

ZCL 实现了 Zigbee 联盟定义的标准化集群库，提供设备间互操作协议。

#### 关键文件

| 文件 | 实现集群 |
|------|----------|
| `zcl.c/h` | ZCL 核心框架（帧构建/解析、通用命令处理） |
| `zcl_general.c/h` | 通用集群：Basic, Power Config, Identify, Groups, Scenes, On/Off, Level Control, Alarms, Time |
| `zcl_lighting.c/h` | 照明集群：Color Control |
| `zcl_hvac.c/h` | HVAC 集群：温控器、风扇、湿度 |
| `zcl_ms.c/h` | 测量与传感集群：温度、湿度、光照、压力 |
| `zcl_ss.c/h` | 安全与安防集群：IAS Zone, IAS Warning |
| `zcl_se.c/h` | 智能能源集群：计量、定价 |
| `zcl_ota.c/h` | OTA 升级集群 |
| `zcl_cc.c/h` | 颜色控制集群 |
| `zcl_diagnostic.c/h` | 诊断集群 |
| `zcl_ha.c/h` | 智能家居 Profile 配置 |
| `zcl_key_establish.c/h` | 密钥建立集群 |
| `zcl_poll_control.c/h` | 轮询控制集群 |
| `zcl_electrical_measurement.c/h` | 电气测量集群 |
| `zcl_green_power.c/h` | Green Power 集群 |
| `zcl_partition.c/h` | 分区集群 |
| `zcl_closures.c/h` | 门窗集群（门锁、窗帘） |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `zcl_Init()` | ZCL 初始化 |
| `zcl_event_loop()` | ZCL 事件处理 |
| `zcl_registerCluster()` | 注册集群 |
| `zcl_SendData()` | 发送 ZCL 数据帧 |
| `zcl_ProcessIncoming()` | 处理接收到的 ZCL 帧 |
| `zcl_ProcessInReadRsp()` | 处理读属性响应 |
| `zcl_ProcessInWrite()` | 处理写属性命令 |
| `zcl_ProcessInReport()` | 处理属性报告 |
| `zcl_ProcessInDefaultRsp()` | 处理默认响应 |
| `zcl_ProcessInCmd()` | 处理集群特定命令 |

---

### 4.8 ZDO — Zigbee 设备对象

**路径**: `Components/stack/zdo/`

ZDO 是 Zigbee 协议栈的"管理面"，负责设备发现、服务发现、绑定管理、网络管理、安全管理等系统级功能。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `ZDApp.c/h` | ZDO 应用主控（任务初始化、网络启动、事件处理） |
| `ZDObject.c/h` | ZDO 端点管理（端点 0 的注册和配置） |
| `ZDConfig.c/h` | ZDO 配置参数（NIB 初始化、配置读取/写入） |
| `ZDProfile.c/h` | ZDO 配置文件描述（设备和服务发现、绑定等标准命令） |
| `ZDSecMgr.c/h` | ZDO 安全管理（信任中心、链路密钥、安装码） |
| `ZDNwkMgr.c/h` | ZDO 网络管理（信道切换、PAN ID 冲突处理） |

#### 关键函数

| 函数 | 说明 |
|------|------|
| `ZDApp_Init()` | ZDO 初始化 |
| `ZDApp_event_loop()` | ZDO 事件处理循环 |
| `ZDO_StartDevice()` | 启动设备（入网/建网） |
| `ZDO_NetworkDiscoveryRequest()` | 网络发现请求 |
| `ZDO_DiscoveryRequest()` | 服务发现请求 |
| `ZDO_BindReq()` | 绑定请求 |
| `ZDO_UnbindReq()` | 解绑请求 |
| `ZDO_RemoveDeviceReq()` | 移除设备请求 |
| `ZDO_GetDeviceInformation()` | 获取设备信息 |
| `ZDO_NetworkUpdate()` | 网络更新（信道切换等） |
| `ZDSecMgr_Init()` | 安全管理初始化 |
| `ZDSecMgr_EstablishKey()` | 建立链路密钥 |

---

### 4.9 BDB — 基础设备行为

**路径**: `Components/stack/bdb/`

BDB 是 Zigbee 3.0 的核心规范，定义了设备入网、配置和交互的标准行为。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `bdb.c/h` | BDB 核心实现（任务调度、状态机、全局 API） |
| `bdb_interface.h` | BDB 对外接口宏和 API |
| `bdb_Reporting.c/h` | BDB 属性报告管理 |
| `bdb_FindingAndBinding.c` | 发现和绑定（F&B）过程 |
| `bdb_tlCommissioning.c/h` | Touchlink 按需组网 |
| `bdb_touchlink.c/h` | Touchlink 核心实现 |
| `bdb_touchlink_initiator.c/h` | Touchlink 发起方 |
| `bdb_touchlink_target.c/h` | Touchlink 目标方 |

#### BDB 入网状态机

BDB 定义了三种入网方式：

1. **Network Steering** — 通过扫描现有网络并加入（标准入网）
2. **Network Formation** — 创建新网络（协调器建网）
3. **Finding & Binding** — 设备发现和绑定
4. **Touchlink** — 通过 NFC 类似的近距离触碰组网

#### 关键 API

```c
// 启动 BDB 入网流程
void bdb_StartCommissioning(uint8 mode);

// BDB 回调 - 报告入网状态
typedef void (*bdb_CommissioningStatusCB_t)(bdb_CommissioningMode_t mode, bdb_Status_t status);

// 设置识别模式
void bdb_SetIdentifyTime(uint16 identifyTime);

// 注册入网状态回调
void bdb_RegisterCommissioningStatusCB(bdb_CommissioningStatusCB_t cb);
```

---

### 4.10 MT — 监视与测试接口

**路径**: `Components/mt/`

MT 层提供了一套基于串口/SPI 的命令接口，用于主机与 Z-Stack 设备通信，是 ZNP 的核心通信协议。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `MT.c/h` | MT 核心框架（命令解析、发送、接收） |
| `MT_UART.c/h` | UART 传输层 |
| `MT_SYS.c/h` | 系统命令（版本、复位、地址、NV 操作） |
| `MT_AF.c/h` | AF 命令（数据发送、端点注册） |
| `MT_ZDO.c/h` | ZDO 命令（网络发现、绑定、设备管理） |
| `MT_NWK.c/h` | NWK 命令（网络层操作） |
| `MT_MAC.c/h` | MAC 命令（MAC 层操作） |
| `MT_SAPI.c/h` | SAPI 命令（简单应用接口） |
| `MT_APP.c/h` | 应用命令 |
| `MT_APP_CONFIG.c/h` | 应用配置命令 |
| `MT_DEBUG.c/h` | 调试命令 |
| `MT_UTIL.c/h` | 工具命令（地址管理、回调注册） |
| `MT_OTA.c/h` | OTA 命令 |
| `MT_GP.c/h` | Green Power 命令 |
| `MT_VERSION.c/h` | 版本信息 |
| `MT_TASK.c/h` | MT 任务处理 |
| `MT_ZNP.c/h` | ZNP 特定命令 |
| `MT_RPC.h` | RPC 协议定义 |

#### MT 命令格式

```
┌──────┬──────┬────────┬──────────┬──────┐
│ SOF  │ Len  │ Cmd0   │ Cmd1     │ Data │  → FCS
│ 0xFE │ 1-255│ 类型   │ 命令号   │ N字节 │
└──────┴──────┴────────┴──────────┴──────┘
```

- **Cmd0**: 命令类型（SYS=0x00, AF=0x04, ZDO=0x05, NWK=0x06, UTIL=0x07, APP=0x0C, MT=0x0D 等）
- **Cmd1**: 具体命令号
- 命令分为 **SREQ** (同步请求), **SRSP** (同步响应), **AREQ** (异步请求)

---

### 4.11 ZNP — Zigbee 网络处理器

**路径**: `Projects/zstack/ZNP/`, `Components/stack/sapi/`

ZNP 是一种特殊的固件，将完整的 Zigbee 协议栈运行在无线 MCU 上，通过串口/SPI 与主机 MCU 通信。主机通过 MT 命令控制 ZNP 完成 Zigbee 功能。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `znp_app.c/h` | ZNP 主应用（NV 初始化、UART/SPI 接口、命令回调） |
| `OSAL_ZNP.c` | ZNP 任务定义和初始化 |
| `znp_cfg` | ZNP 配置文件 |
| `znp_soc.c` | SoC 模式应用 |
| `znp_spi.c` | SPI 接口实现 |
| `znp_spi_udma.c` | uDMA SPI 实现 |
| `sapi.c/h` | 简单应用接口（SAPI - 封装 AF/ZDO 为简易 API） |

#### ZNP 通信方式

- **UART**: 默认 115200 baud, 8N1 (CC2530 使用 ISR 或 DMA 模式)
- **SPI**: CC2538 支持 SPI 从模式，使用 uDMA 传输
- **USB**: CC2531 支持 USB CDC 虚拟串口

#### ZNP 命令处理流程

```
主机 → [MT 命令帧] → UART/SPI → MT 解析 → 调用对应模块
                                             ↓
主机 ← [MT 响应帧] ← UART/SPI ← MT 组帧 ← 模块处理结果
```

---

### 4.12 ZMAC — Zigbee MAC 层适配

**路径**: `Components/zmac/`

ZMAC 是 MAC 层的适配层，将底层 MAC 实现与上层协议栈解耦。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `ZMAC.h` | ZMAC 对外接口 |
| `f8w/zmac.c` | ZMAC 主实现 |
| `f8w/zmac_cb.c` | ZMAC 回调处理 |
| `f8w/zmac_internal.h` | ZMAC 内部数据结构 |

---

### 4.13 Services — 基础服务层

**路径**: `Components/services/`

| 模块 | 文件 | 职责 |
|------|------|------|
| saddr | `saddr.c/h` | 地址操作（复制、比较、地址模式判断） |
| sdata | `sdata.h` | 安全数据结构定义 |
| ecc | `eccapi_163.h`, `eccapi_283.h` | 椭圆曲线加密 API（163位/283位） |

---

### 4.14 GP — Green Power 功能

**路径**: `Components/stack/gp/`

Green Power 是 Zigbee 3.0 中用于超低功耗设备的特性，支持能量采集设备（如无电池开关）。

#### 关键文件

| 文件 | 职责 |
|------|------|
| `gp_common.c/h` | Green Power 公共实现 |
| `gp_interface.h` | GP 接口定义 |
| `gp_proxyTbl.c` | GP 代理表管理 |
| `cGP_stub.h` / `dGP_stub.h` | 紧凑/完整 GP 存根 |

---

## 5. 示例应用程序

所有示例应用位于 `Projects/zstack/HomeAutomation/` 目录，每个应用包含：

- `Source/` — 应用源码
- `CC2530DB/` — CC2530 IAR 工程文件
- `CC2538/` — CC2538 IAR 工程文件

### 5.1 应用列表

| 应用 | 源文件 | 功能描述 |
|------|--------|----------|
| **SampleLight** | `zcl_samplelight.c/h` | 实现 Zigbee 灯光设备（On/Off 或可调光），注册 On/Off、Level Control、Color Control 等集群 |
| **SampleSwitch** | `zcl_samplesw.c/h` | 实现 Zigbee 开关设备，可控制灯光 |
| **SampleTemperatureSensor** | `zcl_sampletemperaturesensor.c/h` | 实现温度传感器，周期性上报温度值 |
| **SampleThermostat** | `zcl_samplethermostat.c/h` | 实现恒温器设备 |
| **SampleDoorLock** | `zcl_sampledoorlock.c/h` | 实现门锁设备 |
| **SampleDoorLockController** | `zcl_sampledoorlockcontroller.c/h` | 实现门锁控制器 |
| **GenericApp** | `zcl_genericapp.c/h` | 通用应用模板，适合自定义开发 |

### 5.2 应用结构（以 SampleLight 为例）

每个应用由以下文件组成：

```
SampleLight/Source/
├── OSAL_SampleLight.c      # 任务定义和初始化（tasksArr[], osalInitTasks()）
├── zcl_samplelight.c       # 应用主逻辑（事件处理、命令处理、状态机）
├── zcl_samplelight.h       # 应用头文件
└── zcl_samplelight_data.c  # 应用数据（属性表、集群定义、端点描述符）
```

### 5.3 应用开发模式

1. **定义端点描述符** — 注册端点号、Profile ID、设备 ID、支持的集群列表
2. **定义属性表** — 注册需要管理的 ZCL 属性（属性 ID、类型、访问权限、值）
3. **注册集群** — 调用 `zcl_registerCluster()` 注册输入/输出集群
4. **实现事件处理** — 在 `zclSampleLight_event_loop()` 中处理应用事件
5. **处理 ZCL 命令** — 实现集群回调函数处理入站命令
6. **注册 AF 端点** — 调用 `afRegister()` 完成端点注册

---

## 6. 依赖关系

### 6.1 模块依赖图

```
应用层 (SampleLight/Switch/...)
  │
  ├── ZCL (zcl.c → zcl_*.c)
  │     ├── AF (AF.c)
  │     ├── OSAL (OSAL.c, OSAL_Nv.c)
  │     └── APS (APS.h, aps_groups.h)
  │
  ├── BDB (bdb.c)
  │     ├── ZCL (zcl.c, zcl_general.c)
  │     ├── AF (AF.c)
  │     ├── ZDO (ZDProfile.c)
  │     └── SSP (ssp.h)
  │
  ├── ZDO (ZDApp.c)
  │     ├── AF (AF.c)
  │     ├── NWK (nwk.h, APS.h, NLMEDE.h)
  │     ├── ZMAC (ZMAC.h)
  │     ├── OSAL (OSAL.c, OSAL_Nv.c)
  │     └── SSP (ssp.h)
  │
  ├── AF (AF.c)
  │     ├── NWK (nwk.h)
  │     └── APS (APSMEDE.h)
  │
  ├── HAL (hal_drivers.c → hal_*.c)
  │     └── 硬件寄存器
  │
  └── OSAL (OSAL.c) — 所有模块都依赖 OSAL
```

### 6.2 预编译库依赖

| 库文件 | 适用平台 | 内容 |
|--------|----------|------|
| `AllDevice.a` / `AllDevice-Pro.lib` | CC2538/CC2530 | 完整设备库（包含协调器、路由、终端） |
| `Router.a` / `Router-Pro.lib` | CC2538/CC2530 | 路由器设备库 |
| `EndDevice.a` / `EndDevice-Pro.lib` | CC2538/CC2530 | 终端设备库 |
| `EndDeviceMt.a` / `EndDeviceMT-Pro.lib` | CC2538/CC2530 | 带 MT 的终端设备库 |
| `RouterMt.a` / `RouterMT-Pro.lib` | CC2538/CC2530 | 带 MT 的路由器库 |
| `Security.lib` | CC2530 | 安全模块库 |
| `ecc.r51` | CC2530 | ECC 加密库 |
| `bsp.lib` | CC2538 | 板级支持包库 |
| `driverlib.lib` | CC2538 | 外设驱动库 |
| `usblib.lib` / `usbcdc.lib` / `usbhid.lib` | CC2538 | USB 库 |
| `TIMAC-CC2530.lib` / `TIMAC-CC2538.a` | 两者 | 定时 MAC 库 |

---

## 7. 项目构建与运行

### 7.1 开发环境

| 平台 | 编译器 | 最低版本 |
|------|--------|----------|
| CC2530 / CC2531 | IAR EW8051 | 10.20.1 |
| CC2538 | IAR EWARM | 8.22.1 |

### 7.2 构建步骤

1. **安装 IAR Embedded Workbench**（对应平台版本）
2. **打开工程文件**：在 `Projects/zstack/HomeAutomation/xxx/CC2530DB/` 或 `CC2538/` 下找到 `.eww` 工作空间文件
3. **选择配置**：在 IAR 中选择目标配置（Coordinator / Router / EndDevice）
4. **编译**：Project → Make
5. **烧录**：
   - CC2530: 使用 CC Debugger 通过 IAR 直接下载
   - CC2538: 使用 JTAG/SWD 或串口 Bootloader

### 7.3 首次烧录注意事项

- 首次烧录前应**擦除整个 Flash**
  - CC2530: `Debugger → Texas Instruments → Download → Erase Flash`
  - CC2538: `Project → Download → Erase Memory`

### 7.4 设备类型配置

在 IAR 编译选项中定义以下宏来选择设备类型：

| 宏定义 | 设备类型 |
|--------|----------|
| `ZDO_COORDINATOR` + `ZIGBEEPRO` | 协调器 |
| `ZG_BUILD_RTR_TYPE` + `ZIGBEEPRO` | 路由器 |
| `ZG_BUILD_ENDDEVICE_TYPE` + `ZIGBEEPRO` | 终端设备 |

### 7.5 ZNP 使用方式

ZNP 固件编译后烧录到设备，通过串口/SPI 与主机通信：

```
主机 (Linux/Windows) ── UART/SPI ── ZNP 设备 (CC2530/CC2538)
```

主机端可通过 Z-Tool 工具或自定义程序发送 MT 命令控制 ZNP。

### 7.6 OTA 升级流程

1. 使用 `Projects/tools/OTA/OtaConverter.exe` 生成 OTA 固件镜像
2. 通过 OTA 服务器或 OTA Dongle 广播升级
3. 设备端运行 OTA 客户端集群接收升级数据
4. Bootloader 验证并写入新固件

---

## 8. 关键配置说明

### 8.1 网络配置 (f8wConfig.cfg)

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `ZIGBEEPRO` | 启用 Zigbee PRO 模式 | 启用 |
| `SECURE` | 启用网络层安全 | 1 |
| `DEFAULT_CHANLIST` | 默认信道列表 | 0x00000800 (信道11) |
| `ZDAPP_CONFIG_PAN_ID` | PAN ID (0xFFFF = 自动) | 0xFFFF |
| `NWK_START_DELAY` | 网络启动延迟(ms) | 100 |
| `ROUTE_EXPIRY_TIME` | 路由过期时间(秒) | 30 |
| `MAX_RTG_SRC_ENTRIES` | 最大源路由表项 | 40 |
| `NWK_MAX_DEVICE_LIST` | 最大设备列表 | 20 |

### 8.2 BDB 配置 (bdb_interface.h)

| 配置项 | 说明 | 默认值 |
|--------|------|--------|
| `BDBC_MIN_COMMISSIONING_TIME` | 最小组网时间(秒) | 180 |
| `BDB_INSTALL_CODE_USE` | 安装码使用方式 | IC+CRC |
| `BDB_FINDING_BINDING_CAPABILITY_ENABLED` | 发现绑定能力 | 1 |
| `BDB_NETWORK_STEERING_CAPABILITY_ENABLED` | 网络转向能力 | 1 |
| `BDB_NETWORK_FORMATION_CAPABILITY_ENABLED` | 建网能力 | 1 |
| `BDB_TOUCHLINK_CAPABILITY_ENABLED` | Touchlink 能力 | 条件启用 |

### 8.3 设备配置 (f8wEndev.cfg / f8wRouter.cfg / f8wCoord.cfg)

分别在对应设备类型的 cfg 文件中配置特定参数，如轮询间隔、休眠模式等。

---

## 附录

### A. 相关文档

| 文档 | 路径 |
|------|------|
| Z-Stack 3.0 Developer's Guide | `Documents/Z-Stack 3.0 Developer's Guide.pdf` |
| Z-Stack 3.0 Sample Application User's Guide | `Documents/Z-Stack 3.0 Sample Application User's Guide.pdf` |
| Z-Stack OTA Upgrade User's Guide | `Documents/Z-Stack OTA Upgrade User's Guide.pdf` |
| Z-Stack API | `Documents/API/Z-Stack API.pdf` |
| Z-Stack ZCL API | `Documents/API/Z-Stack ZCL API.pdf` |
| Z-Stack ZNP Interface Specification | `Documents/API/Z-Stack ZNP Interface Specification.pdf` |
| OSAL API | `Documents/API/OSAL API.pdf` |
| HAL Driver API | `Documents/API/HAL Driver API.pdf` |
| Getting Started Guide | `Getting Started Guide - CC2530.pdf` / `CC2538.pdf` |

### B. 关键头文件路径

| 头文件 | 路径 |
|--------|------|
| `comdef.h` | `Components/osal/include/comdef.h` |
| `OSAL.h` | `Components/osal/include/OSAL.h` |
| `OSAL_Nv.h` | `Components/osal/include/OSAL_Nv.h` |
| `hal_board.h` | `Components/hal/include/hal_board.h` |
| `hal_types.h` | `Components/hal/include/hal_types.h` |
| `AF.h` | `Components/stack/af/AF.h` |
| `zcl.h` | `Components/stack/zcl/zcl.h` |
| `ZDApp.h` | `Components/stack/zdo/ZDApp.h` |
| `bdb_interface.h` | `Components/stack/bdb/bdb_interface.h` |
| `MT.h` | `Components/mt/MT.h` |
| `mac_api.h` | `Components/mac/include/mac_api.h` |
| `ZMAC.h` | `Components/zmac/ZMAC.h` |
| `sapi.h` | `Components/stack/sapi/sapi.h` |

---

*本文档基于 Z-Stack 3.0.2 (2018-06-15) 源码分析生成，如有更新请以实际代码为准。*