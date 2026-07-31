/**************************************************************************************************
  Filename:       OSAL_SampleSw.c
  Description:    86四路智能开关OSAL任务初始化 (基于Z-Stack 3.0.2 SampleSwitch)

  说明:
    1. MT/GP/Touchlink/OTA 通过条件编译排除 (不定义对应宏即可, 保留代码结构便于将来扩展)
    2. 保留ZIGBEE_FREQ_AGILITY/PANID_CONFLICT条件编译 (频率捷变/PAN冲突处理)
    3. 保留INTER_PAN的stub_aps条件编译 (跨PAN通信基础, 当前未启用)
    4. 保留ZIGBEE_FRAGMENTATION条件编译 (分片传输, 当前未启用)

  任务列表 (顺序必须与osalInitTasks中的初始化调用一致):
    macEventLoop           - MAC层事件循环
    nwk_event_loop         - 网络层事件循环
    [gp_event_loop]        - Green Power (条件编译, 当前不启用)
    Hal_ProcessEvent       - 硬件抽象层事件处理
    [MT_ProcessEvent]      - MT串口监控 (条件编译, 当前不启用)
    APS_event_loop         - APS层事件循环
    [APSF_ProcessEvent]    - 分片传输 (条件编译, 当前不启用)
    ZDApp_event_loop       - Zigbee设备应用事件循环
    [ZDNwkMgr_event_loop]  - 频率捷变/PAN冲突 (条件编译, 当前不启用)
    [StubAPS_ProcessEvent] - 跨PAN通信 (条件编译, 当前不启用)
    [touchLink*_event_loop]- Touchlink (条件编译, 当前不启用)
    zcl_event_loop         - ZCL层事件循环
    bdb_event_loop         - BDB基设备行为事件循环
    zclSampleSw_event_loop - 应用层事件循环 (触摸轮询/NV保存/LED状态机等)
    [zclOTA_event_loop]    - OTA升级 (条件编译, 当前不启用)
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */

#include "ZComDef.h"
#include "hal_drivers.h"
#include "OSAL.h"
#include "OSAL_Tasks.h"

#if defined ( MT_TASK )
  #include "MT.h"
  #include "MT_TASK.h"
#endif

#include "nwk.h"
#include "APS.h"
#include "ZDApp.h"

#include "bdb_interface.h"

#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
  #include "gp_common.h"
#endif
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  #include "ZDNwkMgr.h"
#endif
#if defined ( ZIGBEE_FRAGMENTATION )
  #include "aps_frag.h"
#endif

#if defined ( INTER_PAN )
  #include "stub_aps.h"
#if defined ( BDB_TL_INITIATOR )
  #include "bdb_touchlink_initiator.h"
#endif // BDB_TL_INITIATOR
#if defined ( BDB_TL_TARGET )
  #include "bdb_touchlink_target.h"
#endif // BDB_TL_TARGET
#endif // INTER_PAN

#include "zcl_samplesw.h"

#if (defined OTA_CLIENT) && (OTA_CLIENT == TRUE)
  #include "zcl_ota.h"
#endif

/*********************************************************************
 * GLOBAL VARIABLES
 */

// 任务事件处理函数表, 顺序必须与下方osalInitTasks中的初始化调用一致
const pTaskEventHandlerFn tasksArr[] = {
  macEventLoop,
  nwk_event_loop,
#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
  gp_event_loop,
#endif
  Hal_ProcessEvent,
#if defined( MT_TASK )
  MT_ProcessEvent,
#endif
  APS_event_loop,
#if defined ( ZIGBEE_FRAGMENTATION )
  APSF_ProcessEvent,
#endif
  ZDApp_event_loop,
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_event_loop,
#endif
  // Added to include TouchLink functionality
  #if defined ( INTER_PAN )
    StubAPS_ProcessEvent,
  #endif
  // Added to include TouchLink initiator functionality
  #if defined ( BDB_TL_INITIATOR )
    touchLinkInitiator_event_loop,
  #endif
  // Added to include TouchLink target functionality
  #if defined ( BDB_TL_TARGET )
    touchLinkTarget_event_loop,
  #endif
  zcl_event_loop,
  bdb_event_loop,
  zclSampleSw_event_loop,
#if (defined OTA_CLIENT) && (OTA_CLIENT == TRUE)
  zclOTA_event_loop
#endif
};

const uint8 tasksCnt = sizeof( tasksArr ) / sizeof( tasksArr[0] );
uint16 *tasksEvents;

/*********************************************************************
 * FUNCTIONS
 *********************************************************************/

/*********************************************************************
 * @fn      osalInitTasks
 *
 * @brief   此函数依次调用每个任务的初始化函数, 任务ID递增分配
 *          任务ID顺序必须与tasksArr[]中的事件处理函数顺序一致
 *
 * @param   none
 *
 * @return  none
 */
void osalInitTasks( void )
{
  uint8 taskID = 0;

  // 分配任务事件数组并清零
  tasksEvents = (uint16 *)osal_mem_alloc( sizeof( uint16 ) * tasksCnt);
  osal_memset( tasksEvents, 0, (sizeof( uint16 ) * tasksCnt));

  macTaskInit( taskID++ );
  nwk_init( taskID++ );
#if !defined (DISABLE_GREENPOWER_BASIC_PROXY) && (ZG_BUILD_RTR_TYPE)
  gp_Init( taskID++ );
#endif
  Hal_Init( taskID++ );
#if defined( MT_TASK )
  MT_TaskInit( taskID++ );
#endif
  APS_Init( taskID++ );
#if defined ( ZIGBEE_FRAGMENTATION )
  APSF_Init( taskID++ );
#endif
  ZDApp_Init( taskID++ );
#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  ZDNwkMgr_Init( taskID++ );
#endif
  // Added to include TouchLink functionality
  #if defined ( INTER_PAN )
    StubAPS_Init( taskID++ );
  #endif
  // Added to include TouchLink initiator functionality
  #if defined( BDB_TL_INITIATOR )
    touchLinkInitiator_Init( taskID++ );
  #endif
  // Added to include TouchLink target functionality
  #if defined( BDB_TL_TARGET )
    touchLinkTarget_Init( taskID++ );
  #endif
  zcl_Init( taskID++ );
  bdb_Init( taskID++ );
  zclSampleSw_Init( taskID++ );
#if (defined OTA_CLIENT) && (OTA_CLIENT == TRUE)
  zclOTA_Init( taskID );
#endif
}

/*********************************************************************
*********************************************************************/
