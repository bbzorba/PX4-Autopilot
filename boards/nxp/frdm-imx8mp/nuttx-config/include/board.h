/****************************************************************************
 * boards/nxp/frdm-imx8mp/nuttx-config/include/board.h
 *
 * NXP FRDM-IMX8MPLUS, Cortex-M7 side.
 ****************************************************************************/

#pragma once

#include <nuttx/config.h>

#ifndef __ASSEMBLY__
#  include <stdint.h>
#  include <stdbool.h>
#endif

/* Console: UART4, the second serial port of the J19 debug USB-C (/dev/ttyACM1 on the PC).
 * The Linux device tree (imx8mp-bbz.dts) leaves UART4 to the M7.
 */
#define IOMUX_CONSOLE_UART_RX IOMUXC_UART4_RXD_UART4_RX, 0, UART_PAD_CTRL
#define IOMUX_CONSOLE_UART_TX IOMUXC_UART4_TXD_UART4_TX, 0, UART_PAD_CTRL

/* Clocks: the M7 is started by U-Boot (bootaux) or Linux remoteproc, which already
 * configured the M7 core clock (200 MHz from SYS_PLL2 under U-Boot, 800 MHz possible).
 */
#define BOARD_CPU_FREQUENCY   800000000
#define BOARD_GPT_FREQUENCY   24000000   /* GPT1 root = 24 MHz oscillator (hrt) */
