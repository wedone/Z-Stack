/**************************************************************************************************
  Filename:       preinclude.h
  Description:    IAR PreInclude 文件 - 在每个编译单元开头注入项目级配置

  作用:
    1. 禁用 HAL_KEY 驱动 (HAL_KEY=FALSE)
       原因: hal_key.c 的周期性 ADC 读取 (P0_6/AIN6) 会切换 GPIO 模式,
             干扰 WTC6106BSI 触摸IC稳定性和继电器输出.
             linxee 使用应用层 50ms 轮询检测 S1 (P1_3), 不依赖 HAL_KEY 驱动.

    2. 方案B: 重定义板级配置
       通过预定义 HAL_BOARD_CFG_H 跳过 TI 默认 hal_board_cfg.h,
       改为包含 hal_board_cfg_linxee.h (LED1~4 → P0_0~P0_3, S1 → P1_3).
       使 Z-Stack 协议栈 HalLedSet/HalLedBlink 等 API 直接操作应用层 LED 引脚,
       从根本上消除协议栈与应用层 LED 引脚冲突.

  使用方式:
    HGZBSwitch.ewp → C/C++ Compiler → Preprocessor → PreInclude
    填入: $PROJ_DIR$\..\Source\preinclude.h

  注意: 本文件由 IAR PreInclude 机制在每个 .c 文件编译前自动注入,
        等效于在每个 .c 文件开头添加 #include "preinclude.h".
        f8wConfig.cfg 的 -D 选项比本文件更早生效.
**************************************************************************************************/

#ifndef PREINCLUDE_H
#define PREINCLUDE_H

/* ------------------------------------------------------------------------------------------------
 * 1. 禁用 HAL_KEY 驱动
 *    hal_key.c 的 HalKeyPoll() 会周期性读取 P0_6 (AIN6) ADC, 切换 GPIO 模式,
 *    干扰 WTC6106BSI 触摸IC (P0_4~P0_7) 和继电器输出.
 *    linxee 使用应用层 zclSampleSw_ProcessTouchPoll() 替代, 不需要 HAL_KEY.
 * ------------------------------------------------------------------------------------------------
 */
#ifndef HAL_KEY
#define HAL_KEY FALSE
#endif

/* ------------------------------------------------------------------------------------------------
 * 2. 方案B: 重定义板级配置
 *    预定义 HAL_BOARD_CFG_H, 使 TI 原 hal_board_cfg.h 的 include guard 生效,
 *    后续 #include "hal_board_cfg.h" 将被跳过.
 *    同时包含 hal_board_cfg_linxee.h 提供自定义配置.
 * ------------------------------------------------------------------------------------------------
 */
#define HAL_BOARD_CFG_H

#include "hal_board_cfg_linxee.h"

#endif /* PREINCLUDE_H */
