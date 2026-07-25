/**************************************************************************************************
  Filename:       zcl_samplesw.h
  Revised:        $Date: 2015-08-19 17:11:00 -0700 (Wed, 19 Aug 2015) $
  Revision:       $Revision: 44460 $


  Description:    This file contains the Zigbee Cluster Library Home
                  Automation Sample Application.


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
 * CONSTANTS
 */
#define SAMPLESW_ENDPOINT               8

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

#define LIGHT_OFF                       0x00
#define LIGHT_ON                        0x01

// Events for the sample app
#define SAMPLEAPP_END_DEVICE_REJOIN_EVT   0x0001

// UI Events
#define SAMPLEAPP_LCD_AUTO_UPDATE_EVT       0x0010
#define SAMPLEAPP_KEY_AUTO_REPEAT_EVT       0x0020

// 触摸输入轮询事件 (100ms周期, 带软件防抖)
#define SAMPLESW_TOUCH_POLL_EVT             0x0004

// 断电记忆: 延迟写入NV事件 (状态变化后5秒写入, 减少Flash磨损)
#define SAMPLESW_NV_SAVE_EVT                0x0008

// S1长按复位LED闪烁事件 (BUG-011修复: 替代HalLedBlink, 避免HalLedState不一致)
// 3次闪烁, 300ms亮/300ms灭, 共1.8秒, 闪烁结束后执行复位
#define SAMPLESW_RESET_BLINK_EVT            0x0040

// NV存储项ID (应用自定义, 避开Z-Stack系统区0x0001~0x0097和ZNP保留区0x0F01~0x0F07)
#define SAMPLESW_NV_ID_RELAY_STATE          0x0F10  // 4路继电器状态
#define SAMPLESW_NV_ID_STARTUP_ONOFF        0x0F12  // 4路独立startUpOnOff配置 (v0.2.2起, 避开v0.2.1的1字节旧ID 0x0F11)

// startUpOnOff属性值 (ZCL标准)
#define STARTUP_ONOFF_OFF                   0x00    // 上电关闭
#define STARTUP_ONOFF_ON                    0x01    // 上电开启
#define STARTUP_ONOFF_TOGGLE                0x02    // 上电切换
#define STARTUP_ONOFF_PREVIOUS              0xFF    // 恢复断电前状态(断电记忆)

// 延迟写入时间 (ms), 状态变化后等待此时间无新变化才写入NV
#define SAMPLESW_NV_SAVE_DELAY_MS           5000

// ZCL属性ID: startUpOnOff (genOnOff cluster, 非标准定义需自定义)
#define ATTRID_STARTUP_ON_OFF               0x4003

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
extern SimpleDescriptionFormat_t zclSampleSw_SimpleDesc;

extern SimpleDescriptionFormat_t zclSampleSw9_SimpleDesc;

extern SimpleDescriptionFormat_t zclSampleSw_RelaySimpleDesc[SAMPLESW_NUM_RELAYS];

extern SimpleDescriptionFormat_t zclSampleSw_InputSimpleDesc[SAMPLESW_NUM_INPUTS];

extern CONST zclAttrRec_t zclSampleSw_Attrs[];

extern uint8 zclSampleSw_RelayState[SAMPLESW_NUM_RELAYS];

// 断电记忆: startUpOnOff配置 (4路独立, 默认恢复之前状态)
extern uint8 zclSampleSw_StartUpOnOff[SAMPLESW_NUM_RELAYS];

extern float zclSampleSw_InputState[SAMPLESW_NUM_INPUTS];

extern uint8  zclSampleSw_OnOff;

extern uint16 zclSampleSw_IdentifyTime;

extern uint8 zclSampleSw_OnOffSwitchType;

extern uint8 zclSampleSw_OnOffSwitchActions;

extern CONST uint8 zclSampleSw_NumAttributes;

/*********************************************************************
 * FUNCTIONS
 */

 /*
  * Initialization for the task
  */
extern void zclSampleSw_Init( byte task_id );

/*
 *  Event Process for the task
 */
extern UINT16 zclSampleSw_event_loop( byte task_id, UINT16 events );

/*
 *  Reset all writable attributes to their default values.
 */
extern void zclSampleSw_ResetAttributesToDefaultValues(void); //implemented in zcl_samplesw_data.c

/*
 *  初始化继电器/LED/触摸引脚的GPIO配置
 */
extern void zclSampleSw_InitGpio(void);

/*
 *  根据zclSampleSw_RelayState[idx]更新指定通道的继电器GPIO和LED状态
 *  idx: 0~3 对应 EP1~EP4
 */
extern void zclSampleSw_UpdateRelayOutput(uint8 idx);

/*
 *  更新所有4路继电器的GPIO和LED输出
 */
extern void zclSampleSw_UpdateAllRelayOutputs(void);

/*
 *  处理触摸输入轮询 (100ms周期调用, 内含软件防抖)
 */
extern void zclSampleSw_ProcessTouchPoll(void);

/*********************************************************************
*********************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ZCL_SAMPLEAPP_H */
