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
 * @file board_reset.cpp
 *
 * i.MX 8M Plus Cortex-M7: reset. up_systemreset() (SYSRESETREQ) resets only the M7;
 * Linux on the Cortex-A53s is not affected. There is no PX4 USB/ISP bootloader on this
 * core: the M7 image is loaded by U-Boot or bbz-m7-boot.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/shutdown.h>
#include <errno.h>
#include <nuttx/board.h>
#include <arm_internal.h>

/* On the i.MX 8M Plus the M7's SYSRESETREQ (up_systemreset) resets the whole
 * SoC, Linux on the A53s included. Reset only the M7 instead, through its own
 * watchdog WDOG3: SRC_M7RCR.WDOG3_RST_OPTION = 0 (reset default) routes a
 * WDOG3 reset to the M7 only. */

#define WDOG3_BASE 0x302A0000u
#define WDOG3_WCR  (*(volatile uint16_t *)(WDOG3_BASE + 0x0))
#define CCM_WDOG_ROOT_SET  (*(volatile uint32_t *)(0x30380000u + 0xb900u + 4u))
#define CCM_WDOG3_GATE_SET (*(volatile uint32_t *)(0x30380000u + 0x4550u + 4u))

static void mx8mp_m7_reset(void)
{
	CCM_WDOG_ROOT_SET = 1u << 28;   /* wdog clock root on */
	CCM_WDOG3_GATE_SET = 3u << 4;   /* WDOG3 gate on for the M7 domain */

	/* Shortest timeout (0.5 s), WDA | WDE; SRS = 0 asserts the software
	 * reset right away. Whichever comes first resets the M7. */
	WDOG3_WCR = (uint16_t)((0u << 8) | (1u << 5) | (1u << 2));

	for (;;) {
	}
}

int board_reset(int status)
{
#if defined(BOARD_HAS_ON_RESET)
	board_on_reset(status);
#endif
	mx8mp_m7_reset();
	return 0;
}
