/**************************************************************************************************
  Filename:       zcl_samplesw_data.c
  Description:    86四路智能开关Zigbee应用层数据定义 (设备信息/端点描述符/属性表/回调表)

  借壳策略: ModelId = LXN-4S27LX1.0, 匹配z2m HGZB-4S转换规则
  端点布局:
    EP1~EP4: 4路继电器 (genOnOff server, 含onOff + startUpOnOff属性)
    EP5~EP8: 4路输入状态 (genAnalogInput server, 含presentValue属性)
    EP11:    Basic Cluster (供z2m interview, 含ModelId/SwBuildId等)

  ZCL字符串格式: 第一字节为长度前缀, 不含'\0'终止符
  SwBuildId长度限制: 总长度(含长度前缀) <= 16字节, 避免ZCL Read Attrs Rsp超MTU
**************************************************************************************************/

/*********************************************************************
 * INCLUDES
 */
#include "ZComDef.h"
#include "OSAL.h"
#include "AF.h"
#include "ZDConfig.h"

#include "zcl.h"
#include "zcl_general.h"
#include "zcl_ha.h"
#include "zcl_diagnostic.h"

#include "zcl_samplesw.h"

/*********************************************************************
 * CONSTANTS
 */

#define SAMPLESW_DEVICE_VERSION     1
#define SAMPLESW_FLAGS              0

// 硬件版本号 (0=初始原型, 1=未使用, 2=当前量产硬件版本)
#define SAMPLESW_HWVERSION          2
#define SAMPLESW_ZCLVERSION         0

#define DEFAULT_PHYSICAL_ENVIRONMENT 0
#define DEFAULT_DEVICE_ENABLE_STATE DEVICE_ENABLED
#define DEFAULT_IDENTIFY_TIME       0

/*********************************************************************
 * GLOBAL VARIABLES - 设备信息 (Basic Cluster属性)
 */

// 全局clusterRevision (ZCL6对应revision #1)
const uint16 zclSampleSw_clusterRevision_all = 0x0001;

// Basic Cluster属性
const uint8 zclSampleSw_HWRevision = SAMPLESW_HWVERSION;
const uint8 zclSampleSw_ZCLVersion = SAMPLESW_ZCLVERSION;

// ZCL字符串: 第一字节为长度前缀, 不含'\0'
// ManufacturerName: "Linxee" (6字符)
const uint8 zclSampleSw_ManufacturerName[] = { 6, 'L','i','n','x','e','e' };

// ModelId: "LXN-4S27LX1.0" (13字符) - 借壳必需, 修改后z2m无法识别
const uint8 zclSampleSw_ModelId[] = { 13, 'L','X','N','-','4','S','2','7','L','X','1','.','0' };

// DateCode: 固件编译日期 (格式: YYYYMMDD, 8字符)
const uint8 zclSampleSw_DateCode[] = { 8, '2','0','2','6','0','7','3','1' };

// SwBuildId: 型号+版本号 (总长度含前缀16字节, 避免ZCL Read Attrs Rsp超MTU)
// v0.1.1: 修复LED1/3/4不亮 + OnOff属性权限统一
const uint8 zclSampleSw_SwBuildId[] = { 16, 'H','A','-','S','P','A','4','C','1','-','0','.','1','.','1' };

const uint8 zclSampleSw_PowerSource = POWER_SOURCE_MAINS_1_PHASE;

uint8 zclSampleSw_LocationDescription[17];
uint8 zclSampleSw_PhysicalEnvironment;
uint8 zclSampleSw_DeviceEnable = DEVICE_ENABLED;

// Identify Cluster
uint16 zclSampleSw_IdentifyTime = 0;

/*********************************************************************
 * GLOBAL VARIABLES - 应用状态
 */

// 4路继电器状态 (TRUE=ON, FALSE=OFF, 默认全OFF)
// 实际初始值由zclSampleSw_NvLoadPowerOnState()按startUpOnOff策略恢复
uint8 zclSampleSw_RelayState[SAMPLESW_NUM_RELAYS] = { FALSE, FALSE, FALSE, FALSE };

// 4路独立startUpOnOff配置 (默认恢复断电前状态)
// v0.1.0: 4路独立数组, 避免Z2M独立配置被覆盖
uint8 zclSampleSw_StartUpOnOff[SAMPLESW_NUM_RELAYS] = {
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS,
  STARTUP_ONOFF_PREVIOUS, STARTUP_ONOFF_PREVIOUS
};

// 4路输入状态 (触摸: 1.0f=触摸中, 0.0f=未触摸)
float zclSampleSw_InputState[SAMPLESW_NUM_INPUTS] = { 0.0f, 0.0f, 0.0f, 0.0f };

/*********************************************************************
 * ATTRIBUTE DEFINITIONS - 属性表
 * 注意: 同一cluster内的属性ID必须升序排列, 以支持Foundation discovery命令
 */

// EP11: Basic Cluster属性表 (供z2m interview)
CONST zclAttrRec_t zclSampleSw_Attrs[] =
{
  // *** General Basic Cluster Attributes ***
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_ZCL_VERSION, ZCL_DATATYPE_UINT8, ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_ZCLVersion }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_HW_VERSION, ZCL_DATATYPE_UINT8, ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_HWRevision }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_MANUFACTURER_NAME, ZCL_DATATYPE_CHAR_STR, ACCESS_CONTROL_READ,
      (void *)zclSampleSw_ManufacturerName }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_MODEL_ID, ZCL_DATATYPE_CHAR_STR, ACCESS_CONTROL_READ,
      (void *)zclSampleSw_ModelId }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_DATE_CODE, ZCL_DATATYPE_CHAR_STR, ACCESS_CONTROL_READ,
      (void *)zclSampleSw_DateCode }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_SW_BUILD_ID, ZCL_DATATYPE_CHAR_STR, ACCESS_CONTROL_READ,
      (void *)zclSampleSw_SwBuildId }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_POWER_SOURCE, ZCL_DATATYPE_ENUM8, ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_PowerSource }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_LOCATION_DESC, ZCL_DATATYPE_CHAR_STR,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)zclSampleSw_LocationDescription }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_PHYSICAL_ENV, ZCL_DATATYPE_ENUM8,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSampleSw_PhysicalEnvironment }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_BASIC_DEVICE_ENABLED, ZCL_DATATYPE_BOOLEAN,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSampleSw_DeviceEnable }
  },
  {
    ZCL_CLUSTER_ID_GEN_BASIC,
    { ATTRID_CLUSTER_REVISION, ZCL_DATATYPE_UINT16, ACCESS_CONTROL_READ,
      (void *)&zclSampleSw_clusterRevision_all }
  },

  // *** Identify Cluster Attribute ***
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { ATTRID_IDENTIFY_TIME, ZCL_DATATYPE_UINT16,
      (ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE),
      (void *)&zclSampleSw_IdentifyTime }
  },
  {
    ZCL_CLUSTER_ID_GEN_IDENTIFY,
    { ATTRID_CLUSTER_REVISION, ZCL_DATATYPE_UINT16,
      ACCESS_CONTROL_READ | ACCESS_GLOBAL,
      (void *)&zclSampleSw_clusterRevision_all }
  },
};

CONST uint8 zclSampleSw_NumAttributes = ( sizeof(zclSampleSw_Attrs) / sizeof(zclSampleSw_Attrs[0]) );

// EP1~EP4: 4路继电器属性表 (genOnOff server: onOff + startUpOnOff)
CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep1[] =
{
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_ON_OFF, ZCL_DATATYPE_BOOLEAN,
    ACCESS_CONTROL_READ, (void *)&zclSampleSw_RelayState[0] } },
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_STARTUP_ON_OFF, ZCL_DATATYPE_ENUM8,
    ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_StartUpOnOff[0] } },
};

CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep2[] =
{
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_ON_OFF, ZCL_DATATYPE_BOOLEAN,
    ACCESS_CONTROL_READ, (void *)&zclSampleSw_RelayState[1] } },
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_STARTUP_ON_OFF, ZCL_DATATYPE_ENUM8,
    ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_StartUpOnOff[1] } },
};

CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep3[] =
{
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_ON_OFF, ZCL_DATATYPE_BOOLEAN,
    ACCESS_CONTROL_READ, (void *)&zclSampleSw_RelayState[2] } },
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_STARTUP_ON_OFF, ZCL_DATATYPE_ENUM8,
    ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_StartUpOnOff[2] } },
};

CONST zclAttrRec_t zclSampleSw_RelayAttrs_ep4[] =
{
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_ON_OFF, ZCL_DATATYPE_BOOLEAN,
    ACCESS_CONTROL_READ, (void *)&zclSampleSw_RelayState[3] } },
  { ZCL_CLUSTER_ID_GEN_ON_OFF, { ATTRID_STARTUP_ON_OFF, ZCL_DATATYPE_ENUM8,
    ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (void *)&zclSampleSw_StartUpOnOff[3] } },
};

// EP5~EP8: 4路输入状态属性表 (genAnalogInput server: presentValue)
CONST zclAttrRec_t zclSampleSw_InputAttrs_ep5[] =
{
  { ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC, { ATTRID_IOV_BASIC_PRESENT_VALUE,
    ZCL_DATATYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
    (void *)&zclSampleSw_InputState[0] } },
};

CONST zclAttrRec_t zclSampleSw_InputAttrs_ep6[] =
{
  { ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC, { ATTRID_IOV_BASIC_PRESENT_VALUE,
    ZCL_DATATYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
    (void *)&zclSampleSw_InputState[1] } },
};

CONST zclAttrRec_t zclSampleSw_InputAttrs_ep7[] =
{
  { ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC, { ATTRID_IOV_BASIC_PRESENT_VALUE,
    ZCL_DATATYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
    (void *)&zclSampleSw_InputState[2] } },
};

CONST zclAttrRec_t zclSampleSw_InputAttrs_ep8[] =
{
  { ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC, { ATTRID_IOV_BASIC_PRESENT_VALUE,
    ZCL_DATATYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,
    (void *)&zclSampleSw_InputState[3] } },
};

/*********************************************************************
 * SIMPLE DESCRIPTOR - 端点描述符
 */

// EP11: Basic Cluster端点 (供z2m interview)
// 包含genBasic + genIdentify输入cluster
const cId_t zclSampleSw_InClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_BASIC,
  ZCL_CLUSTER_ID_GEN_IDENTIFY,
};
#define ZCLSAMPLESW_MAX_INCLUSTERS    ( sizeof( zclSampleSw_InClusterList ) / sizeof( zclSampleSw_InClusterList[0] ))

SimpleDescriptionFormat_t zclSampleSw_SimpleDesc =
{
  SAMPLESW_ENDPOINT,                  //  int Endpoint;
  ZCL_HA_PROFILE_ID,                  //  uint16 AppProfId;
  ZCL_HA_DEVICEID_ON_OFF_LIGHT_SWITCH,//  uint16 AppDeviceId;
  SAMPLESW_DEVICE_VERSION,            //  int AppDevVer:4;
  SAMPLESW_FLAGS,                     //  int AppFlags:4;
  ZCLSAMPLESW_MAX_INCLUSTERS,         //  byte AppNumInClusters;
  (cId_t *)zclSampleSw_InClusterList, //  byte *pAppInClusterList;
  0,                                  //  byte AppNumOutClusters;
  NULL                                //  byte *pAppOutClusterList;
};

// EP1~EP4: 4路继电器端点 (genOnOff server)
const cId_t zclSampleSw_RelayInClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_ON_OFF,
};
#define ZCLSAMPLESW_MAX_RELAY_INCLUSTERS  ( sizeof( zclSampleSw_RelayInClusterList ) / sizeof( zclSampleSw_RelayInClusterList[0] ))

SimpleDescriptionFormat_t zclSampleSw_RelaySimpleDesc[SAMPLESW_NUM_RELAYS] =
{
  { SAMPLESW_ENDPOINT_RELAY1, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY2, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY3, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_RELAY4, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_ON_OFF_LIGHT,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_RELAY_INCLUSTERS, (cId_t *)zclSampleSw_RelayInClusterList, 0, NULL },
};

// EP5~EP8: 4路输入状态端点 (genAnalogInput server)
const cId_t zclSampleSw_InputInClusterList[] =
{
  ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC,
};
#define ZCLSAMPLESW_MAX_INPUT_INCLUSTERS  ( sizeof( zclSampleSw_InputInClusterList ) / sizeof( zclSampleSw_InputInClusterList[0] ))

SimpleDescriptionFormat_t zclSampleSw_InputSimpleDesc[SAMPLESW_NUM_INPUTS] =
{
  { SAMPLESW_ENDPOINT_INPUT1, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_LEVEL_CONTROL_SWITCH,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_INPUT_INCLUSTERS, (cId_t *)zclSampleSw_InputInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_INPUT2, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_LEVEL_CONTROL_SWITCH,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_INPUT_INCLUSTERS, (cId_t *)zclSampleSw_InputInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_INPUT3, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_LEVEL_CONTROL_SWITCH,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_INPUT_INCLUSTERS, (cId_t *)zclSampleSw_InputInClusterList, 0, NULL },
  { SAMPLESW_ENDPOINT_INPUT4, ZCL_HA_PROFILE_ID, ZCL_HA_DEVICEID_LEVEL_CONTROL_SWITCH,
    SAMPLESW_DEVICE_VERSION, SAMPLESW_FLAGS,
    ZCLSAMPLESW_MAX_INPUT_INCLUSTERS, (cId_t *)zclSampleSw_InputInClusterList, 0, NULL },
};

/*********************************************************************
 * ZCL CALLBACK FUNCTIONS - 命令回调函数表
 * 每个继电器端点注册独立的callbacks, 使OnOff命令能定位到正确的继电器索引
 * 回调函数实现于zcl_samplesw.c
 */

// EP1: 继电器1命令回调
// 字段顺序须与zclGeneral_AppCallbacks_t结构体定义一致(见zcl_general.h)
zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep1 =
{
  zclSampleSw_BasicResetCB,           // pfnBasicReset
  NULL,                               // pfnIdentifyTriggerEffect
  zclSampleSw_OnOffCB_ep1,            // pfnOnOff
  NULL,                               // pfnOnOff_OffWithEffect
  NULL,                               // pfnOnOff_OnWithRecallGlobalScene
  NULL,                               // pfnOnOff_OnWithTimedOff
#ifdef ZCL_LEVEL_CTRL
  NULL,                               // pfnLevelControlMoveToLevel
  NULL,                               // pfnLevelControlMove
  NULL,                               // pfnLevelControlStep
  NULL,                               // pfnLevelControlStop
#endif
#ifdef ZCL_GROUPS
  NULL,                               // pfnGroupRsp
#endif
#ifdef ZCL_SCENES
  NULL,                               // pfnSceneStoreReq
  NULL,                               // pfnSceneRecallReq
  NULL,                               // pfnSceneRsp
#endif
#ifdef ZCL_ALARMS
  NULL,                               // pfnAlarm
#endif
#ifdef SE_UK_EXT
  NULL,                               // pfnGetEventLog
  NULL,                               // pfnPublishEventLog
#endif
  NULL,                               // pfnLocation
  NULL                                // pfnLocationRsp
};

// EP2: 继电器2命令回调
zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep2 =
{
  zclSampleSw_BasicResetCB,           // pfnBasicReset
  NULL,                               // pfnIdentifyTriggerEffect
  zclSampleSw_OnOffCB_ep2,            // pfnOnOff
  NULL,                               // pfnOnOff_OffWithEffect
  NULL,                               // pfnOnOff_OnWithRecallGlobalScene
  NULL,                               // pfnOnOff_OnWithTimedOff
#ifdef ZCL_LEVEL_CTRL
  NULL,
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_GROUPS
  NULL,
#endif
#ifdef ZCL_SCENES
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_ALARMS
  NULL,
#endif
#ifdef SE_UK_EXT
  NULL,
  NULL,
#endif
  NULL,                               // pfnLocation
  NULL                                // pfnLocationRsp
};

// EP3: 继电器3命令回调
zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep3 =
{
  zclSampleSw_BasicResetCB,           // pfnBasicReset
  NULL,                               // pfnIdentifyTriggerEffect
  zclSampleSw_OnOffCB_ep3,            // pfnOnOff
  NULL,                               // pfnOnOff_OffWithEffect
  NULL,                               // pfnOnOff_OnWithRecallGlobalScene
  NULL,                               // pfnOnOff_OnWithTimedOff
#ifdef ZCL_LEVEL_CTRL
  NULL,
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_GROUPS
  NULL,
#endif
#ifdef ZCL_SCENES
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_ALARMS
  NULL,
#endif
#ifdef SE_UK_EXT
  NULL,
  NULL,
#endif
  NULL,                               // pfnLocation
  NULL                                // pfnLocationRsp
};

// EP4: 继电器4命令回调
zclGeneral_AppCallbacks_t zclSampleSw_CmdCallbacks_ep4 =
{
  zclSampleSw_BasicResetCB,           // pfnBasicReset
  NULL,                               // pfnIdentifyTriggerEffect
  zclSampleSw_OnOffCB_ep4,            // pfnOnOff
  NULL,                               // pfnOnOff_OffWithEffect
  NULL,                               // pfnOnOff_OnWithRecallGlobalScene
  NULL,                               // pfnOnOff_OnWithTimedOff
#ifdef ZCL_LEVEL_CTRL
  NULL,
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_GROUPS
  NULL,
#endif
#ifdef ZCL_SCENES
  NULL,
  NULL,
  NULL,
#endif
#ifdef ZCL_ALARMS
  NULL,
#endif
#ifdef SE_UK_EXT
  NULL,
  NULL,
#endif
  NULL,                               // pfnLocation
  NULL                                // pfnLocationRsp
};

/*********************************************************************
 * GLOBAL FUNCTIONS
 */

/*********************************************************************
 * @fn      zclSampleSw_ResetAttributesToDefaultValues
 * @brief   重置所有可写属性到默认值 (Basic Reset命令调用)
 * @return  none
 */
void zclSampleSw_ResetAttributesToDefaultValues(void)
{
  int i;

  // LocationDescription: 16字符空格
  zclSampleSw_LocationDescription[0] = 16;
  for (i = 1; i <= 16; i++)
  {
    zclSampleSw_LocationDescription[i] = ' ';
  }

  zclSampleSw_PhysicalEnvironment = DEFAULT_PHYSICAL_ENVIRONMENT;
  zclSampleSw_DeviceEnable = DEFAULT_DEVICE_ENABLE_STATE;
  zclSampleSw_IdentifyTime = DEFAULT_IDENTIFY_TIME;

  // 重置4路独立startUpOnOff为默认值(恢复断电前状态)
  for (i = 0; i < SAMPLESW_NUM_RELAYS; i++)
  {
    zclSampleSw_StartUpOnOff[i] = STARTUP_ONOFF_PREVIOUS;
  }
}

/****************************************************************************
****************************************************************************/
