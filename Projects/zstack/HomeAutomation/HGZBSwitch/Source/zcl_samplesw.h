/**************************************************************************************************
  Filename:       zcl_samplesw.h
  Description:    86四路智能开关Zigbee应用层头文件 (基于Z-Stack 3.0.2 SampleSwitch)

  硬件: CC2530F256 + WTC6106BSI触摸芯片 + 4路继电器
  借壳: ModelId = LXN-4S27LX1.0 (匹配z2m HGZB-4S转换规则)
  端点布局:
    EP1~EP4: 4路继电器 (genOnOff server)
    EP5~EP8: 4路输入状态 (genAnalogInput server)
    EP11:    Basic Cluster (供z2m interview)
**************************************************************************************************/

#ifndef ZCL_SAMPLESW_H
#define ZCL_SAMPLESW_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************
 * INCLUDES
 */
#include "zcl.h"

/*********************************************************************
 * 常量定义 - 端点布局
 */

// EP11: Basic Cluster端点, 供z2m interview (避开EP1-8继电器/输入端点)
#define SAMPLESW_ENDPOINT               11

// 4路继电器端点 (EP 1-4, 对应 z2m 的 l1-l4)
#define SAMPLESW_ENDPOINT_RELAY1        1
#define SAMPLESW_ENDPOINT_RELAY2        2
#define SAMPLESW_ENDPOINT_RELAY3        3
#define SAMPLESW_ENDPOINT_RELAY4        4
#define SAMPLESW_NUM_RELAYS             4

// 4路输入状态端点 (EP 5-8, 对应 z2m 的 in1-in4)
#define SAMPLESW_ENDPOINT_INPUT1        5
#define SAMPLESW_ENDPOINT_INPUT2        6
#define SAMPLESW_ENDPOINT_INPUT3        7
#define SAMPLESW_ENDPOINT_INPUT4        8
#define SAMPLESW_NUM_INPUTS             4

/*********************************************************************
 * 常量定义 - ZCL属性
 */

// ZCL属性ID: startUpOnOff (genOnOff cluster, 断电恢复策略)
#define ATTRID_STARTUP_ON_OFF           0x4003

// startUpOnOff属性值 (ZCL标准)
#define STARTUP_ONOFF_OFF               0x00    // 上电关闭
#define STARTUP_ONOFF_ON                0x01    // 上电开启
#define STARTUP_ONOFF_TOGGLE            0x02    // 上电切换
#define STARTUP_ONOFF_PREVIOUS          0xFF    // 恢复断电前状态(断电记忆)

// 继电器端点属性数量 (onOff + startUpOnOff)
#define ZCLSAMPLESW_NUM_RELAY_ATTRS     2
// 输入状态端点属性数量 (presentValue)
#define ZCLSAMPLESW_NUM_INPUT_ATTRS     1

/*********************************************************************
 * 常量定义 - NV存储 (断电记忆)
 */

// NV存储项ID (应用自定义, 避开Z-Stack系统区和ZNP保留区)
#define SAMPLESW_NV_ID_RELAY_STATE      0x0F10  // 4路继电器状态 (4字节)
#define SAMPLESW_NV_ID_STARTUP_ONOFF    0x0F12  // 4路独立startUpOnOff配置 (4字节)

// 延迟写入时间 (ms), 状态变化后等待此时间无新变化才写入NV
#define SAMPLESW_NV_SAVE_DELAY_MS       5000

/*********************************************************************
 * 常量定义 - 触摸检测
 */

// 触摸轮询周期 (50ms, 接近WTC6106BSI硬件响应极限)
#define TOUCH_POLL_INTERVAL_MS          50

// 连续2次(100ms)确认状态变化, 防抖
#define TOUCH_DEBOUNCE_COUNTS           2

// S1配网按键长按复位阈值 (5秒 = 100 * 50ms轮询周期)
#define S1_RESET_THRESHOLD              100

// 触摸输入引脚位掩码 (P0_4~P0_7)
#define TOUCH_INPUT_BV                  (BV(4) | BV(5) | BV(6) | BV(7))

// 继电器引脚位掩码 (P1口: P1_0/P1_2/P1_6, P2口: P2_0)
#define RELAY_P1_BV                     (BV(0) | BV(2) | BV(6))
#define RELAY_P2_BV                     (BV(0))

/*********************************************************************
 * 常量定义 - LED控制
 */

// 配网中LED1慢闪超时时间 (5分钟, 与BDB NWK_STEERING超时接近)
#define SAMPLESW_PAIRING_TIMEOUT_MS     (5 * 60 * 1000UL)

// 配网慢闪每次亮或灭的持续时间 (1Hz, 500ms亮/500ms灭)
#define PAIRING_BLINK_PERIOD_MS         500

// 复位闪烁参数 (3次闪烁 = 6次状态切换, 300ms亮/300ms灭)
#define RESET_BLINK_TOTAL_COUNT         6
#define RESET_BLINK_PERIOD_MS           300

/*********************************************************************
 * 常量定义 - TX功率
 */

// TX功率档位 (必须在bdb_StartCommissioning之前设置)
// 4 dBm, 与TI ZNP官方默认一致, 7dBm会导致RF不稳定
#define SAMPLESW_TX_POWER               TX_PWR_PLUS_4

/*********************************************************************
 * 事件定义
 */

// 触摸输入轮询事件 (50ms周期, 内含软件防抖和S1长按检测)
#define SAMPLESW_TOUCH_POLL_EVT         0x0001

// End Device rejoin事件 (保留兼容)
#define SAMPLEAPP_END_DEVICE_REJOIN_EVT 0x0002

// 配网中LED1慢闪事件 (1Hz闪烁, 入网成功或5分钟超时后停止)
#define SAMPLESW_PAIRING_BLINK_EVT      0x0020

// S1复位LED闪烁事件 (3次闪烁, 完成后执行复位)
#define SAMPLESW_RESET_BLINK_EVT        0x0040

// 断电记忆延迟写入NV事件 (5秒到期, 执行Compare-Before-Write)
#define SAMPLESW_NV_SAVE_EVT            0x0080

// End Device rejoin延迟
#define SAMPLEAPP_END_DEVICE_REJOIN_DELAY 10000

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * VARIABLES
 */

// EP11 Basic Cluster端点描述符
extern SimpleDescriptionFormat_t zclSampleSw_SimpleDesc;

// 4路继电器端点描述符数组
extern SimpleDescriptionFormat_t zclSampleSw_RelaySimpleDesc[SAMPLESW_NUM_RELAYS];

// 4路输入状态端点描述符数组
extern SimpleDescriptionFormat_t zclSampleSw_InputSimpleDesc[SAMPLESW_NUM_INPUTS];

// Basic Cluster属性表 (EP11)
extern CONST zclAttrRec_t zclSampleSw_Attrs[];

// 4路继电器属性表 (EP1~EP4)
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep1[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep2[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep3[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep4[];

// 4路输入状态属性表 (EP5~EP8)
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep5[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep6[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep7[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep8[];

// Basic Cluster属性数量
extern CONST uint8 zclSampleSw_NumAttributes;

// 4路继电器状态 (TRUE=ON, FALSE=OFF)
extern uint8 zclSampleSw_RelayState[SAMPLESW_NUM_RELAYS];

// 4路独立startUpOnOff配置 (断电恢复策略)
extern uint8 zclSampleSw_StartUpOnOff[SAMPLESW_NUM_RELAYS];

// 4路输入状态 (触摸: 1.0f=触摸中, 0.0f=未触摸)
extern float zclSampleSw_InputState[SAMPLESW_NUM_INPUTS];

// Identify时间
extern uint16 zclSampleSw_IdentifyTime;

// 4路继电器端点的ZCL命令回调表
extern zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep1;
extern zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep2;
extern zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep3;
extern zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep4;

// ZCL命令回调函数 (实现于zcl_samplesw.c, 由zcl_samplesw_data.c回调表引用)
extern void zclSampleSw_BasicResetCB(void);
extern void zclSampleSw_OnOffCB_ep1(uint8 cmd);
extern void zclSampleSw_OnOffCB_ep2(uint8 cmd);
extern void zclSampleSw_OnOffCB_ep3(uint8 cmd);
extern void zclSampleSw_OnOffCB_ep4(uint8 cmd);

/*********************************************************************
 * FUNCTIONS
 */

// 任务初始化
extern void zclSampleSw_Init( byte task_id );

// 任务事件处理
extern UINT16 zclSampleSw_event_loop( byte task_id, UINT16 events );

// 重置所有可写属性到默认值 (zcl_samplesw_data.c实现)
extern void zclSampleSw_ResetAttributesToDefaultValues(void);

// 应用层接口: 更新指定继电器输出 (含LED联动)
extern void zclSampleSw_UpdateRelayOutput(uint8 idx);

// 应用层接口: 更新所有继电器输出
extern void zclSampleSw_UpdateAllRelayOutputs(void);

// 应用层接口: 初始化GPIO方向与初始电平
extern void zclSampleSw_InitGpio(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_SAMPLESW_H */
