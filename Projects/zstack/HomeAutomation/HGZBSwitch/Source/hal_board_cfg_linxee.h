/**************************************************************************************************
  Filename:       hal_board_cfg_linxee.h
  Description:    linxee 自定义板级配置 (替代 TI 默认 hal_board_cfg.h)

  背景:
    TI 默认 hal_board_cfg.h 为 SmartRF05EB 评估板配置:
      - LED1~3 映射到 P1_0/P1_1/P1_4 (ACTIVE_HIGH)
      - LED4 (ENABLE_LED4_DISABLE_S1) 映射到 P0_1, 与 S1 按键复用
      - PUSH1 (S1) 映射到 P0_1
      - PUSH2 映射到 P2_0
    与 linxee 86 四路开关硬件冲突:
      - 应用层 LED1~4 使用 P0_0~P0_3 (ACTIVE_LOW, 反逻辑)
      - S1 按键使用 P1_3 (低电平有效)
      - 继电器使用 P1_0/P1_2/P1_6/P2_0 (与 TI LED1/LED3/PUSH2 冲突)
      - 触摸输入使用 P0_4~P0_7

  方案B (重定义 hal_board_cfg):
    通过 PreInclude 机制注入本文件, 预定义 HAL_BOARD_CFG_H 跳过 TI 原文件,
    使 Z-Stack 协议栈的 HalLedSet/HalLedBlink 等 API 直接操作 P0_0~P0_3,
    与应用层 LED 引脚一致, 从根本上消除协议栈与应用层 LED 引脚冲突.

  硬件: CC2530F256 + WTC6106BSI + 4路继电器 + 4路LED (P0_0~P0_3, ACTIVE_LOW)
  引脚分配:
    P0_0~P0_3: LED1~4 (输出, ACTIVE_LOW: 0=亮, 1=灭)
    P0_4~P0_7: 触摸输入 (输入, 上拉)
    P1_0:      继电器1 (输出)
    P1_2:      继电器2 (输出)
    P1_3:      S1 按键 (输入, 上拉, 低电平有效)
    P1_6:      继电器3 (输出)
    P2_0:      继电器4 (输出)

  注意: 本文件基于 TI hal_board_cfg.h 修改, 保留原文件的 Flash/时钟/DMA/RF 等配置不变,
        仅修改 LED 和按键 (PUSH) 相关定义及 HAL_BOARD_INIT().
**************************************************************************************************/

#ifndef HAL_BOARD_CFG_LINXEE_H
#define HAL_BOARD_CFG_LINXEE_H

/* ------------------------------------------------------------------------------------------------
 *                                           Includes
 * ------------------------------------------------------------------------------------------------
 */

#include "hal_mcu.h"
#include "hal_defs.h"
#include "hal_types.h"

/* ------------------------------------------------------------------------------------------------
 *                                       CC2590/CC2591 support
 *
 *                        Define HAL_PA_LNA_CC2590 if CC2530+CC2590EM is used
 *                        Define HAL_PA_LNA if CC2530+CC2591EM is used
 *                        Note that only one of them can be defined
 * ------------------------------------------------------------------------------------------------
 */
#define xHAL_PA_LNA
#define xHAL_PA_LNA_CC2590
#define xHAL_PA_LNA_SE2431L
#define xHAL_PA_LNA_CC2592

/* ------------------------------------------------------------------------------------------------
 *                                       Board Indentifier
 *
 *      Define the Board Identifier to HAL_BOARD_CC2530EB_REV13 for SmartRF05 EB 1.3 board
 * ------------------------------------------------------------------------------------------------
 */

/* linxee: 强制使用 REV17 配置路径 (无 PA/LNA) */
#define HAL_BOARD_CC2530EB_REV17

/* ------------------------------------------------------------------------------------------------
 *                                          Clock Speed
 * ------------------------------------------------------------------------------------------------
 */

#define HAL_CPU_CLOCK_MHZ     32

/* This flag should be defined if the SoC uses the 32MHz crystal
 * as the main clock source (instead of DCO).
 */
#define HAL_CLOCK_CRYSTAL

/* 32 kHz clock source select in CLKCONCMD */
#if !defined (OSC32K_CRYSTAL_INSTALLED) || (defined (OSC32K_CRYSTAL_INSTALLED) && (OSC32K_CRYSTAL_INSTALLED == TRUE))
  #define OSC_32KHZ  0x00 /* external 32 KHz xosc */
#else
  #define OSC_32KHZ  0x80 /* internal 32 KHz rcosc */
#endif

#define HAL_CLOCK_STABLE()    st( while (CLKCONSTA != (CLKCONCMD_32MHZ | OSC_32KHZ)); )

/* ------------------------------------------------------------------------------------------------
 *                                       LED Configuration
 *
 * linxee 修改: LED1~4 映射到 P0_0~P0_3, 极性 ACTIVE_LOW (反逻辑: 0=亮, 1=灭)
 *              移除 ENABLE_LED4_DISABLE_S1 条件, LED4 独立映射到 P0_3
 *              HAL_NUM_LEDS = 4 (协议栈 HalLedSet 可操作 4 路 LED)
 * ------------------------------------------------------------------------------------------------
 */

#define HAL_NUM_LEDS            4

#define HAL_LED_BLINK_DELAY()   st( { volatile uint32 i; for (i=0; i<0x5800; i++) { }; } )

/* linxee LED 映射: P0_0~P0_3, ACTIVE_LOW */
/* 1 - LED1 (P0_0) - 状态指示灯 (配网慢闪/继电器1联动) */
#define LED1_BV           BV(0)
#define LED1_SBIT         P0_0
#define LED1_DDR          P0DIR
#define LED1_POLARITY     ACTIVE_LOW
#define LED1_SET_DIR()    do {LED1_DDR |= LED1_BV;} while (0)

/* 2 - LED2 (P0_1) - 继电器2联动 */
#define LED2_BV           BV(1)
#define LED2_SBIT         P0_1
#define LED2_DDR          P0DIR
#define LED2_POLARITY     ACTIVE_LOW
#define LED2_SET_DIR()    do {LED2_DDR |= LED2_BV;} while (0)

/* 3 - LED3 (P0_2) - 继电器3联动 */
#define LED3_BV           BV(2)
#define LED3_SBIT         P0_2
#define LED3_DDR          P0DIR
#define LED3_POLARITY     ACTIVE_LOW
#define LED3_SET_DIR()    do {LED3_DDR |= LED3_BV;} while (0)

/* 4 - LED4 (P0_3) - 继电器4联动 */
#define LED4_BV           BV(3)
#define LED4_SBIT         P0_3
#define LED4_DDR          P0DIR
#define LED4_POLARITY     ACTIVE_LOW
#define LED4_SET_DIR()    do {LED4_DDR |= LED4_BV;} while (0)

/* ------------------------------------------------------------------------------------------------
 *                                    Push Button Configuration
 *
 * linxee 修改: S1 映射到 P1_3 (原 TI 为 P0_1), 极性 ACTIVE_LOW
 *              PUSH2 保留原定义但不使用 (P2_0 已用作继电器4输出)
 * ------------------------------------------------------------------------------------------------
 */

#define ACTIVE_LOW        !
#define ACTIVE_HIGH       !!    /* double negation forces result to be '1' */

/* S1 - linxee: P1_3, 低电平有效 (WTC6106BSI 触摸芯片 TKEY4 输出) */
#define PUSH1_BV          BV(3)
#define PUSH1_SBIT        P1_3
#define PUSH1_POLARITY    ACTIVE_LOW

/* Joystick Center Press - 保留原定义 (linxee 不使用, P2_0 已用作继电器4) */
#define PUSH2_BV          BV(0)
#define PUSH2_SBIT        P2_0
#define PUSH2_POLARITY    ACTIVE_HIGH

/* ------------------------------------------------------------------------------------------------
 *                                    LCD Configuration
 * ------------------------------------------------------------------------------------------------
 */

/* LCD Max Chars and Buffer */
#define HAL_LCD_MAX_CHARS   16
#define HAL_LCD_MAX_BUFF    25

/* ------------------------------------------------------------------------------------------------
 *                         OSAL NV implemented by internal flash pages.
 * ------------------------------------------------------------------------------------------------
 */

// Flash is partitioned into 8 banks of 32 KB or 16 pages.
#define HAL_FLASH_PAGE_PER_BANK    16
// Flash is constructed of 128 pages of 2 KB.
#define HAL_FLASH_PAGE_SIZE        2048
#define HAL_FLASH_WORD_SIZE        4

// CODE banks get mapped into the XDATA range 8000-FFFF.
#define HAL_FLASH_PAGE_MAP         0x8000

// The last 16 bytes of the last available page are reserved for flash lock bits.
// NV page definitions must coincide with segment declaration in project *.xcl file.
#if defined NON_BANKED
#define HAL_FLASH_LOCK_BITS        16
#define HAL_NV_PAGE_END            30
#define HAL_NV_PAGE_CNT            2
#else
#define HAL_FLASH_LOCK_BITS        16
#define HAL_NV_PAGE_END            126
#define HAL_NV_PAGE_CNT            6
#endif

// Re-defining Z_EXTADDR_LEN here so as not to include a Z-Stack .h file.
#define HAL_FLASH_IEEE_SIZE        8
#define HAL_FLASH_IEEE_PAGE       (HAL_NV_PAGE_END+1)
#define HAL_FLASH_IEEE_OSET       (HAL_FLASH_PAGE_SIZE - HAL_FLASH_LOCK_BITS - HAL_FLASH_IEEE_SIZE)
#define HAL_INFOP_IEEE_OSET        0xC

#define HAL_FLASH_DEV_PRIVATE_KEY_OSET     0x7D2
#define HAL_FLASH_CA_PUBLIC_KEY_OSET       0x7BC
#define HAL_FLASH_IMPLICIT_CERT_OSET       0x78C

#define HAL_NV_PAGE_BEG           (HAL_NV_PAGE_END-HAL_NV_PAGE_CNT+1)

// Used by DMA macros to shift 1 to create a mask for DMA registers.
#define HAL_NV_DMA_CH              0
#define HAL_DMA_CH_RX              3
#define HAL_DMA_CH_TX              4

#define HAL_NV_DMA_GET_DESC()      HAL_DMA_GET_DESC0()
#define HAL_NV_DMA_SET_ADDR(a)     HAL_DMA_SET_ADDR_DESC0((a))

/* ------------------------------------------------------------------------------------------------
 *  Serial Boot Loader: reserving the first 4 pages of flash and other memory in cc2530-sb.xcl.
 * ------------------------------------------------------------------------------------------------
 */

#define HAL_SB_IMG_ADDR       0x2000
#define HAL_SB_CRC_ADDR       0x2090
// Size of internal flash less 4 pages for boot loader, 6 pages for NV, & 1 page for lock bits.
#define HAL_SB_IMG_SIZE      (0x40000 - 0x2000 - 0x3000 - 0x0800)

/* ------------------------------------------------------------------------------------------------
 *                                            Macros
 * ------------------------------------------------------------------------------------------------
 */

/* ----------- RF-frontend Connection Initialization ---------- */
#if defined HAL_PA_LNA || defined HAL_PA_LNA_CC2590 || \
    defined HAL_PA_LNA_SE2431L || defined HAL_PA_LNA_CC2592
extern void MAC_RfFrontendSetup(void);
#define HAL_BOARD_RF_FRONTEND_SETUP() MAC_RfFrontendSetup()
#else
#define HAL_BOARD_RF_FRONTEND_SETUP()
#endif

/* ----------- Cache Prefetch control ---------- */
#define PREFETCH_ENABLE()     st( FCTL = 0x08; )
#define PREFETCH_DISABLE()    st( FCTL = 0x04; )

/* ----------- Board Initialization ---------- */
/* linxee 修改:
 *   1. LED1~4 初始化为 P0_0~P0_3 输出, 初始关闭 (输出1=灭, ACTIVE_LOW)
 *   2. 移除 P0INP |= PUSH2_BV (原 TI 代码会设置 P0_0 三态, 影响 LED1 输出)
 *   3. 保留时钟/Cache/RF 初始化不变
 */
#define HAL_BOARD_INIT()                                         \
{                                                                \
  uint16 i;                                                      \
                                                                 \
  SLEEPCMD &= ~OSC_PD;                       /* turn on 16MHz RC and 32MHz XOSC */                \
  while (!(SLEEPSTA & XOSC_STB));            /* wait for 32MHz XOSC stable */                     \
  asm("NOP");                                /* chip bug workaround */                            \
  for (i=0; i<504; i++) asm("NOP");          /* Require 63us delay for all revs */                \
  CLKCONCMD = (CLKCONCMD_32MHZ | OSC_32KHZ); /* Select 32MHz XOSC and the source for 32K clock */ \
  while (CLKCONSTA != (CLKCONCMD_32MHZ | OSC_32KHZ)); /* Wait for the change to be effective */   \
  SLEEPCMD |= OSC_PD;                        /* turn off 16MHz RC */                              \
                                                                 \
  /* Turn on cache prefetch mode */                              \
  PREFETCH_ENABLE();                                             \
                                                                 \
  /* linxee: LED1~4 初始化 (P0_0~P0_3, ACTIVE_LOW) */            \
  HAL_TURN_OFF_LED1();                                           \
  LED1_SET_DIR();                                                \
  HAL_TURN_OFF_LED2();                                           \
  LED2_SET_DIR();                                                \
  HAL_TURN_OFF_LED3();                                           \
  LED3_SET_DIR();                                                \
  HAL_TURN_OFF_LED4();                                           \
  LED4_SET_DIR();                                                \
}

/* ----------- Debounce ---------- */
#define HAL_DEBOUNCE(expr)    { int i; for (i=0; i<500; i++) { if (!(expr)) i = 0; } }

/* ----------- Push Buttons ---------- */
#define HAL_PUSH_BUTTON1()        (PUSH1_POLARITY (PUSH1_SBIT))
#define HAL_PUSH_BUTTON2()        (PUSH2_POLARITY (PUSH2_SBIT))
#define HAL_PUSH_BUTTON3()        (0)
#define HAL_PUSH_BUTTON4()        (0)
#define HAL_PUSH_BUTTON5()        (0)
#define HAL_PUSH_BUTTON6()        (0)

/* ----------- LED's ---------- */
/* linxee: 所有 LED 宏基于 P0_0~P0_3, ACTIVE_LOW */
#define HAL_TURN_OFF_LED1()       st( LED1_SBIT = LED1_POLARITY (0); )
#define HAL_TURN_OFF_LED2()       st( LED2_SBIT = LED2_POLARITY (0); )
#define HAL_TURN_OFF_LED3()       st( LED3_SBIT = LED3_POLARITY (0); )
#define HAL_TURN_OFF_LED4()       st( LED4_SBIT = LED4_POLARITY (0); )

#define HAL_TURN_ON_LED1()        st( LED1_SBIT = LED1_POLARITY (1); )
#define HAL_TURN_ON_LED2()        st( LED2_SBIT = LED2_POLARITY (1); )
#define HAL_TURN_ON_LED3()        st( LED3_SBIT = LED3_POLARITY (1); )
#define HAL_TURN_ON_LED4()        st( LED4_SBIT = LED4_POLARITY (1); )

#define HAL_TOGGLE_LED1()         st( if (LED1_SBIT) { LED1_SBIT = 0; } else { LED1_SBIT = 1;} )
#define HAL_TOGGLE_LED2()         st( if (LED2_SBIT) { LED2_SBIT = 0; } else { LED2_SBIT = 1;} )
#define HAL_TOGGLE_LED3()         st( if (LED3_SBIT) { LED3_SBIT = 0; } else { LED3_SBIT = 1;} )
#define HAL_TOGGLE_LED4()         st( if (LED4_SBIT) { LED4_SBIT = 0; } else { LED4_SBIT = 1;} )

#define HAL_STATE_LED1()          (LED1_POLARITY (LED1_SBIT))
#define HAL_STATE_LED2()          (LED2_POLARITY (LED2_SBIT))
#define HAL_STATE_LED3()          (LED3_POLARITY (LED3_SBIT))
#define HAL_STATE_LED4()          (LED4_POLARITY (LED4_SBIT))

/* ----------- XNV ---------- */
#define XNV_SPI_BEGIN()             st(P1_3 = 0;)
#define XNV_SPI_TX(x)               st(U1CSR &= ~0x02; U1DBUF = (x);)
#define XNV_SPI_RX()                U1DBUF
#define XNV_SPI_WAIT_RXRDY()        st(while (!(U1CSR & 0x02));)
#define XNV_SPI_END()               st(P1_3 = 1;)

// The TI reference design uses UART1 Alt. 2 in SPI mode.
#define XNV_SPI_INIT() \
st( \
  /* Mode select UART1 SPI Mode as master. */\
  U1CSR = 0; \
  \
  /* Setup for 115200 baud. */\
  U1GCR = 11; \
  U1BAUD = 216; \
  \
  /* Set bit order to MSB */\
  U1GCR |= BV(5); \
  \
  /* Set UART1 I/O to alternate 2 location on P1 pins. */\
  PERCFG |= 0x02;  /* U1CFG */\
  \
  /* Select peripheral function on I/O pins but SS is left as GPIO for separate control. */\
  P1SEL |= 0xE0;  /* SELP1_[7:4] */\
  /* P1.1,2,3: reset, LCD CS, XNV CS. */\
  P1SEL &= ~0x0E; \
  P1 |= 0x0E; \
  P1_1 = 0; \
  P1DIR |= 0x0E; \
  \
  /* Give UART1 priority over Timer3. */\
  P2SEL &= ~0x20;  /* PRI2P1 */\
  \
  /* When SPI config is complete, enable it. */\
  U1CSR |= 0x40; \
  /* Release XNV reset. */\
  P1_1 = 1; \
)

/* ----------- Minimum safe bus voltage ---------- */

// Vdd/3 / Internal Reference X ENOB --> (Vdd / 3) / 1.15 X 127
#define VDD_2_0  74   // 2.0 V required to safely read/write internal flash.
#define VDD_2_7  100  // 2.7 V required for the Numonyx device.
#define VDD_MIN_RUN  (VDD_2_0+4)  // VDD_MIN_RUN = VDD_MIN_NV
#define VDD_MIN_NV   (VDD_2_0+4)  // 5% margin over minimum to survive a page erase and compaction.
#define VDD_MIN_GOOD (VDD_2_0+8)  // 10% margin over minimum to survive a page erase and compaction.
#define VDD_MIN_XNV  (VDD_2_7+5)  // 5% margin over minimum to survive a page erase and compaction.

/* ------------------------------------------------------------------------------------------------
 *                                     Driver Configuration
 * ------------------------------------------------------------------------------------------------
 */

/* Set to TRUE enable H/W TIMER usage, FALSE disable it */
#ifndef HAL_TIMER
#define HAL_TIMER FALSE
#endif

/* Set to TRUE enable ADC usage, FALSE disable it */
#ifndef HAL_ADC
#define HAL_ADC TRUE
#endif

/* Set to TRUE enable DMA usage, FALSE disable it */
#ifndef HAL_DMA
#define HAL_DMA TRUE
#endif

/* Set to TRUE enable Flash access, FALSE disable it */
#ifndef HAL_FLASH
#define HAL_FLASH TRUE
#endif

/* Set to TRUE enable AES usage, FALSE disable it */
#ifndef HAL_AES
#define HAL_AES TRUE
#endif

#ifndef HAL_AES_DMA
#define HAL_AES_DMA TRUE
#endif

/* Set to TRUE enable LCD usage, FALSE disable it */
#ifndef HAL_LCD
#define HAL_LCD TRUE
#endif

/* Set to TRUE enable LED usage, FALSE disable it */
#ifndef HAL_LED
#define HAL_LED TRUE
#endif
#if (!defined BLINK_LEDS) && (HAL_LED == TRUE)
#define BLINK_LEDS
#endif

/* Set to TRUE enable KEY usage, FALSE disable it */
#ifndef HAL_KEY
#define HAL_KEY TRUE
#endif

/* Set to TRUE enable UART usage, FALSE disable it */
#ifndef HAL_UART
#if (defined ZAPP_P1) || (defined ZAPP_P2) || (defined ZTOOL_P1) || (defined ZTOOL_P2)
#define HAL_UART TRUE
#else
#define HAL_UART FALSE
#endif
#endif

#if HAL_UART
#ifndef HAL_UART_DMA
#if HAL_DMA
#if (defined ZAPP_P2) || (defined ZTOOL_P2)
#define HAL_UART_DMA  2
#else
#define HAL_UART_DMA  1
#endif
#else
#define HAL_UART_DMA  0
#endif
#endif

#ifndef HAL_UART_ISR
#if HAL_UART_DMA           // Default preference for DMA over ISR.
#define HAL_UART_ISR  0
#elif (defined ZAPP_P2) || (defined ZTOOL_P2)
#define HAL_UART_ISR  2
#else
#define HAL_UART_ISR  1
#endif
#endif

#if (HAL_UART_DMA && (HAL_UART_DMA == HAL_UART_ISR))
#error HAL_UART_DMA & HAL_UART_ISR must be different.
#endif

// Used to set P2 priority - USART0 over USART1 if both are defined.
#if ((HAL_UART_DMA == 1) || (HAL_UART_ISR == 1))
#define HAL_UART_PRIPO             0x00
#else
#define HAL_UART_PRIPO             0x40
#endif

#else
#define HAL_UART_DMA  0
#define HAL_UART_ISR  0
#endif

/* USB is not used for CC2530 configuration */
#define HAL_UART_USB  0

#endif /* HAL_BOARD_CFG_LINXEE_H */
