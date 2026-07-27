/**************************************************************************************************
  Filename:       zcl_samplesw.c
  Revised:        $Date: 2015-08-19 17:11:00 -0700 (Wed, 19 Aug 2015) $
  Revision:       $Revision: 44460 $

  Description:    Zigbee Cluster Library - sample switch application.


  Copyright 2006-2013 Texas Instruments Incorporated. All rights reserved.

  IMPORTANT: Your use of this Software is limited to those specific rights
  granted under the terms of a software license agreement between the user
  who downloaded the software, his/her employer (which must be your employer)
  and Texas Instruments Incorporated (the "License").  You may not use this
  Software unless you agree to abide by the terms of the License. The License
  limits your use, and you acknowledge, that the Software may not be modified,
  copied or distributed unless embedded on a Texas Instruments microcontroller
  or used solely and exclusively in conjunction with a Texas Instruments radio
  frequency transceiver, which is integrated into your product.  Other than for
  the foregoing purpose, you may not use, reproduce, copy, prepare derivative
  works of, modify, distribute, perform, display or sell this Software and/or
  its documentation for any purpose.

  YOU FURTHER ACKNOWLEDGE AND AGREE THAT THE SOFTWARE AND DOCUMENTATION ARE
  PROVIDED �AS IS� WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED,
  INCLUDING WITHOUT LIMITATION, ANY WARRANTY OF MERCHANTABILITY, TITLE,
  NON-INFRINGEMENT AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT SHALL
  TEXAS INSTRUMENTS OR ITS LICENSORS BE LIABLE OR OBLIGATED UNDER CONTRACT,
  NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION, BREACH OF WARRANTY, OR OTHER
  LEGAL EQUITABLE THEORY ANY DIRECT OR INDIRECT DAMAGES OR EXPENSES
  INCLUDING BUT NOT LIMITED TO ANY INCIDENTAL, SPECIAL, INDIRECT, PUNITIVE
  OR CONSEQUENTIAL DAMAGES, LOST PROFITS OR LOST DATA, COST OF PROCUREMENT
  OF SUBSTITUTE GOODS, TECHNOLOGY, SERVICES, OR ANY CLAIMS BY THIRD PARTIES
  (INCLUDING BUT NOT LIMITED TO ANY DEFENSE THEREOF), OR OTHER SIMILAR COSTS.

  Should you have any questions regarding your right to use this Software,
  contact Texas Instruments Incorporated at www.TI.com.
**************************************************************************************************/

/*********************************************************************
  This application implements a ZigBee On/Off Switch, based on Z-Stack 3.0.

  This application is based on the common sample-application user interface. Please see the main
  comment in zcl_sampleapp_ui.c. The rest of this comment describes only the content specific for
  this sample applicetion.
  
  Application-specific UI peripherals being used:

  - none (LED1 is currently unused by this application).

  Application-specific menu system:

    <TOGGLE LIGHT> Send an On, Off or Toggle command targeting appropriate devices from the binding table.
      Pressing / releasing [OK] will have the following functionality, depending on the value of the 
      zclSampleSw_OnOffSwitchActions attribute:
      - OnOffSwitchActions == 0: pressing [OK] will send ON command, releasing it will send OFF command;
      - OnOffSwitchActions == 1: pressing [OK] will send OFF command, releasing it will send ON command;
      - OnOffSwitchActions == 2: pressing [OK] will send TOGGLE command, releasing it will not send any command.

*********************************************************************/

#if ! defined ZCL_ON_OFF
#error ZCL_ON_OFF must be defined for this project.
#endif

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"
#include "OSAL.h"
#include "OSAL_Nv.h"
#include "AF.h"
#include "ZDApp.h"
#include "ZDObject.h"
#include "ZDProfile.h"
#include "MT_SYS.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_samplesw.h"
#include "zcl_diagnostic.h"

#include "onboard.h"

/* HAL */
#include "hal_lcd.h"
#include "hal_led.h"
#include "hal_key.h"

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
#include "zcl_ota.h"
#include "hal_ota.h"
#endif

#include "bdb.h"
#include "bdb_interface.h"

// v1.0.0重构: 移除 zcl_sampleapps_ui.h 依赖 (无LCD/无物理按键, UI模块剥离)

// 4路继电器端点属性数组 (定义在 zcl_samplesw_data.c)
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep1[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep2[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep3[];
extern CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep4[];
extern CONST uint8 ZCLSAMPLESW_NUM_RELAY_ATTRS;

// 4路输入状态端点属性数组 (定义在 zcl_samplesw_data.c)
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep5[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep6[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep7[];
extern CONST zclAttrRec_t zclSampleSw_InputAttrs_ep8[];
extern CONST uint8 ZCLSAMPLESW_NUM_INPUT_ATTRS;

/*********************************************************************
 * MACROS
 */
// v1.0.0重构: 移除 UI_STATE_TOGGLE_LIGHT 和 APP_TITLE (UI模块已剥离)

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */
byte zclSampleSw_TaskID;

uint8 zclSampleSwSeqNum;

uint8 zclSampleSw_OnOffSwitchType = ON_OFF_SWITCH_TYPE_MOMENTARY;

uint8 zclSampleSw_OnOffSwitchActions;

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * LOCAL VARIABLES
 */
afAddrType_t zclSampleSw_DstAddr;

// Endpoint to allow SYS_APP_MSGs
static endPointDesc_t sampleSw_TestEp =
{
  SAMPLESW_ENDPOINT,                  // endpoint
  0,
  &zclSampleSw_TaskID,
  (SimpleDescriptionFormat_t *)NULL,  // No Simple description for this test endpoint
  (afNetworkLatencyReq_t)0            // No Network Latency req
};

//static uint8 aProcessCmd[] = { 1, 0, 0, 0 }; // used for reset command, { length + cmd0 + cmd1 + data }

devStates_t zclSampleSw_NwkState = DEV_INIT;

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
#define DEVICE_POLL_RATE                 8000   // Poll rate for end device
#endif

#define SAMPLESW_TOGGLE_TEST_EVT       0x1000
// BUG-010修复: 移除30秒周期性上报(SAMPLESW_STATE_REPORT_EVT)
// 原因: 周期性上报会触发z2m的state_action选项, 生成无意义的action事件(每30秒4个action)
// 现方案: 仅在状态变化时(触摸/远程操作后)和入网后立即上报, 避免无操作时的action事件
// 状态同步保障: z2m availability检测 + 下次操作时的立即上报

/* ============================================================
 * 86四路智能开关硬件引脚映射 (参见 4路智能开关_Zigbee固件开发方案.md)
 *   继电器1~4 (低电平触发吸合): P1_0, P1_2, P1_6, P2_0
 *   LED1~4    (反逻辑, ACTIVE_LOW: 写0=亮, 写1=灭): P0_0~P0_3
 *   触摸1~4   (WTC6106BSI输出, 低电平=触摸中): P0_4~P0_7
 *   S1配网按键 (低电平有效): P1_3
 * LED与继电器联动: 继电器OFF→LED亮(写0), 继电器ON→LED灭(写1)
 * ============================================================ */
// v1.0.2优化: 触摸轮询周期 100ms→50ms, 触发延迟从~200ms降到~100ms (接近WTC6106BSI硬件响应极限)
// 抗干扰能力不变(仍需2次连续确认), CPU开销可忽略(4路GPIO读取极轻量)
#define TOUCH_POLL_INTERVAL_MS    50        // 触摸轮询周期
#define TOUCH_DEBOUNCE_COUNTS     2        // 连续2次(100ms)确认状态变化, 防抖

// S1配网按键长按复位 (P1_3, 低电平有效)
// v1.0.2调整: 轮询周期50ms后, 阈值从50→100, 保持5秒长按触发复位
#define S1_RESET_THRESHOLD        100      // 5秒 = 100 * 50ms轮询周期

// 继电器引脚位掩码 (P1口: P1_0/P1_2/P1_6, P2口: P2_0)
#define RELAY_P1_BV               (BV(0) | BV(2) | BV(6))
#define RELAY_P2_BV               (BV(0))
// 触摸输入引脚位掩码 (P0_4~P0_7)
#define TOUCH_INPUT_BV            (BV(4) | BV(5) | BV(6) | BV(7))

// 触摸防抖状态 (bit i 对应通道 i: 0=稳定未触摸, 1=稳定触摸中)
static uint8 touchStableState = 0;
static uint8 touchDebounce[4] = {0, 0, 0, 0};   // 各通道防抖计数器
static uint8 touchPending[4]  = {0, 0, 0, 0};   // 各通道待确认的新电平
// S1长按复位计数器 (每次触摸轮询+1, 达到S1_RESET_THRESHOLD执行复位)
static uint8 s1HoldCount = 0;

// BUG-011修复: S1复位LED闪烁状态机
// 用直接GPIO操作替代HalLedBlink, 避免HalLedState与实际硬件状态不一致
// 闪烁参数: 3次, 300ms亮/300ms灭, 共1.8秒
#define RESET_BLINK_TOTAL_COUNT   6    // 3次闪烁 = 6次状态切换(亮/灭/亮/灭/亮/灭)
#define RESET_BLINK_PERIOD_MS     300  // 每次亮或灭的持续时间
static uint8 resetBlinkCount = 0;      // 闪烁状态机计数器(0~6)

// v1.0.4新增: 配网中LED1慢闪状态机 (业界惯例, 提示用户正在配网)
// 启动时机: zclSampleSw_Init()末尾, 设备未入网时启动
// 停止时机: ZDO_STATE_CHANGE收到DEV_ROUTER(已入网) 或 超时(5分钟)
// 闪烁参数: 1Hz, 500ms亮/500ms灭, 仅操作LED1(P0_0), 不影响继电器状态
#define PAIRING_BLINK_PERIOD_MS   500  // 每次亮或灭的持续时间 (1Hz)
static uint8 pairingBlinkActive = FALSE;  // 慢闪是否活跃
static uint8 pairingBlinkLedOn = FALSE;   // 当前LED1是否亮(反逻辑: 0=亮)
static uint16 pairingBlinkTickCount = 0;  // 已闪烁的tick数(每500ms+1, 用于超时判断)
// 配网超时阈值: 5分钟 = 300秒 = 600个500ms tick
#define PAIRING_BLINK_TIMEOUT_TICKS   (SAMPLESW_PAIRING_TIMEOUT_MS / PAIRING_BLINK_PERIOD_MS)

// v1.0.6新增: LED软件PWM状态 (50%占空比, 降低LED亮度)
// ledTargetOn[i]=TRUE 表示期望LED亮(经PWM 50%导通), FALSE表示期望LED灭(全关)
// idx 0~3 对应 LED1~4 (P0_0~P0_3), 反逻辑: TRUE=亮(写0), FALSE=灭(写1)
static uint8 ledTargetOn[SAMPLESW_NUM_RELAYS] = {FALSE, FALSE, FALSE, FALSE};
static uint8 ledPwmCounter = 0;  // PWM计数器(0/1交替, 50%占空比)

// 断电记忆: startUpOnOff缓存值 (用于检测Z2M远程修改, 4路独立)
static uint8 startupOnOffCached[SAMPLESW_NUM_RELAYS] = {STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS};

/*********************************************************************
 * LOCAL FUNCTIONS
 */
static void zclSampleSw_HandleKeys( byte shift, byte keys );
static void zclSampleSw_BasicResetCB( void );

static void zclSampleSw_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);

// 86开关: 继电器/触摸/OnOff回调内部实现
static void zclSampleSw_HandleOnOffCmd(uint8 idx, uint8 cmd);
// BUG-011修复: S1复位闪烁状态机 (替代HalLedBlink)
static void zclSampleSw_StartResetBlink(void);
static void zclSampleSw_ProcessResetBlink(void);
// v1.0.4新增: 配网中LED1慢闪状态机
static void zclSampleSw_StartPairingBlink(void);
static void zclSampleSw_ProcessPairingBlink(void);
static void zclSampleSw_StopPairingBlink(void);
// v1.0.6新增: LED软件PWM层 (50%占空比, 替代直接P0_x写入)
static void zclSampleSw_LedWriteGpio(uint8 idx, uint8 on);
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on);
static void zclSampleSw_LedPwmApply(void);
static void zclSampleSw_ToggleRelay(uint8 idx);
static uint8 zclSampleSw_ReadTouchInputs(void);
static void zclSampleSw_ReportOnOffState(uint8 idx);
static void zclSampleSw_ReportAllOnOffState(void);
static void zclSampleSw_ReportInputState(uint8 idx);

// 断电记忆: NV存储相关函数
static void zclSampleSw_NvInit(void);
static void zclSampleSw_NvLoadPowerOnState(void);
static void zclSampleSw_NvScheduleSave(void);
static void zclSampleSw_NvProcessSave(void);

// v1.0.0重构: 移除 zclSampleSw_UiActionToggleLight/zclSampleSw_UiUpdateLcd 声明 (UI模块已剥离)


// Functions to process ZCL Foundation incoming Command/Response messages
static void zclSampleSw_ProcessIncomingMsg( zclIncomingMsg_t *msg );
#ifdef ZCL_READ
static uint8 zclSampleSw_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg );
#endif
#ifdef ZCL_WRITE
static uint8 zclSampleSw_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg );
#endif
static uint8 zclSampleSw_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg );
#ifdef ZCL_DISCOVER
static uint8 zclSampleSw_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclSampleSw_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg );
static uint8 zclSampleSw_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg );
#endif

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
static void zclSampleSw_ProcessOTAMsgs( zclOTA_CallbackMsg_t* pMsg );
#endif

static void zclSampleApp_BatteryWarningCB( uint8 voltLevel);
// v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)


/*********************************************************************
 * CONSTANTS
 */
  // v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
  // 已移除: const uiState_t zclSampleSw_UiStatesMain[] 状态表
  
/*********************************************************************
 * REFERENCED EXTERNALS
 */
extern int16 zdpExternalStateTaskID;

/*********************************************************************
 * ZCL General Profile Callback table
 */
static zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks =
{
  zclSampleSw_BasicResetCB,               // Basic Cluster Reset command
  NULL,                                   // Identify Trigger Effect command
  NULL,                                   // On/Off cluster commands
  NULL,                                   // On/Off cluster enhanced command Off with Effect
  NULL,                                   // On/Off cluster enhanced command On with Recall Global Scene
  NULL,                                   // On/Off cluster enhanced command On with Timed Off
#ifdef ZCL_LEVEL_CTRL
  NULL,                                   // Level Control Move to Level command
  NULL,                                   // Level Control Move command
  NULL,                                   // Level Control Step command
  NULL,                                   // Level Control Stop command
#endif
#ifdef ZCL_GROUPS
  NULL,                                   // Group Response commands
#endif
#ifdef ZCL_SCENES
  NULL,                                   // Scene Store Request command
  NULL,                                   // Scene Recall Request command
  NULL,                                   // Scene Response command
#endif
#ifdef ZCL_ALARMS
  NULL,                                   // Alarm (Response) commands
#endif
#ifdef SE_UK_EXT
  NULL,                                   // Get Event Log command
  NULL,                                   // Publish Event Log command
#endif
  NULL,                                   // RSSI Location command
  NULL                                    // RSSI Location Response command
};

/* 86开关: 为4路继电器端点(EP1~EP4)生成独立的OnOff回调与callbacks表。
 * ZCL按endpoint查找callbacks后调用pfnOnOff(cmd), 签名不传endpoint,
 * 故每个端点需独立的回调函数以记录目标继电器索引。
 * 字段顺序须与上面 zclSampleSw_CmdCallbacks 一致 (当前项目定义:
 * ZCL_BASIC/ZCL_IDENTIFY/ZCL_ON_OFF/ZCL_GROUPS, 未定义 ZCL_LEVEL_CTRL/
 * ZCL_SCENES/ZCL_ALARMS/SE_UK_EXT, 故结构体共9个字段)。 */
#define DEFINE_RELAY_ONOFF_CB(IDX, EP)                                  \
  static void zclSampleSw_OnOffCB_ep##EP(uint8 cmd)                     \
  {                                                                      \
    zclSampleSw_HandleOnOffCmd((IDX), cmd);                              \
  }                                                                      \
  static zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep##EP =    \
  {                                                                      \
    zclSampleSw_BasicResetCB,   /* pfnBasicReset */                     \
    NULL,                       /* pfnIdentifyTriggerEffect */          \
    zclSampleSw_OnOffCB_ep##EP, /* pfnOnOff */                         \
    NULL,                       /* pfnOnOff_OffWithEffect */            \
    NULL,                       /* pfnOnOff_OnWithRecallGlobalScene */ \
    NULL,                       /* pfnOnOff_OnWithTimedOff */          \
    NULL,                       /* pfnGroupRsp (ZCL_GROUPS) */          \
    NULL,                       /* pfnLocation */                       \
    NULL                        /* pfnLocationRsp */                   \
  };

DEFINE_RELAY_ONOFF_CB(0, 1)
DEFINE_RELAY_ONOFF_CB(1, 2)
DEFINE_RELAY_ONOFF_CB(2, 3)
DEFINE_RELAY_ONOFF_CB(3, 4)

/*********************************************************************
 * @fn          zclSampleSw_Init
 *
 * @brief       Initialization function for the zclGeneral layer.
 *
 * @param       none
 *
 * @return      none
 */
void zclSampleSw_Init( byte task_id )
{
  zclSampleSw_TaskID = task_id;

  // Set destination address to indirect
  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)AddrNotPresent;
  zclSampleSw_DstAddr.endPoint = 0;
  zclSampleSw_DstAddr.addr.shortAddr = 0;

  // Register the Simple Descriptor for this application
  bdb_RegisterSimpleDescriptor( &zclSampleSw_SimpleDesc );

  // Register the ZCL General Cluster Library callback functions
  zclGeneral_RegisterCmdCallbacks( SAMPLESW_ENDPOINT, &zclSampleSw_CmdCallbacks );

  zclSampleSw_ResetAttributesToDefaultValues();
  
  // Register the application's attribute list
  zcl_registerAttrList( SAMPLESW_ENDPOINT, zclSampleSw_NumAttributes, zclSampleSw_Attrs );

  // Register the Application to receive the unprocessed Foundation command/response messages
  zcl_registerForMsg( zclSampleSw_TaskID );
  
  // Register low voltage NV memory protection application callback
  RegisterVoltageWarningCB( zclSampleApp_BatteryWarningCB );

  // Register for all key events - This app will handle all key events
  RegisterForKeys( zclSampleSw_TaskID );
  
  bdb_RegisterCommissioningStatusCB( zclSampleSw_ProcessCommissioningStatus );

  // Register for a test endpoint
  afRegister( &sampleSw_TestEp );

  // 注册4路继电器端点 (EP 1-4, 对应 alab.switch l1-l4)
  // 每个端点注册独立的callbacks, 使OnOff命令能定位到正确的继电器索引
  {
    uint8 ep;
    static zclGeneral_AppCallbacks_t* relayCBs[SAMPLESW_NUM_RELAYS] = {
      &zclSampleSw_CmdCallbacks_ep1,
      &zclSampleSw_CmdCallbacks_ep2,
      &zclSampleSw_CmdCallbacks_ep3,
      &zclSampleSw_CmdCallbacks_ep4,
    };
    for (ep = 0; ep < SAMPLESW_NUM_RELAYS; ep++)
    {
      bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[ep]);
      zclGeneral_RegisterCmdCallbacks(zclSampleSw_RelaySimpleDesc[ep].EndPoint, relayCBs[ep]);
    }
    zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY1, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep1);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY2, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep2);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY3, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep3);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY4, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep4);
  }

  // 注册4路输入状态端点 (EP 5-8, 对应 alab.switch in1-in4)
  {
    uint8 ep;
    for (ep = 0; ep < SAMPLESW_NUM_INPUTS; ep++)
    {
      bdb_RegisterSimpleDescriptor(&zclSampleSw_InputSimpleDesc[ep]);
    }
    zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT1, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep5);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT2, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep6);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT3, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep7);
    zcl_registerAttrList(SAMPLESW_ENDPOINT_INPUT4, ZCLSAMPLESW_NUM_INPUT_ATTRS, zclSampleSw_InputAttrs_ep8);
  }

  // 断电记忆: 初始化NV项, 并按startUpOnOff策略恢复断电前继电器状态
  // 必须在UpdateAllRelayOutputs()之前调用, 确保GPIO输出与恢复后的状态一致
  zclSampleSw_NvInit();
  zclSampleSw_NvLoadPowerOnState();

  // 86开关: 初始化继电器/LED/触摸GPIO, 并根据zclSampleSw_RelayState应用初始输出
  zclSampleSw_InitGpio();
  zclSampleSw_UpdateAllRelayOutputs();

  // 86开关: 启动触摸输入轮询 (50ms周期, 内含软件防抖)
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT, TOUCH_POLL_INTERVAL_MS);

  // v1.0.6: 启动LED软件PWM定时器 (100Hz, 50%占空比, 降低LED亮度)
  // 必须在UpdateAllRelayOutputs之后启动, 确保ledTargetOn已初始化
  // 注: 2ms(500Hz)会过载OSAL干扰协议栈MAC时序, 10ms(100Hz)为安全上限
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_LED_PWM_EVT, SAMPLESW_LED_PWM_PERIOD_MS);
  
#ifdef ZCL_DIAGNOSTIC
  // Register the application's callback function to read/write attribute data.
  // This is only required when the attribute data format is unknown to ZCL.
  zcl_registerReadWriteCB( SAMPLESW_ENDPOINT, zclDiagnostic_ReadWriteAttrCB, NULL );

  if ( zclDiagnostic_InitStats() == ZSuccess )
  {
    // Here the user could start the timer to save Diagnostics to NV
  }
#endif

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
  // Register for callback events from the ZCL OTA
  zclOTA_Register(zclSampleSw_TaskID);
#endif

  zdpExternalStateTaskID = zclSampleSw_TaskID;

  // v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
  // 已移除: UI_Init(...) 与 UI_UpdateLcd()

  // v1.0.5新增: 设置CC2530发射功率为 4 dBm (TX_PWR_PLUS_4)
  // 原因: MAC PIB 默认 phyTransmitPower=0 (0 dBm, 1mW), 偏低导致信号不稳定
  // 采用 4 dBm (约2.5mW), 与 TI 官方 ZNP 项目默认值一致 (znp_app.c:411)
  // 注: CC2530裸片 datasheet 标称最大 7 dBm, 但 7 dBm (0xFF寄存器值) 在某些模块上
  //     会导致 RF 工作不稳定 (信号经常归零), 4 dBm 是稳定性与功率的最佳平衡点
  // 功率表参考: Components/mac/low_level/srf05/single_chip/mac_radio_defs.c
  // 必须在 bdb_StartCommissioning() 之前调用, 确保入网时即使用设置后的发射功率
  ZMacSetTransmitPower(TX_PWR_PLUS_4);

  // v1.0.0修复: UI模块剥离后, 需显式触发BDB commissioning启动Zigbee网络
  // 原本由 UI_Init() 内部调用 bdb_StartCommissioning(), 移除UI后应用层需自行启动
  // 参数 0x00 = BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP
  //   - 已配网设备: 尝试rejoin恢复网络
  //   - 新设备: 触发BDB initialization后启动NWK_STEERING commissioning
  bdb_StartCommissioning(BDB_COMMISSIONING_REJOIN_EXISTING_NETWORK_ON_STARTUP);

  // v1.0.4新增: 启动配网中LED1慢闪, 提示用户设备正在配网
  // 已配网设备: BDB rejoin成功后ZDO_STATE_CHANGE会触发StopPairingBlink
  // 新设备: 持续慢闪直到入网成功或5分钟超时
  zclSampleSw_StartPairingBlink();
}

/*********************************************************************
 * @fn          zclSample_event_loop
 *
 * @brief       Event Loop Processor for zclGeneral.
 *
 * @param       none
 *
 * @return      none
 */
uint16 zclSampleSw_event_loop( uint8 task_id, uint16 events )
{
  afIncomingMSGPacket_t *MSGpkt;
  (void)task_id;  // Intentionally unreferenced parameter

  //Send toggle every 500ms
  if( events & SAMPLESW_TOGGLE_TEST_EVT )
  {
    osal_start_timerEx(zclSampleSw_TaskID,SAMPLESW_TOGGLE_TEST_EVT,500);
    zclGeneral_SendOnOff_CmdToggle( SAMPLESW_ENDPOINT, &zclSampleSw_DstAddr, FALSE, 0 );
    
    // return unprocessed events
    return (events ^ SAMPLESW_TOGGLE_TEST_EVT);
  }
  
  
  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclSampleSw_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZCL_INCOMING_MSG:
          // Incoming ZCL Foundation command/response messages
          zclSampleSw_ProcessIncomingMsg( (zclIncomingMsg_t *)MSGpkt );
          break;

        case KEY_CHANGE:
          zclSampleSw_HandleKeys( ((keyChange_t *)MSGpkt)->state, ((keyChange_t *)MSGpkt)->keys );
          break;

        case ZDO_STATE_CHANGE:
          // v1.0.0重构: 移除 UI_DeviceStateUpdated (UI模块已剥离)
          // 86开关: 协议栈Router启动时会操作LED3/LED4(ZDApp.c), 覆盖继电器状态灯。
          // 入网状态变化后重新刷新所有继电器/LED输出, 恢复正确显示。
          zclSampleSw_UpdateAllRelayOutputs();
          // 状态同步: 入网成功(DEV_ROUTER)后立即上报当前OnOff状态, 修复断电恢复后Z2M状态不同步(BUG-007)
          // BUG-010修复: 移除30秒周期性上报定时器, 避免无操作时触发z2m state_action生成无意义action事件
          // 状态同步改为依赖: 1)入网后立即上报 2)触摸/远程操作后立即上报 3)z2m availability检测
          if ((devStates_t)(MSGpkt->hdr.status) == DEV_ROUTER && zclSampleSw_NwkState != DEV_ROUTER)
          {
            // v1.0.4新增: 入网成功, 停止配网中LED1慢闪
            zclSampleSw_StopPairingBlink();
            zclSampleSw_ReportAllOnOffState();
          }
          zclSampleSw_NwkState = (devStates_t)(MSGpkt->hdr.status);
          break;

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
        case ZCL_OTA_CALLBACK_IND:
          zclSampleSw_ProcessOTAMsgs( (zclOTA_CallbackMsg_t*)MSGpkt  );
          break;
#endif

        default:
          break;
      }

      // Release the memory
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }

    // return unprocessed events
    return (events ^ SYS_EVENT_MSG);
  }

#if ZG_BUILD_ENDDEVICE_TYPE    
  if ( events & SAMPLEAPP_END_DEVICE_REJOIN_EVT )
  {
    bdb_ZedAttemptRecoverNwk();
    return ( events ^ SAMPLEAPP_END_DEVICE_REJOIN_EVT );
  }
#endif

  // v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
  // 已移除: SAMPLEAPP_LCD_AUTO_UPDATE_EVT 事件处理块 (含 UI_UpdateLcd 调用)
  // 已移除: SAMPLEAPP_KEY_AUTO_REPEAT_EVT 事件处理块 (含 UI_MainStateMachine 调用)

  // 86开关: 触摸输入轮询事件 (50ms周期, 内含软件防抖)
  if ( events & SAMPLESW_TOUCH_POLL_EVT )
  {
    zclSampleSw_ProcessTouchPoll();
    return ( events ^ SAMPLESW_TOUCH_POLL_EVT );
  }

  // 断电记忆: 延迟写入NV事件 (5秒到期, 执行Compare-Before-Write)
  if ( events & SAMPLESW_NV_SAVE_EVT )
  {
    zclSampleSw_NvProcessSave();
    return ( events ^ SAMPLESW_NV_SAVE_EVT );
  }

  // BUG-011修复: S1复位LED闪烁事件处理
  // 用直接GPIO操作替代HalLedBlink, 避免HalLedState与硬件状态不一致
  if ( events & SAMPLESW_RESET_BLINK_EVT )
  {
    zclSampleSw_ProcessResetBlink();
    return ( events ^ SAMPLESW_RESET_BLINK_EVT );
  }

  // v1.0.4新增: 配网中LED1慢闪事件处理
  // 1Hz闪烁(500ms亮/500ms灭), 入网成功或超时(5分钟)后停止
  if ( events & SAMPLESW_PAIRING_BLINK_EVT )
  {
    zclSampleSw_ProcessPairingBlink();
    return ( events ^ SAMPLESW_PAIRING_BLINK_EVT );
  }

  // v1.0.6新增: LED软件PWM事件处理 (500Hz, 50%占空比, 降低LED亮度)
  // 持续运行, 每2ms应用一次PWM, 期望亮的LED以50%占空比导通
  if ( events & SAMPLESW_LED_PWM_EVT )
  {
    zclSampleSw_LedPwmApply();
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_LED_PWM_EVT, SAMPLESW_LED_PWM_PERIOD_MS);
    return ( events ^ SAMPLESW_LED_PWM_EVT );
  }

  // BUG-010修复: 移除SAMPLESW_STATE_REPORT_EVT周期性上报事件处理
  // 原因: 30秒周期性上报会触发z2m state_action, 生成无意义action事件
  // 状态同步改为依赖: 入网后立即上报 + 操作后立即上报 + z2m availability检测

  // Discard unknown events
  return 0;
}

/*********************************************************************
 * @fn      zclSampleSw_HandleKeys
 *
 * @brief   Handles all key events for this device.
 *
 * @param   shift - true if in shift/alt.
 * @param   keys - bit field for key events. Valid entries:
 *                 HAL_KEY_SW_5
 *                 HAL_KEY_SW_4
 *                 HAL_KEY_SW_2
 *                 HAL_KEY_SW_1
 *
 * @return  none
 */
static void zclSampleSw_HandleKeys( byte shift, byte keys )
{
  // v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
  // 已移除: UI_MainStateMachine(keys);
  (void)shift;  // Intentionally unreferenced parameter
  (void)keys;   // Intentionally unreferenced parameter
}

/* ============================================================
 * 86四路智能开关: 继电器/LED/触摸输入实现
 * ============================================================ */

/*********************************************************************
 * @fn      zclSampleSw_InitGpio
 *
 * @brief   初始化继电器/LED/触摸引脚的GPIO方向与初始电平
 *          继电器(P1_0/P1_2/P1_6/P2_0): 输出, 默认高电平(继电器断开)
 *          LED(P0_0~P0_3): 已由HalLedInit配置为输出, 这里同步为继电器状态
 *          触摸(P0_4~P0_7): 输入, 上拉(CC2530 P0口默认上拉)
 * @return  none
 */
void zclSampleSw_InitGpio(void)
{
  // 继电器引脚设为GPIO功能并配置为输出
  P1SEL &= ~RELAY_P1_BV;        // P1_0/P1_2/P1_6 选为GPIO
  P2SEL &= ~RELAY_P2_BV;        // P2_0 选为GPIO
  P1DIR |= RELAY_P1_BV;         // 设为输出
  P2DIR |= RELAY_P2_BV;         // 设为输出

  // 触摸输入引脚设为GPIO功能并配置为输入(默认上拉)
  // WTC6106BSI: 未触摸=高电平, 触摸=低电平(开漏输出, 需上拉)
  P0SEL &= ~TOUCH_INPUT_BV;     // P0_4~P0_7 选为GPIO
  P0DIR &= ~TOUCH_INPUT_BV;     // 设为输入

  touchStableState = 0;
}

/*********************************************************************
 * @fn      zclSampleSw_LedWriteGpio
 *
 * @brief   v1.0.6新增: LED底层GPIO写入 (反逻辑: on=TRUE→写0亮, on=FALSE→写1灭)
 * @param   idx - LED索引 0~3 (LED1~4 → P0_0~P0_3)
 * @param   on  - TRUE=亮, FALSE=灭
 * @return  none
 */
static void zclSampleSw_LedWriteGpio(uint8 idx, uint8 on)
{
  uint8 val = on ? 0 : 1;   // 反逻辑: 亮=0, 灭=1
  switch (idx)
  {
    case 0: P0_0 = val; break;
    case 1: P0_1 = val; break;
    case 2: P0_2 = val; break;
    case 3: P0_3 = val; break;
    default: break;
  }
}

/*********************************************************************
 * @fn      zclSampleSw_LedSetTarget
 *
 * @brief   v1.0.6新增: 设置LED期望亮灭状态 (替代直接P0_x=val)
 *          所有LED控制点(继电器联动/配网慢闪/复位闪烁)统一调用本函数
 * @param   idx - LED索引 0~3
 * @param   on  - TRUE=期望亮(经PWM 50%导通), FALSE=期望灭(全关)
 * @return  none
 */
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on)
{
  ledTargetOn[idx] = on;
  // 立即应用一次, 避免等待下个PWM周期(最多2ms延迟)
  zclSampleSw_LedPwmApply();
}

/*********************************************************************
 * @fn      zclSampleSw_LedPwmApply
 *
 * @brief   v1.0.6新增: LED软件PWM应用 (由SAMPLESW_LED_PWM_EVT周期调用)
 *          期望亮: 50%占空比(ledPwmCounter==0亮, ==1灭)
 *          期望灭: 全关
 *          ledPwmCounter 0/1交替, 形成500Hz/50%占空比PWM
 * @return  none
 */
static void zclSampleSw_LedPwmApply(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    if (ledTargetOn[i])
    {
      // 期望亮: 50%占空比 (counter==0导通, ==1关断)
      zclSampleSw_LedWriteGpio(i, (ledPwmCounter == 0));
    }
    else
    {
      // 期望灭: 全关
      zclSampleSw_LedWriteGpio(i, FALSE);
    }
  }
  ledPwmCounter ^= 1;   // 0/1交替
}

/*********************************************************************
 * @fn      zclSampleSw_UpdateRelayOutput
 *
 * @brief   根据zclSampleSw_RelayState[idx]更新指定通道的继电器GPIO和LED状态
 *          继电器: ON=低电平触发吸合, OFF=高电平断开
 *          LED反逻辑: 继电器OFF→LED亮, 继电器ON→LED灭 (经PWM 50%亮度)
 * @param   idx - 继电器索引 0~3
 * @return  none
 */
void zclSampleSw_UpdateRelayOutput(uint8 idx)
{
  uint8 on = zclSampleSw_RelayState[idx];

  switch (idx)
  {
    case 0:
      P1_0 = on ? 0 : 1;        // 继电器1: ON=低电平
      // v1.0.4: 配网中LED1慢闪激活时, 不刷新LED1, 由慢闪状态机控制
      if (!pairingBlinkActive)
      {
        zclSampleSw_LedSetTarget(0, !on);   // LED1: 反逻辑 OFF→亮, ON→灭
      }
      break;
    case 1:
      P1_2 = on ? 0 : 1;        // 继电器2
      zclSampleSw_LedSetTarget(1, !on);     // LED2
      break;
    case 2:
      P1_6 = on ? 0 : 1;        // 继电器3
      zclSampleSw_LedSetTarget(2, !on);     // LED3
      break;
    case 3:
      P2_0 = on ? 0 : 1;        // 继电器4
      zclSampleSw_LedSetTarget(3, !on);     // LED4
      break;
    default:
      break;
  }
}

/*********************************************************************
 * @fn      zclSampleSw_UpdateAllRelayOutputs
 * @brief   更新所有4路继电器的GPIO和LED输出
 * @return  none
 */
void zclSampleSw_UpdateAllRelayOutputs(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_UpdateRelayOutput(i);
  }
}

/* ============================================================
 * 断电记忆 (Power-On State Recovery) 实现
 *
 * 业界Flash寿命优化标准做法:
 *   1. 延迟写入 (Write Coalescing): 状态变化后不立即写Flash,
 *      启动定时器(5秒), 期间若有新变化则继续等待, 到期后一次性写入,
 *      把多次变化合并为1次Flash写操作。
 *   2. 对比写入 (Compare-Before-Write): 写入前对比RAM与NV中的值,
 *      相同则跳过Flash写, 避免无意义擦写。
 *   3. Z-Stack OSAL NV驱动内部已实现磨损均衡与冗余页机制。
 *
 * 数据布局:
 *   SAMPLESW_NV_ID_RELAY_STATE   (0x0F10): 4字节, 4路继电器状态
 *   SAMPLESW_NV_ID_STARTUP_ONOFF (0x0F12): 4字节, 4路独立startUpOnOff配置 (v0.2.2起)
 *
 * 开关控制: Z2M通过读写startUpOnOff属性(0x4003)控制断电记忆行为
 *   0x00=上电OFF, 0x01=上电ON, 0x02=上电TOGGLE, 0xFF=恢复断电前状态
 * ============================================================ */

/*********************************************************************
 * @fn      zclSampleSw_NvInit
 * @brief   初始化断电记忆NV存储项 (若不存在则用默认值创建)
 * @return  none
 */
static void zclSampleSw_NvInit(void)
{
  uint8 defaultRelayState[SAMPLESW_NUM_RELAYS] = {FALSE, FALSE, FALSE, FALSE};
  uint8 defaultStartupOnOff[SAMPLESW_NUM_RELAYS] = {STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS};

  // osal_nv_item_init: 若NV项已存在则不做改动, 不存在则用默认值创建
  osal_nv_item_init(SAMPLESW_NV_ID_RELAY_STATE, SAMPLESW_NUM_RELAYS, defaultRelayState);
  osal_nv_item_init(SAMPLESW_NV_ID_STARTUP_ONOFF, SAMPLESW_NUM_RELAYS, defaultStartupOnOff);
}

/*********************************************************************
 * @fn      zclSampleSw_NvLoadPowerOnState
 * @brief   上电时从NV读取配置和断电前状态, 按4路独立startUpOnOff策略恢复继电器
 *          必须在zclSampleSw_UpdateAllRelayOutputs()之前调用
 * @return  none
 */
static void zclSampleSw_NvLoadPowerOnState(void)
{
  uint8 savedRelayState[SAMPLESW_NUM_RELAYS];
  uint8 i;

  // 读取4路独立startUpOnOff配置
  if (osal_nv_read(SAMPLESW_NV_ID_STARTUP_ONOFF, 0, SAMPLESW_NUM_RELAYS, zclSampleSw_StartUpOnOff) != SUCCESS)
  {
    // NV读取失败, 保持编译时默认值
    for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
      zclSampleSw_StartUpOnOff[i] = STARTUP_ONOFF_PREVIOUS;
  }
  osal_memcpy(startupOnOffCached, zclSampleSw_StartUpOnOff, SAMPLESW_NUM_RELAYS);

  // 读取断电前继电器状态
  if (osal_nv_read(SAMPLESW_NV_ID_RELAY_STATE, 0, SAMPLESW_NUM_RELAYS, savedRelayState) != SUCCESS)
  {
    // NV读取失败, 保持全OFF默认值
    return;
  }

  // 按每路独立startUpOnOff策略设置上电继电器状态 (BUG-009修复: 4路独立配置)
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    switch (zclSampleSw_StartUpOnOff[i])
    {
      case STARTUP_ONOFF_OFF:
        zclSampleSw_RelayState[i] = FALSE;
        break;
      case STARTUP_ONOFF_ON:
        zclSampleSw_RelayState[i] = TRUE;
        break;
      case STARTUP_ONOFF_TOGGLE:
        zclSampleSw_RelayState[i] = !savedRelayState[i];
        break;
      case STARTUP_ONOFF_PREVIOUS:
      default:
        zclSampleSw_RelayState[i] = savedRelayState[i];
        break;
    }
  }
}

/*********************************************************************
 * @fn      zclSampleSw_NvScheduleSave
 * @brief   调度延迟写入NV (5秒后执行, 期间新调用会重置定时器)
 *          实现Write Coalescing: 多次状态变化合并为1次Flash写
 * @return  none
 */
static void zclSampleSw_NvScheduleSave(void)
{
  // 启动/重启延迟写入定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_NV_SAVE_EVT, SAMPLESW_NV_SAVE_DELAY_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_NvProcessSave
 * @brief   延迟写入定时器到期处理: 对比RAM与NV值, 不同才写入 (Compare-Before-Write)
 * @return  none
 */
static void zclSampleSw_NvProcessSave(void)
{
  uint8 savedRelayState[SAMPLESW_NUM_RELAYS];
  uint8 savedStartupOnOff[SAMPLESW_NUM_RELAYS];

  // 1. 对比并写入继电器状态
  if (osal_nv_read(SAMPLESW_NV_ID_RELAY_STATE, 0, SAMPLESW_NUM_RELAYS, savedRelayState) == SUCCESS)
  {
    if (osal_memcmp(savedRelayState, zclSampleSw_RelayState, SAMPLESW_NUM_RELAYS) == FALSE)
    {
      osal_nv_write(SAMPLESW_NV_ID_RELAY_STATE, 0, SAMPLESW_NUM_RELAYS, zclSampleSw_RelayState);
    }
  }
  else
  {
    // NV项不存在, 直接写入
    osal_nv_write(SAMPLESW_NV_ID_RELAY_STATE, 0, SAMPLESW_NUM_RELAYS, zclSampleSw_RelayState);
  }

  // 2. 对比并写入4路独立startUpOnOff配置
  if (osal_nv_read(SAMPLESW_NV_ID_STARTUP_ONOFF, 0, SAMPLESW_NUM_RELAYS, savedStartupOnOff) == SUCCESS)
  {
    if (osal_memcmp(savedStartupOnOff, zclSampleSw_StartUpOnOff, SAMPLESW_NUM_RELAYS) == FALSE)
    {
      osal_nv_write(SAMPLESW_NV_ID_STARTUP_ONOFF, 0, SAMPLESW_NUM_RELAYS, zclSampleSw_StartUpOnOff);
    }
  }
  else
  {
    osal_nv_write(SAMPLESW_NV_ID_STARTUP_ONOFF, 0, SAMPLESW_NUM_RELAYS, zclSampleSw_StartUpOnOff);
  }
}

/*********************************************************************
 * @fn      zclSampleSw_ToggleRelay
 * @brief   翻转指定通道继电器状态并更新GPIO输出, 并向协调器上报新状态
 * @param   idx - 继电器索引 0~3
 * @return  none
 */
static void zclSampleSw_ToggleRelay(uint8 idx)
{
  zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx];
  zclSampleSw_UpdateRelayOutput(idx);
  zclSampleSw_ReportOnOffState(idx);
  // 断电记忆: 调度延迟写入 (5秒后合并写入NV)
  zclSampleSw_NvScheduleSave();
}

/*********************************************************************
 * @fn      zclSampleSw_HandleOnOffCmd
 * @brief   处理ZCL On/Off/Toggle命令, 更新继电器状态和GPIO
 * @param   idx - 继电器索引 0~3
 * @param   cmd - ZCL命令ID (COMMAND_ON / COMMAND_OFF / COMMAND_TOGGLE)
 * @return  none
 */
static void zclSampleSw_HandleOnOffCmd(uint8 idx, uint8 cmd)
{
  switch (cmd)
  {
    case COMMAND_ON:
      zclSampleSw_RelayState[idx] = TRUE;
      break;
    case COMMAND_OFF:
      zclSampleSw_RelayState[idx] = FALSE;
      break;
    case COMMAND_TOGGLE:
      zclSampleSw_RelayState[idx] = !zclSampleSw_RelayState[idx];
      break;
    default:
      return;
  }
  zclSampleSw_UpdateRelayOutput(idx);
  zclSampleSw_ReportOnOffState(idx);
  // 断电记忆: 调度延迟写入 (5秒后合并写入NV)
  zclSampleSw_NvScheduleSave();
}

/*********************************************************************
 * @fn      zclSampleSw_ReportOnOffState
 * @brief   向协调器上报指定通道继电器的OnOff属性状态(ZCL Report Attributes)
 *          触摸翻转或ZCL命令处理后调用, 确保z2m状态同步
 * @param   idx - 继电器索引 0~3
 * @return  none
 */
static void zclSampleSw_ReportOnOffState(uint8 idx)
{
  uint8 ep;
  zclReportCmd_t *reportCmd;
  zclReport_t *reportRec;

  switch (idx)
  {
    case 0:  ep = SAMPLESW_ENDPOINT_RELAY1; break;
    case 1:  ep = SAMPLESW_ENDPOINT_RELAY2; break;
    case 2:  ep = SAMPLESW_ENDPOINT_RELAY3; break;
    case 3:  ep = SAMPLESW_ENDPOINT_RELAY4; break;
    default: return;
  }

  reportCmd = (zclReportCmd_t *)osal_msg_allocate(sizeof(zclReportCmd_t) + sizeof(zclReport_t));
  if (reportCmd == NULL) return;

  reportCmd->numAttr = 1;
  reportRec = &(reportCmd->attrList[0]);
  reportRec->attrID = ATTRID_ON_OFF;
  reportRec->dataType = ZCL_DATATYPE_BOOLEAN;
  reportRec->attrData = &zclSampleSw_RelayState[idx];

  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  zclSampleSw_DstAddr.addr.shortAddr = 0;  // 协调器
  zclSampleSw_DstAddr.endPoint = 1;

  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ON_OFF,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);

  osal_msg_deallocate((uint8 *)reportCmd);
}

/*********************************************************************
 * @fn      zclSampleSw_ReportAllOnOffState
 * @brief   向协调器上报所有4路继电器的OnOff状态
 *          用于入网后立即同步状态(BUG-007)和周期性状态同步(BUG-008),
 *          确保Z2M状态与设备实际状态一致, 即使某次Report丢失也能在下次周期恢复。
 * @return  none
 */
static void zclSampleSw_ReportAllOnOffState(void)
{
  uint8 i;
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_ReportOnOffState(i);
  }
}

/*********************************************************************
 * @fn      zclSampleSw_ReportInputState
 * @brief   向协调器上报指定通道的输入状态(genAnalogInput.presentValue)
 *          触摸状态变化时调用, 使z2m的input_state_inX同步
 * @param   idx - 输入索引 0~3
 * @return  none
 */
static void zclSampleSw_ReportInputState(uint8 idx)
{
  uint8 ep;
  zclReportCmd_t *reportCmd;
  zclReport_t *reportRec;

  switch (idx)
  {
    case 0:  ep = SAMPLESW_ENDPOINT_INPUT1; break;
    case 1:  ep = SAMPLESW_ENDPOINT_INPUT2; break;
    case 2:  ep = SAMPLESW_ENDPOINT_INPUT3; break;
    case 3:  ep = SAMPLESW_ENDPOINT_INPUT4; break;
    default: return;
  }

  reportCmd = (zclReportCmd_t *)osal_msg_allocate(sizeof(zclReportCmd_t) + sizeof(zclReport_t));
  if (reportCmd == NULL) return;

  reportCmd->numAttr = 1;
  reportRec = &(reportCmd->attrList[0]);
  reportRec->attrID = ATTRID_IOV_BASIC_PRESENT_VALUE;
  reportRec->dataType = ZCL_DATATYPE_SINGLE_PREC;
  reportRec->attrData = (uint8 *)&zclSampleSw_InputState[idx];

  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  zclSampleSw_DstAddr.addr.shortAddr = 0;  // 协调器
  zclSampleSw_DstAddr.endPoint = 1;

  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);

  osal_msg_deallocate((uint8 *)reportCmd);
}

/*********************************************************************
 * @fn      zclSampleSw_ReadTouchInputs
 * @brief   读取P0_4~P0_7触摸输入, 低电平=触摸中(WTC6106BSI输出极性固定)
 *          经诊断固件验证: WTC6106BSI未触摸=高电平, 触摸=低电平
 * @return  触摸状态位图 (bit i = 通道i: 1=触摸中, 0=未触摸)
 */
static uint8 zclSampleSw_ReadTouchInputs(void)
{
  uint8 port = P0;
  uint8 val = 0;
  if (!(port & BV(4))) val |= BV(0);  // 通道1: P0_4=低=触摸
  if (!(port & BV(5))) val |= BV(1);  // 通道2: P0_5
  if (!(port & BV(6))) val |= BV(2);  // 通道3: P0_6
  if (!(port & BV(7))) val |= BV(3);  // 通道4: P0_7
  return val;
}

/*********************************************************************
 * @fn      zclSampleSw_StartResetBlink
 *
 * @brief   BUG-011修复: 启动S1复位LED闪烁状态机
 *          替代HalLedBlink, 避免HalLedState与硬件状态不一致导致LED1异常
 *          闪烁参数: 3次, 300ms亮/300ms灭, 共1.8秒, 完成后执行复位
 *          仅操作LED1(P0_0), 不影响LED2~4(继电器状态指示)
 *
 * @return  none
 */
static void zclSampleSw_StartResetBlink(void)
{
  // 初始化闪烁状态机计数器
  resetBlinkCount = RESET_BLINK_TOTAL_COUNT;

  // 第1次切换: LED1亮 (经PWM层, 50%亮度)
  zclSampleSw_LedSetTarget(0, TRUE);

  // 启动300ms定时器, 触发下一次切换
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessResetBlink
 *
 * @brief   BUG-011修复: S1复位LED闪烁状态机处理
 *          每次切换LED1状态, 闪烁完成后执行复位流程
 *          闪烁序列: 亮(启动)-灭-亮-灭-亮-灭(完成) = 3次闪烁
 *          复位流程: Basic Reset + BDB Reset to FN(发送NLME_LeaveReq)
 *
 * @return  none
 */
static void zclSampleSw_ProcessResetBlink(void)
{
  resetBlinkCount--;

  if (resetBlinkCount > 0)
  {
    // 切换LED1状态: 奇数count=灭, 偶数count=亮 (经PWM层, 50%亮度)
    // count=5->灭, =4->亮, =3->灭, =2->亮, =1->灭 (共3次闪烁)
    zclSampleSw_LedSetTarget(0, !(resetBlinkCount % 2));

    // 启动下一次切换定时器
    osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
  }
  else
  {
    // 闪烁完成, 执行复位流程

    // 1. 调用Basic Reset回调, 重置ZCL属性到默认值
    zclSampleSw_BasicResetCB();

    // 2. 调用BDB Reset to FN, 让协议栈处理网络离开请求
    //    bdb_resetLocalAction()内部判断:
    //    - 若设备在网络中: 发送NLME_LeaveReq(z2m日志会显示leave请求)
    //    - 若设备不在网络中: 调用ZDApp_ResetTimerStart(500)直接重启
    bdb_resetLocalAction();

    // 3. 刷新所有继电器/LED状态到默认(继电器OFF, LED亮)
    zclSampleSw_UpdateAllRelayOutputs();
  }
}

/*********************************************************************
 * @fn      zclSampleSw_StartPairingBlink
 * @brief   v1.0.4新增: 启动配网中LED1慢闪状态机
 *          业界惯例: 设备配网中LED1慢闪(1Hz), 提示用户正在配网
 *          停止条件: 1)ZDO_STATE_CHANGE收到DEV_ROUTER(入网成功)
 *                   2)超时5分钟(SAMPLESW_PAIRING_TIMEOUT_MS)
 *          仅操作LED1(P0_0), 不影响继电器状态和其他LED
 * @return  none
 */
static void zclSampleSw_StartPairingBlink(void)
{
  pairingBlinkActive = TRUE;
  pairingBlinkLedOn = FALSE;  // 初始为灭
  pairingBlinkTickCount = 0;
  // LED1亮 (经PWM层, 50%亮度)
  zclSampleSw_LedSetTarget(0, TRUE);
  pairingBlinkLedOn = TRUE;
  // 启动500ms定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessPairingBlink
 * @brief   v1.0.4新增: 配网中LED1慢闪处理
 *          每500ms切换LED1状态(亮/灭), 形成1Hz闪烁
 *          超时5分钟后自动停止, 恢复LED1显示继电器1状态
 * @return  none
 */
static void zclSampleSw_ProcessPairingBlink(void)
{
  if (!pairingBlinkActive) return;

  pairingBlinkTickCount++;

  // 超时检查: 5分钟无入网则停止慢闪
  if (pairingBlinkTickCount >= PAIRING_BLINK_TIMEOUT_TICKS)
  {
    zclSampleSw_StopPairingBlink();
    return;
  }

  // 切换LED1状态 (经PWM层, 亮时50%亮度)
  if (pairingBlinkLedOn)
  {
    zclSampleSw_LedSetTarget(0, FALSE);  // 灭
    pairingBlinkLedOn = FALSE;
  }
  else
  {
    zclSampleSw_LedSetTarget(0, TRUE);   // 亮
    pairingBlinkLedOn = TRUE;
  }

  // 重启500ms定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_StopPairingBlink
 * @brief   v1.0.4新增: 停止配网中LED1慢闪, 恢复LED1显示继电器1状态
 *          由ZDO_STATE_CHANGE(DEV_ROUTER)或超时调用
 * @return  none
 */
static void zclSampleSw_StopPairingBlink(void)
{
  if (!pairingBlinkActive) return;

  pairingBlinkActive = FALSE;
  osal_stop_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT);

  // 恢复LED1显示继电器1状态
  zclSampleSw_UpdateRelayOutput(0);
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessTouchPoll
 * @brief   触摸输入轮询处理, 包含软件防抖状态机
 *          检测到电平变化后需连续 TOUCH_DEBOUNCE_COUNTS 次(200ms)确认,
 *          仅在上升沿(未触摸→触摸)触发一次继电器翻转, 长按不重复触发。
 *          同时检测S1(P1_3)长按5秒复位。
 *          处理后重新启动下一次轮询定时器。
 * @return  none
 */
void zclSampleSw_ProcessTouchPoll(void)
{
  uint8 cur = zclSampleSw_ReadTouchInputs();
  uint8 i;

  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    uint8 curBit    = (cur >> i) & 1;
    uint8 stableBit = (touchStableState >> i) & 1;

    if (curBit != stableBit)
    {
      // 当前电平与稳定状态不同, 累加防抖计数
      if (touchPending[i] != curBit)
      {
        touchPending[i] = curBit;
        touchDebounce[i] = 1;
      }
      else
      {
        touchDebounce[i]++;
      }

      // 连续确认达到阈值, 更新稳定状态
      if (touchDebounce[i] >= TOUCH_DEBOUNCE_COUNTS)
      {
        if (curBit)
        {
          touchStableState |= BV(i);
          // 上升沿确认(未触摸→触摸), 触发继电器翻转
          zclSampleSw_ToggleRelay(i);
        }
        else
        {
          touchStableState &= ~BV(i);
        }
        // 更新input_state并上报 (1.0f=触摸中, 0.0f=未触摸)
        zclSampleSw_InputState[i] = curBit ? 1.0f : 0.0f;
        zclSampleSw_ReportInputState(i);
        touchDebounce[i] = 0;
      }
    }
    else
    {
      // 电平与稳定状态一致, 重置防抖
      touchDebounce[i] = 0;
      touchPending[i] = stableBit;
    }
  }

  // S1长按检测 (P1_3, 低电平有效)
  if (!P1_3)
  {
    s1HoldCount++;
    if (s1HoldCount >= S1_RESET_THRESHOLD)
    {
      s1HoldCount = 0;
      // BUG-011修复: 启动LED闪烁状态机, 闪烁结束后执行复位
      // 不再使用HalLedBlink(避免HalLedState不一致导致LED1异常)
      zclSampleSw_StartResetBlink();
      // 停止触摸轮询, 避免闪烁期间被干扰
      osal_stop_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT);
      return;  // 直接返回, 不重启轮询(由闪烁状态机接管)
    }
  }
  else
  {
    s1HoldCount = 0;
  }

  // 断电记忆: 检测Z2M远程修改的startUpOnOff属性 (50ms周期轮询, 4路独立)
  // 若发现任一路变化, 调度延迟写入NV持久化新配置
  if (osal_memcmp(zclSampleSw_StartUpOnOff, startupOnOffCached, SAMPLESW_NUM_RELAYS) == FALSE)
  {
    osal_memcpy(startupOnOffCached, zclSampleSw_StartUpOnOff, SAMPLESW_NUM_RELAYS);
    zclSampleSw_NvScheduleSave();
  }

  // BUG-011修复: 防御性刷新LED状态 (每50ms, 跟随触摸轮询周期)
  // Z-Stack协议栈残留代码可能意外修改P0_0~P0_3, 定期刷新确保LED正确显示继电器状态
  zclSampleSw_UpdateAllRelayOutputs();

  // 重新启动下一次轮询
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT, TOUCH_POLL_INTERVAL_MS);
}


/*********************************************************************
 * @fn      zclSampleSw_ProcessCommissioningStatus
 *
 * @brief   Callback in which the status of the commissioning process are reported
 *
 * @param   bdbCommissioningModeMsg - Context message of the status of a commissioning process
 *
 * @return  none
 */
static void zclSampleSw_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //After formation, perform nwk steering again plus the remaining commissioning modes that has not been processed yet
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      else
      {
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
        //We are on the nwk, what now?
      }
      else
      {
        //See the possible errors for nwk steering procedure
        //No suitable networks found
        //Want to try other channels?
        //try with bdb_setChannelAttribute
      }
    break;
    case BDB_COMMISSIONING_FINDING_BINDING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //YOUR JOB:
      }
      else
      {
        //YOUR JOB:
        //retry?, wait for user interaction?
      }
    break;
    case BDB_COMMISSIONING_INITIALIZATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        //已成功恢复网络
      }
      else
      {
        //设备未入网，自动启动网络引导（加入现有网络）+ 发现绑定
        //注意：不请求 NWK_FORMATION，避免路由器创建自己的分布式网络
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING |
                               BDB_COMMISSIONING_MODE_FINDING_BINDING);
      }
    break;
#if ZG_BUILD_ENDDEVICE_TYPE    
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        //We did recover from losing parent
      }
      else
      {
        //Parent not found, attempt to rejoin again after a fixed delay
        osal_start_timerEx(zclSampleSw_TaskID, SAMPLEAPP_END_DEVICE_REJOIN_EVT, SAMPLEAPP_END_DEVICE_REJOIN_DELAY);
      }
    break;
#endif 
  }

  // v1.0.0重构: 移除 UI_UpdateComissioningStatus (UI模块已剥离)
}

/*********************************************************************
 * @fn      zclSampleSw_BasicResetCB
 *
 * @brief   Callback from the ZCL General Cluster Library
 *          to set all the Basic Cluster attributes to  default values.
 *
 * @param   none
 *
 * @return  none
 */
static void zclSampleSw_BasicResetCB( void )
{
  zclSampleSw_ResetAttributesToDefaultValues();

  // v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
  // 已移除: UI_UpdateLcd( );
}

/*********************************************************************
 * @fn      zclSampleApp_BatteryWarningCB
 *
 * @brief   Called to handle battery-low situation.
 *
 * @param   voltLevel - level of severity
 *
 * @return  none
 */
void zclSampleApp_BatteryWarningCB( uint8 voltLevel )
{
  if ( voltLevel == VOLT_LEVEL_CAUTIOUS )
  {
    // Send warning message to the gateway and blink LED
  }
  else if ( voltLevel == VOLT_LEVEL_BAD )
  {
    // Shut down the system
  }
}

/******************************************************************************
 *
 *  Functions for processing ZCL Foundation incoming Command/Response messages
 *
 *****************************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_ProcessIncomingMsg
 *
 * @brief   Process ZCL Foundation incoming message
 *
 * @param   pInMsg - pointer to the received message
 *
 * @return  none
 */
static void zclSampleSw_ProcessIncomingMsg( zclIncomingMsg_t *pInMsg )
{
  switch ( pInMsg->zclHdr.commandID )
  {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
      zclSampleSw_ProcessInReadRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE_RSP:
      zclSampleSw_ProcessInWriteRspCmd( pInMsg );
      break;
#endif
#ifdef ZCL_REPORT
    // See ZCL Test Applicaiton (zcl_testapp.c) for sample code on Attribute Reporting
    case ZCL_CMD_CONFIG_REPORT:
      //zclSampleSw_ProcessInConfigReportCmd( pInMsg );
      break;

    case ZCL_CMD_CONFIG_REPORT_RSP:
      //zclSampleSw_ProcessInConfigReportRspCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG:
      //zclSampleSw_ProcessInReadReportCfgCmd( pInMsg );
      break;

    case ZCL_CMD_READ_REPORT_CFG_RSP:
      //zclSampleSw_ProcessInReadReportCfgRspCmd( pInMsg );
      break;

    case ZCL_CMD_REPORT:
      //zclSampleSw_ProcessInReportCmd( pInMsg );
      break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
      zclSampleSw_ProcessInDefaultRspCmd( pInMsg );
      break;
#ifdef ZCL_DISCOVER
    case ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP:
      zclSampleSw_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_CMDS_GEN_RSP:
      zclSampleSw_ProcessInDiscCmdsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_RSP:
      zclSampleSw_ProcessInDiscAttrsRspCmd( pInMsg );
      break;

    case ZCL_CMD_DISCOVER_ATTRS_EXT_RSP:
      zclSampleSw_ProcessInDiscAttrsExtRspCmd( pInMsg );
      break;
#endif
    default:
      break;
  }

  if ( pInMsg->attrCmd )
    osal_mem_free( pInMsg->attrCmd );
}

#ifdef ZCL_READ
/*********************************************************************
 * @fn      zclSampleSw_ProcessInReadRspCmd
 *
 * @brief   Process the "Profile" Read Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInReadRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclReadRspCmd_t *readRspCmd;
  uint8 i;

  readRspCmd = (zclReadRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < readRspCmd->numAttr; i++)
  {
    // Notify the originator of the results of the original read attributes
    // attempt and, for each successfull request, the value of the requested
    // attribute
  }

  return TRUE;
}
#endif // ZCL_READ

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      zclSampleSw_ProcessInWriteRspCmd
 *
 * @brief   Process the "Profile" Write Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInWriteRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclWriteRspCmd_t *writeRspCmd;
  uint8 i;

  writeRspCmd = (zclWriteRspCmd_t *)pInMsg->attrCmd;
  for (i = 0; i < writeRspCmd->numAttr; i++)
  {
    // Notify the device of the results of the its original write attributes
    // command.
  }

  return TRUE;
}
#endif // ZCL_WRITE

/*********************************************************************
 * @fn      zclSampleSw_ProcessInDefaultRspCmd
 *
 * @brief   Process the "Profile" Default Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInDefaultRspCmd( zclIncomingMsg_t *pInMsg )
{
  // zclDefaultRspCmd_t *defaultRspCmd = (zclDefaultRspCmd_t *)pInMsg->attrCmd;
  // Device is notified of the Default Response command.
  (void)pInMsg;
  return TRUE;
}

#ifdef ZCL_DISCOVER
/*********************************************************************
 * @fn      zclSampleSw_ProcessInDiscCmdsRspCmd
 *
 * @brief   Process the Discover Commands Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInDiscCmdsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverCmdsCmdRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverCmdsCmdRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numCmd; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessInDiscAttrsRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInDiscAttrsRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsRspCmd_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsRspCmd_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessInDiscAttrsExtRspCmd
 *
 * @brief   Process the "Profile" Discover Attributes Extended Response Command
 *
 * @param   pInMsg - incoming message to process
 *
 * @return  none
 */
static uint8 zclSampleSw_ProcessInDiscAttrsExtRspCmd( zclIncomingMsg_t *pInMsg )
{
  zclDiscoverAttrsExtRsp_t *discoverRspCmd;
  uint8 i;

  discoverRspCmd = (zclDiscoverAttrsExtRsp_t *)pInMsg->attrCmd;
  for ( i = 0; i < discoverRspCmd->numAttr; i++ )
  {
    // Device is notified of the result of its attribute discovery command.
  }

  return TRUE;
}
#endif // ZCL_DISCOVER

#if defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)
/*********************************************************************
 * @fn      zclSampleSw_ProcessOTAMsgs
 *
 * @brief   Called to process callbacks from the ZCL OTA.
 *
 * @param   none
 *
 * @return  none
 */
static void zclSampleSw_ProcessOTAMsgs( zclOTA_CallbackMsg_t* pMsg )
{
  uint8 RxOnIdle;

  switch(pMsg->ota_event)
  {
  case ZCL_OTA_START_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {
      // Speed up the poll rate
      RxOnIdle = TRUE;
      ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
      NLME_SetPollRate( 2000 );
    }
    break;

  case ZCL_OTA_DL_COMPLETE_CALLBACK:
    if (pMsg->hdr.status == ZSuccess)
    {
      // Reset the CRC Shadow and reboot.  The bootloader will see the
      // CRC shadow has been cleared and switch to the new image
      HalOTAInvRC();
      SystemReset();
    }
    else
    {
#if (ZG_BUILD_ENDDEVICE_TYPE)    
      // slow the poll rate back down.
      RxOnIdle = FALSE;
      ZMacSetReq( ZMacRxOnIdle, &RxOnIdle );
      NLME_SetPollRate(DEVICE_POLL_RATE);
#endif
    }
    break;

  default:
    break;
  }
}
#endif // defined (OTA_CLIENT) && (OTA_CLIENT == TRUE)

/****************************************************************************
****************************************************************************/

// v1.0.0重构: 移除UI模块调用 (无LCD/无物理按键)
// 已移除: void zclSampleSw_UiActionToggleLight(uint16 keys) 函数实现
// 已移除: void zclSampleSw_UiUpdateLcd(uint8 gui_state, char * line[3]) 函数实现


