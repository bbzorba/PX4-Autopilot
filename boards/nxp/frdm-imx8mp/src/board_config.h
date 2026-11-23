/****************************************************************************
 *
 *   Copyright (c) 2026 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 *
 ****************************************************************************/

/**
 * @file board_config.h
 *
 * NXP FRDM-IMX8MPLUS, Cortex-M7 side (PX4 board nxp/frdm-imx8mp).
 *
 * The M7 shares the i.MX 8M Plus with Linux on the 4x Cortex-A53. Which peripherals
 * belong to the M7 is decided in the Linux device tree (imx8mp-bbz.dts): UART4
 * (console), FlexCAN1, PWM4 and WDOG3. Everything else stays with Linux.
 */

#pragma once

#include <px4_platform_common/px4_config.h>
#include <nuttx/compiler.h>
#include <stdint.h>

__BEGIN_DECLS

#include <arch/board/board.h>

/* High resolution timer: GPT1 (0x302D0000, IRQ 55), 24 MHz oscillator root */
#define HRT_TIMER                1
#define HRT_TIMER_CHANNEL        1  /* output compare channel 1 */

/* No LEDs, PWM outputs, ADC or sensors on the M7 side yet */
#define BOARD_HAS_NO_LEDS        1
#define DIRECT_PWM_OUTPUT_CHANNELS 0
#define BOARD_NUMBER_BRICKS      0

/* Console log buffer for dmesg */
#define BOARD_ENABLE_CONSOLE_BUFFER

/* WDOG3: M7-only watchdog (resets only the M7, Linux keeps running) */
#define BOARD_WDOG3_BASE         0x302A0000
#define BOARD_CCM_BASE           0x30380000
#define BOARD_SRC_M7RCR          0x3039000C

#ifndef __ASSEMBLY__

void frdm_imx8mp_wdog3_start(void);
int frdm_imx8mp_rpmsg_tty_init(const char *cpuname, const char *eptname, const char *devpath, int bufsize);

#include <px4_platform_common/board_common.h>

#endif /* __ASSEMBLY__ */

__END_DECLS
