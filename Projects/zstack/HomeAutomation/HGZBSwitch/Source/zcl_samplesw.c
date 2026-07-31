/**************************************************************************************************
  Filename:       zcl_samplesw.c
  Description:    86四路智能开关Zigbee应用层主源文件 (基于Z-Stack 3.0.2 SampleSwitch)

  硬件: CC2530F256 + WTC6106BSI触摸芯片 + 4路继电器 + 4路LED
  功能模块:
    1. GPIO初始化 (继电器/LED/触摸引脚)
    2. 继电器控制 (UpdateRelayOutput/UpdateAllRelayOutputs)
    3. 触摸检测 (50ms轮询 + 2次防抖 + S1长按5秒复位)
    4. BDB入网 (区分已配网/新设备选择commissioning模式 + TX功率4dBm)
    5. ZCL命令处理 (On/Off/Toggle, 4路独立回调)
    6. LED控制 (继电器联动 + 配网慢闪1Hz + 复位闪烁3次, 直接GPIO无PWM)
    7. NV存储 (断电恢复, 延迟写入5秒 + 对比写入, 4路独立startUpOnOff)
    8. ZCL上报 (触摸后上报, 入网后全上报, ZCL命令后不上报)

  数据布局 (NV):
    SAMPLESW_NV_ID_RELAY_STATE   (0x0F10): 4字节, 4路继电器状态
    SAMPLESW_NV_ID_STARTUP_ONOFF (0x0F12): 4字节, 4路独立startUpOnOff配置
**************************************************************************************************/

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
#include "hal_drivers.h"
#include "hal_led.h"
#include "hal_uart.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_diagnostic.h"

#include "bdb_interface.h"
#include "nwk_util.h"
#include "ZMac.h"

#if defined ( ZIGBEE_FREQ_AGILITY ) || defined ( ZIGBEE_PANID_CONFLICT )
  #include "ZDNwkMgr.h"
#endif

#include "zcl_samplesw.h"

/*********************************************************************
 * MACROS
 */

/*********************************************************************
 * TYPEDEFS
 */

/*********************************************************************
 * GLOBAL VARIABLES
 */

// 任务ID
byte zclSampleSw_TaskID;

// 目标地址 (协调器)
afAddrType_t zclSampleSw_DstAddr;

// ZCL序列号
uint8 zclSampleSwSeqNum = 0;

// 网络状态
devStates_t zclSampleSw_NwkState = DEV_INIT;

/*********************************************************************
 * LOCAL VARIABLES
 */

// 触摸防抖状态 (bit i 对应通道 i: 0=稳定未触摸, 1=稳定触摸中)
static uint8 touchStableState = 0;
static uint8 touchDebounce[SAMPLESW_NUM_RELAYS] = {0, 0, 0, 0};   // 各通道防抖计数器
static uint8 touchPending[SAMPLESW_NUM_RELAYS]  = {0, 0, 0, 0};   // 各通道待确认的新电平
// S1长按复位计数器 (每次触摸轮询+1, 达到S1_RESET_THRESHOLD执行复位)
static uint8 s1HoldCount = 0;

// 断电记忆: startUpOnOff缓存值 (用于检测Z2M远程修改, 4路独立)
static uint8 startupOnOffCached[SAMPLESW_NUM_RELAYS] = {
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS,
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS
};

// 配网中LED1慢闪状态机
static uint8 pairingBlinkActive = FALSE;   // 慢闪是否活跃
static uint8 pairingBlinkLedOn = FALSE;    // 当前LED1是否亮
static uint16 pairingBlinkTickCount = 0;   // 已闪烁的tick数(每500ms+1, 用于超时判断)
// 配网超时阈值: 5分钟 = 300秒 = 600个500ms tick
#define PAIRING_BLINK_TIMEOUT_TICKS   (SAMPLESW_PAIRING_TIMEOUT_MS / PAIRING_BLINK_PERIOD_MS)

// S1复位LED闪烁状态机 (替代HalLedBlink, 避免HalLedState不一致)
static uint8 resetBlinkCount = 0;          // 闪烁状态机计数器(0~6)

/*********************************************************************
 * LOCAL FUNCTIONS 前向声明
 */
static void zclSampleSw_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg);
static void zclSampleSw_ProcessTouchPoll(void);
static uint8 zclSampleSw_ReadTouchInputs(void);
static void zclSampleSw_ToggleRelay(uint8 idx);
static void zclSampleSw_HandleOnOffCmd(uint8 idx, uint8 cmd);

// LED控制
static void zclSampleSw_LedWriteGpio(uint8 idx, uint8 on);
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on);

// 配网慢闪
static void zclSampleSw_StartPairingBlink(void);
static void zclSampleSw_ProcessPairingBlink(void);
static void zclSampleSw_StopPairingBlink(void);

// 复位闪烁
static void zclSampleSw_StartResetBlink(void);
static void zclSampleSw_ProcessResetBlink(void);

// NV存储
static void zclSampleSw_NvInit(void);
static void zclSampleSw_NvLoadPowerOnState(void);
static void zclSampleSw_NvScheduleSave(void);
static void zclSampleSw_NvProcessSave(void);

// ZCL上报
static void zclSampleSw_ReportOnOffState(uint8 idx);
static void zclSampleSw_ReportAllOnOffState(void);
static void zclSampleSw_ReportInputState(uint8 idx);

// ZCL回调函数 (4路独立)
void zclSampleSw_BasicResetCB(void);
void zclSampleSw_IdentifyCB(uint8 endpoint, uint16 identifyTime);
void zclSampleSw_OnOffCB_ep1(uint8 cmd);
void zclSampleSw_OnOffCB_ep2(uint8 cmd);
void zclSampleSw_OnOffCB_ep3(uint8 cmd);
void zclSampleSw_OnOffCB_ep4(uint8 cmd);

/*********************************************************************
 * @fn      zclSampleSw_Init
 * @brief   任务初始化
 * @param   task_id - 任务ID
 * @return  none
 */
void zclSampleSw_Init( byte task_id )
{
  zclSampleSw_TaskID = task_id;

  // 注册EP11端点 (Basic Cluster, 供z2m interview)
  bdb_RegisterSimpleDescriptor( &zclSampleSw_SimpleDesc );
  zcl_registerAttrList(SAMPLESW_ENDPOINT, zclSampleSw_NumAttributes, zclSampleSw_Attrs);

  // 注册4路继电器端点 (EP1-4, 对应 l1-l4)
  // 每个端点注册独立的callbacks, 使OnOff命令能定位到正确的继电器索引
  bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[0]);
  zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY1, &zclSampleSw_CmdCallbacks_ep1);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY1, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep1);

  bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[1]);
  zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY2, &zclSampleSw_CmdCallbacks_ep2);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY2, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep2);

  bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[2]);
  zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY3, &zclSampleSw_CmdCallbacks_ep3);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY3, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep3);

  bdb_RegisterSimpleDescriptor(&zclSampleSw_RelaySimpleDesc[3]);
  zclGeneral_RegisterCmdCallbacks(SAMPLESW_ENDPOINT_RELAY4, &zclSampleSw_CmdCallbacks_ep4);
  zcl_registerAttrList(SAMPLESW_ENDPOINT_RELAY4, ZCLSAMPLESW_NUM_RELAY_ATTRS, zclSampleSw_RelayAttrs_ep4);

  // 注册4路输入状态端点 (EP5-8, 对应 in1-in4)
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

  // 注册BDB commissioning状态回调
  bdb_RegisterCommissioningStatusCB( zclSampleSw_ProcessCommissioningStatus );

  // 初始化目标地址 (协调器)
  zclSampleSw_DstAddr.addrMode = (afAddrMode_t)Addr16Bit;
  zclSampleSw_DstAddr.addr.shortAddr = 0;
  zclSampleSw_DstAddr.endPoint = 1;

  // 默认属性值
  zclSampleSw_ResetAttributesToDefaultValues();

  // 断电记忆: 初始化NV项, 并按startUpOnOff策略恢复断电前继电器状态
  // 必须在UpdateAllRelayOutputs()之前调用, 确保GPIO输出与恢复后的状态一致
  zclSampleSw_NvInit();
  zclSampleSw_NvLoadPowerOnState();

  // 初始化继电器/LED/触摸GPIO, 并根据zclSampleSw_RelayState应用初始输出
  zclSampleSw_InitGpio();
  zclSampleSw_UpdateAllRelayOutputs();

  // 启动触摸输入轮询 (50ms周期, 内含软件防抖和S1长按检测)
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT, TOUCH_POLL_INTERVAL_MS);

  // 设置TX功率为4dBm (必须在bdb_StartCommissioning之前调用, 确保入网时即使用设置后的功率)
  // 4dBm与TI ZNP官方默认一致, 7dBm会导致RF不稳定
  ZMacSetTransmitPower(SAMPLESW_TX_POWER);

  // commissioning mode选择: 读取NV中的bdbNodeIsOnNetwork状态, 区分已配网/新设备
  // 已配网设备: 尝试rejoin恢复网络
  // 新设备(S1复位后/首次配网): 触发NWK_STEERING发现网络并加入
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

  // 启动配网中LED1慢闪, 提示用户设备正在配网
  // 已配网设备: BDB rejoin成功后ZDO_STATE_CHANGE会触发StopPairingBlink
  // 新设备: 持续慢闪直到入网成功或5分钟超时
  zclSampleSw_StartPairingBlink();
}

/*********************************************************************
 * @fn      zclSampleSw_event_loop
 * @brief   任务事件处理
 * @param   task_id - 任务ID
 * @param   events - 事件标志
 * @return  未处理的事件
 */
UINT16 zclSampleSw_event_loop( byte task_id, UINT16 events )
{
  afIncomingMSGPacket_t *MSGpkt;

  (void)task_id;  // 避免未使用参数警告

  if ( events & SYS_EVENT_MSG )
  {
    while ( (MSGpkt = (afIncomingMSGPacket_t *)osal_msg_receive( zclSampleSw_TaskID )) )
    {
      switch ( MSGpkt->hdr.event )
      {
        case ZDO_STATE_CHANGE:
          // 协议栈Router启动时会操作LED3/LED4(ZDApp.c), 覆盖继电器状态灯
          // 入网状态变化后重新刷新所有继电器/LED输出, 恢复正确显示
          zclSampleSw_UpdateAllRelayOutputs();
          // 状态同步: 入网成功(DEV_ROUTER)后立即上报当前OnOff状态
          // 修复断电恢复后Z2M状态不同步
          // 移除30秒周期性上报定时器, 避免无操作时触发z2m state_action生成无意义action事件
          // 状态同步改为依赖: 入网后立即上报 + 操作后立即上报 + z2m availability检测
          if ((devStates_t)(MSGpkt->hdr.status) == DEV_ROUTER)
          {
            // 入网成功, 停止配网中LED1慢闪 (StopPairingBlink内部有pairingBlinkActive保护, 多次调用安全)
            zclSampleSw_StopPairingBlink();
            // 立即上报所有4路OnOff状态
            zclSampleSw_ReportAllOnOffState();
          }
          zclSampleSw_NwkState = (devStates_t)(MSGpkt->hdr.status);
          break;

        default:
          break;
      }

      // 释放消息
      osal_msg_deallocate( (uint8 *)MSGpkt );
    }
    // 返回未处理的事件
    return ( events ^ SYS_EVENT_MSG );
  }

  // 触摸输入轮询事件 (50ms周期, 内含软件防抖和S1长按检测)
  if ( events & SAMPLESW_TOUCH_POLL_EVT )
  {
    zclSampleSw_ProcessTouchPoll();
    return ( events ^ SAMPLESW_TOUCH_POLL_EVT );
  }

  // 配网中LED1慢闪事件处理 (1Hz闪烁, 入网成功或5分钟超时后停止)
  if ( events & SAMPLESW_PAIRING_BLINK_EVT )
  {
    zclSampleSw_ProcessPairingBlink();
    return ( events ^ SAMPLESW_PAIRING_BLINK_EVT );
  }

  // S1复位LED闪烁事件处理 (用直接GPIO操作替代HalLedBlink, 避免HalLedState不一致)
  if ( events & SAMPLESW_RESET_BLINK_EVT )
  {
    zclSampleSw_ProcessResetBlink();
    return ( events ^ SAMPLESW_RESET_BLINK_EVT );
  }

  // 断电记忆: 延迟写入NV事件 (5秒到期, 执行Compare-Before-Write)
  if ( events & SAMPLESW_NV_SAVE_EVT )
  {
    zclSampleSw_NvProcessSave();
    return ( events ^ SAMPLESW_NV_SAVE_EVT );
  }

  // End Device rejoin (保留兼容)
  if ( events & SAMPLEAPP_END_DEVICE_REJOIN_EVT )
  {
    bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING);
    return ( events ^ SAMPLEAPP_END_DEVICE_REJOIN_EVT );
  }

  // 丢弃未处理的事件
  return ( 0 );
}

/*********************************************************************
 * GPIO初始化
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_InitGpio
 * @brief   初始化继电器/LED/触摸引脚的GPIO方向与初始电平
 *          LED(P0_0~P0_3): 输出, P0SEL需显式配置为GPIO功能
 *          继电器(P1_0/P1_2/P1_6/P2_0): 输出, 默认高电平(继电器断开)
 *          触摸(P0_4~P0_7): 输入, 上拉(CC2530 P0口默认上拉)
 *          S1(P1_3): 输入, 上拉(低电平有效)
 * @return  none
 */
void zclSampleSw_InitGpio(void)
{
  // LED引脚设为GPIO功能 (P0_0~P0_3)
  // 方案B: LED方向由 HAL_BOARD_INIT() 通过 LEDx_SET_DIR() 配置,
  //        但 P0SEL 需在此显式设置 (HAL_BOARD_INIT 不配置 P0SEL)
  P0SEL &= ~(BV(0) | BV(1) | BV(2) | BV(3));  // P0_0~P0_3 选为GPIO

  // 继电器引脚设为GPIO功能并配置为输出
  P1SEL &= ~RELAY_P1_BV;        // P1_0/P1_2/P1_6 选为GPIO
  P2SEL &= ~RELAY_P2_BV;        // P2_0 选为GPIO
  P1DIR |= RELAY_P1_BV;         // 设为输出
  P2DIR |= RELAY_P2_BV;         // 设为输出

  // 触摸输入引脚设为GPIO功能并配置为输入(默认上拉)
  // WTC6106BSI: 未触摸=高电平, 触摸=低电平(开漏输出, 需上拉)
  P0SEL &= ~TOUCH_INPUT_BV;     // P0_4~P0_7 选为GPIO
  P0DIR &= ~TOUCH_INPUT_BV;     // 设为输入

  // S1按键引脚 (P1_3, 输入, 上拉, 低电平有效)
  P1SEL &= ~BV(3);
  P1DIR &= ~BV(3);

  // 初始化触摸防抖状态
  touchStableState = 0;
}

/*********************************************************************
 * LED控制
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_LedWriteGpio
 * @brief   LED底层控制 (方案B: 通过 HalLedSet API 操作)
 *          LED引脚映射由 hal_board_cfg_linxee.h 定义: LED1~4 → P0_0~P0_3 (ACTIVE_LOW)
 *          on=TRUE → HalLedSet(ON)  → HAL_TURN_ON_LEDn()  → P0_x=0 (亮)
 *          on=FALSE→ HalLedSet(OFF) → HAL_TURN_OFF_LEDn() → P0_x=1 (灭)
 * @param   idx - LED索引 0~3 (LED1~4)
 * @param   on  - TRUE=亮, FALSE=灭
 * @return  none
 */
static void zclSampleSw_LedWriteGpio(uint8 idx, uint8 on)
{
  // HAL_LED_1(0x01)<<idx 映射 idx 0~3 → HAL_LED_1~4
  HalLedSet(HAL_LED_1 << idx, on ? HAL_LED_MODE_ON : HAL_LED_MODE_OFF);
}

/*********************************************************************
 * @fn      zclSampleSw_LedSetTarget
 * @brief   设置LED期望亮灭状态 (替代直接P0_x=val)
 *          所有LED控制点(继电器联动/配网慢闪/复位闪烁)统一调用本函数
 *          移除PWM调光, 恢复直接写GPIO控制亮度(100%)
 * @param   idx - LED索引 0~3
 * @param   on  - TRUE=期望亮, FALSE=期望灭
 * @return  none
 */
static void zclSampleSw_LedSetTarget(uint8 idx, uint8 on)
{
  zclSampleSw_LedWriteGpio(idx, on);
}

/*********************************************************************
 * 继电器控制
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_UpdateRelayOutput
 * @brief   根据zclSampleSw_RelayState[idx]更新继电器GPIO与LED状态
 *          LED1在配网慢闪期间被跳过, 由慢闪状态机控制
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
      // 配网中LED1慢闪激活时, 不刷新LED1, 由慢闪状态机控制
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
 * @brief   更新所有4路继电器输出 (含LED联动)
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

/*********************************************************************
 * 触摸检测
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_ReadTouchInputs
 * @brief   读取P0口, 按位解析4路触摸状态 (低电平=触摸)
 * @return  bit i = 1: 通道i触摸中; bit i = 0: 通道i未触摸
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
 * @fn      zclSampleSw_ProcessTouchPoll
 * @brief   触摸轮询处理 (50ms周期)
 *          - 4路触摸电平读取 + 2次防抖确认
 *          - S1长按5秒复位检测
 *          - Z2M远程修改startUpOnOff检测
 *          - 防御性LED刷新
 * @return  none
 */
static void zclSampleSw_ProcessTouchPoll(void)
{
  uint8 cur = zclSampleSw_ReadTouchInputs();
  uint8 i;

  // 4路触摸防抖处理
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
      // 启动LED闪烁状态机, 闪烁结束后执行复位
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

  // 重新启动下一次轮询
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_TOUCH_POLL_EVT, TOUCH_POLL_INTERVAL_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_ToggleRelay
 * @brief   翻转指定继电器状态, 更新GPIO输出, 上报ZCL状态, 调度NV写入
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
 * 配网慢闪状态机 (1Hz)
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_StartPairingBlink
 * @brief   启动配网中LED1慢闪状态机 (1Hz, 5分钟超时)
 *          仅操作LED1(P0_0), 不影响继电器状态和其他LED
 * @return  none
 */
static void zclSampleSw_StartPairingBlink(void)
{
  pairingBlinkActive = TRUE;
  pairingBlinkLedOn = FALSE;  // 初始为灭
  pairingBlinkTickCount = 0;
  // LED1亮
  zclSampleSw_LedSetTarget(0, TRUE);
  pairingBlinkLedOn = TRUE;
  // 启动500ms定时器
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_PAIRING_BLINK_EVT, PAIRING_BLINK_PERIOD_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessPairingBlink
 * @brief   配网中LED1慢闪处理 (每500ms切换LED1状态, 形成1Hz闪烁)
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

  // 切换LED1状态
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
 * @brief   停止配网中LED1慢闪, 恢复LED1显示继电器1状态
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
 * 复位闪烁状态机 (3次)
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_StartResetBlink
 * @brief   启动S1复位LED闪烁状态机 (3次, 300ms亮/300ms灭, 共1.8秒)
 *          替代HalLedBlink, 避免HalLedState与硬件状态不一致导致LED1异常
 *          仅操作LED1(P0_0), 不影响LED2~4(继电器状态指示)
 * @return  none
 */
static void zclSampleSw_StartResetBlink(void)
{
  // 初始化闪烁状态机计数器
  resetBlinkCount = RESET_BLINK_TOTAL_COUNT;

  // 第1次切换: LED1亮
  zclSampleSw_LedSetTarget(0, TRUE);

  // 启动300ms定时器, 触发下一次切换
  osal_start_timerEx(zclSampleSw_TaskID, SAMPLESW_RESET_BLINK_EVT, RESET_BLINK_PERIOD_MS);
}

/*********************************************************************
 * @fn      zclSampleSw_ProcessResetBlink
 * @brief   S1复位LED闪烁状态机处理
 *          每次切换LED1状态, 闪烁完成后执行复位流程
 *          闪烁序列: 亮(启动)-灭-亮-灭-亮-灭(完成) = 3次闪烁
 *          复位流程: Basic Reset + BDB Reset to FN(发送NLME_LeaveReq)
 * @return  none
 */
static void zclSampleSw_ProcessResetBlink(void)
{
  resetBlinkCount--;

  if (resetBlinkCount > 0)
  {
    // 切换LED1状态: 奇数count=灭, 偶数count=亮
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
 * NV存储 (断电恢复)
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_NvInit
 * @brief   初始化断电记忆NV存储项 (若不存在则用默认值创建)
 * @return  none
 */
static void zclSampleSw_NvInit(void)
{
  uint8 defaultRelayState[SAMPLESW_NUM_RELAYS] = {FALSE, FALSE, FALSE, FALSE};
  uint8 defaultStartupOnOff[SAMPLESW_NUM_RELAYS] = {
    STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS,
    STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS
  };

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

  // 按每路独立startUpOnOff策略设置上电继电器状态
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
 * ZCL上报
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_ReportOnOffState
 * @brief   向协调器上报指定通道继电器的OnOff属性状态(ZCL Report Attributes)
 *          仅触摸翻转后调用(z2m不知道本地操作); ZCL命令处理后不再调用
 *          (z2m通过Default Response已确认状态, 主动Report是冗余的)
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
 *          用于入网后立即同步状态, 确保Z2M状态与设备实际状态一致
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
 * @brief   向协调器上报指定通道的触摸输入状态 (genAnalogInput.presentValue)
 *          1.0f=触摸中, 0.0f=未触摸
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

  zcl_SendReportCmd(ep, &zclSampleSw_DstAddr, ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC,
                    reportCmd, ZCL_FRAME_SERVER_CLIENT_DIR, TRUE, zclSampleSwSeqNum++);

  osal_msg_deallocate((uint8 *)reportCmd);
}

/*********************************************************************
 * ZCL命令处理
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_HandleOnOffCmd
 * @brief   处理ZCL On/Off/Toggle命令, 更新继电器状态和GPIO
 *          ZCL命令处理后不主动Report (z2m通过Default Response已确认状态)
 *          触摸操作(ToggleRelay)仍保留主动上报, 因为z2m不知道本地触摸事件
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
  // 断电记忆: 调度延迟写入 (5秒后合并写入NV)
  zclSampleSw_NvScheduleSave();
}

/*********************************************************************
 * ZCL回调函数 (4路独立, 每个端点对应一个继电器)
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_BasicResetCB
 * @brief   Basic Cluster Reset命令回调, 重置ZCL属性到默认值
 * @return  none
 */
void zclSampleSw_BasicResetCB(void)
{
  zclSampleSw_ResetAttributesToDefaultValues();
}

/*********************************************************************
 * @fn      zclSampleSw_IdentifyCB
 * @brief   Identify Cluster命令回调
 * @param   endpoint - 端点
 * @param   identifyTime - 识别时间(秒)
 * @return  none
 */
void zclSampleSw_IdentifyCB(uint8 endpoint, uint16 identifyTime)
{
  (void)endpoint;
  (void)identifyTime;
  // 简单实现, 不做特殊处理
}

/*********************************************************************
 * @fn      zclSampleSw_OnOffCB_ep1
 * @brief   EP1(继电器1) On/Off命令回调
 * @param   cmd - ZCL命令ID
 * @return  none
 */
void zclSampleSw_OnOffCB_ep1(uint8 cmd)
{
  zclSampleSw_HandleOnOffCmd(0, cmd);
}

/*********************************************************************
 * @fn      zclSampleSw_OnOffCB_ep2
 * @brief   EP2(继电器2) On/Off命令回调
 * @return  none
 */
void zclSampleSw_OnOffCB_ep2(uint8 cmd)
{
  zclSampleSw_HandleOnOffCmd(1, cmd);
}

/*********************************************************************
 * @fn      zclSampleSw_OnOffCB_ep3
 * @brief   EP3(继电器3) On/Off命令回调
 * @return  none
 */
void zclSampleSw_OnOffCB_ep3(uint8 cmd)
{
  zclSampleSw_HandleOnOffCmd(2, cmd);
}

/*********************************************************************
 * @fn      zclSampleSw_OnOffCB_ep4
 * @brief   EP4(继电器4) On/Off命令回调
 * @return  none
 */
void zclSampleSw_OnOffCB_ep4(uint8 cmd)
{
  zclSampleSw_HandleOnOffCmd(3, cmd);
}

/*********************************************************************
 * BDB Commissioning状态回调
 *********************************************************************/

/*********************************************************************
 * @fn      zclSampleSw_ProcessCommissioningStatus
 * @brief   BDB commissioning状态回调
 * @param   bdbCommissioningModeMsg - commissioning状态消息
 * @return  none
 */
static void zclSampleSw_ProcessCommissioningStatus(bdbCommissioningModeMsg_t *bdbCommissioningModeMsg)
{
  switch(bdbCommissioningModeMsg->bdbCommissioningMode)
  {
    case BDB_COMMISSIONING_FORMATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        // Formation成功后, 继续NWK_STEERING + 剩余commissioning modes
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING | bdbCommissioningModeMsg->bdbRemainingCommissioningModes);
      }
      break;

    case BDB_COMMISSIONING_NWK_STEERING:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        // 新设备入网成功, 停止配网中LED1慢闪
        zclSampleSw_StopPairingBlink();
      }
      break;

    case BDB_COMMISSIONING_FINDING_BINDING:
      break;

    case BDB_COMMISSIONING_INITIALIZATION:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_SUCCESS)
      {
        // 已配网设备网络信息恢复成功, 停止配网中LED1慢闪
        // (ZDO_STATE_CHANGE(DEV_ROUTER)可能因状态去重不触发, BDB回调更可靠)
        zclSampleSw_StopPairingBlink();
      }
      else
      {
        // 设备未入网, 自动启动网络引导(加入现有网络) + 发现绑定
        // 注意: 不请求NWK_FORMATION, 避免路由器创建自己的分布式网络
        bdb_StartCommissioning(BDB_COMMISSIONING_MODE_NWK_STEERING |
                               BDB_COMMISSIONING_MODE_FINDING_BINDING);
      }
      break;

#if ZG_BUILD_ENDDEVICE_TYPE
    case BDB_COMMISSIONING_PARENT_LOST:
      if(bdbCommissioningModeMsg->bdbCommissioningStatus == BDB_COMMISSIONING_NETWORK_RESTORED)
      {
        // 已恢复与父节点的连接
      }
      else
      {
        // 父节点未找到, 延迟后重试rejoin
        osal_start_timerEx(zclSampleSw_TaskID, SAMPLEAPP_END_DEVICE_REJOIN_EVT, SAMPLEAPP_END_DEVICE_REJOIN_DELAY);
      }
      break;
#endif
  }
}

/****************************************************************************
****************************************************************************/
