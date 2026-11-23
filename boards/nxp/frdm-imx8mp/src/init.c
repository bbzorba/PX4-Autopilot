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
 * @file init.c
 *
 * NXP FRDM-IMX8MPLUS Cortex-M7 board initialization.
 */

#include <nuttx/config.h>
#include <nuttx/board.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>
#include <syslog.h>
#include <stdint.h>

#include "board_config.h"
#include "arm_internal.h"
#include "mpu.h"

#ifdef CONFIG_MX8MP_RPMSG
#include "mx8mp_rptun.h"
#endif
#include <px4_platform_common/init.h>

#ifndef OK
#  define OK 0
#endif

#define REG16(a) (*(volatile uint16_t *)(a))
#define REG32(a) (*(volatile uint32_t *)(a))

/****************************************************************************
 * WDOG3: the M7-only watchdog
 *
 * SRC_M7RCR.WDOG3_RST_OPTION = 0 (reset default): a WDOG3 timeout resets only the M7;
 * Linux on the Cortex-A53s keeps running. It is fed from the high-priority work queue,
 * so a stalled scheduler, interrupts left disabled or a hard fault loop all reset the M7.
 ****************************************************************************/

#ifdef CONFIG_BOARD_FRDM_IMX8MP_WDOG3
static struct work_s g_wdog3_work;
#define WDOG3_FEED_MS 100

static void wdog3_feed(void *arg)
{
	REG16(BOARD_WDOG3_BASE + 0x2) = 0x5555;   /* WSR service sequence */
	REG16(BOARD_WDOG3_BASE + 0x2) = 0xAAAA;
	work_queue(HPWORK, &g_wdog3_work, wdog3_feed, NULL, MSEC2TICK(WDOG3_FEED_MS));
}

void frdm_imx8mp_wdog3_start(void)
{
	/* "wdog" clock root on, WDOG3 gate on for the M7's clock domain (domain 1), so
	 * Linux's clock framework cannot switch it off */
	REG32(BOARD_CCM_BASE + 0xb900 + 4) = 1u << 28;
	REG32(BOARD_CCM_BASE + 0x4550 + 4) = 3u << 4;

	uint32_t wt = (CONFIG_BOARD_FRDM_IMX8MP_WDOG3_TIMEOUT_MS / 500);   /* 0.5 s steps */

	if (wt < 1) { wt = 1; }

	if (wt > 256) { wt = 256; }

	REG16(BOARD_WDOG3_BASE + 0x8) = 0;   /* WMCR: power-down counter off */
	/* WT | WDA (no WDOG_B pin) | SRS (no software reset) | WDE | WDBG (paused while a
	 * debugger halts the M7); keeps counting in low-power modes */
	REG16(BOARD_WDOG3_BASE + 0x0) = (uint16_t)(((wt - 1) << 8) | (1u << 5) | (1u << 4) | (1u << 2) |
					(1u << 1));

	wdog3_feed(NULL);
	syslog(LOG_INFO, "WDOG3 armed: %d ms, M7-only reset\n", CONFIG_BOARD_FRDM_IMX8MP_WDOG3_TIMEOUT_MS);
}
#else
void frdm_imx8mp_wdog3_start(void) {}
#endif

/****************************************************************************
 * Name: mx8mp_board_initialize
 *
 * Called by the chip start-up code (mx8mp_start.c) after clocks and memory are set up.
 ****************************************************************************/

#define WDOG3_BOOT_TIMEOUT_HALFSEC 20   /* 10 s window until board_app_initialize() */

/* MPU regions on top of the chip's (mx8mp_mpuinit.c, called before this);
 * higher region numbers take precedence. */
static void frdm_imx8mp_mpu_initialize(void)
{
	/* RPMsg shared memory 0x55000000-0x55FFFFFF (vrings, resource table
	 * 0x550FF000, Linux vdev buffers 0x55400000): normal, shareable,
	 * non-cacheable, so no cache maintenance for data shared with the A53s.
	 * The chip maps all DDR below 0x80000000 cacheable. */
	mpu_configure_region(0x55000000, 16 * 1024 * 1024,
			     MPU_RASR_TEX_NOR | MPU_RASR_S | MPU_RASR_AP_RWRW | MPU_RASR_XN);

	/* M7 DDR window 0x80000000-0x80FFFFFF (bbz-m7-boot, this image, its RAM):
	 * normal, write-back cacheable, executable. Not covered by the chip's map. */
	mpu_configure_region(0x80000000, 16 * 1024 * 1024,
			     MPU_RASR_TEX_NOR | MPU_RASR_C | MPU_RASR_B | MPU_RASR_AP_RWRW);

	__asm__ __volatile__("dsb\n\tisb" ::: "memory");
}

void mx8mp_board_initialize(void)
{
	frdm_imx8mp_mpu_initialize();

	/* Started by bbz-m7-boot? It leaves WDOG3 running (WDE is write-once) with its
	 * own short timeout. Refresh it and widen the timeout so NuttX start-up and
	 * px4_platform_init() cannot trip it; board_app_initialize() then sets the
	 * configured timeout and starts feeding from the work queue. */
	if (REG16(BOARD_WDOG3_BASE + 0x0) & (1u << 2)) {
		REG16(BOARD_WDOG3_BASE + 0x2) = 0x5555;
		REG16(BOARD_WDOG3_BASE + 0x2) = 0xAAAA;
		REG16(BOARD_WDOG3_BASE + 0x0) = (uint16_t)((REG16(BOARD_WDOG3_BASE + 0x0) & 0x00ffu) |
						((WDOG3_BOOT_TIMEOUT_HALFSEC - 1) << 8) | (1u << 1));
		REG16(BOARD_WDOG3_BASE + 0x2) = 0x5555;
		REG16(BOARD_WDOG3_BASE + 0x2) = 0xAAAA;
	}
}

/****************************************************************************
 * Name: board_on_reset
 ****************************************************************************/

__EXPORT void board_on_reset(int status)
{
}

/****************************************************************************
 * Name: board_app_initialize
 *
 * Called by NSH (CONFIG_NSH_ARCHINIT / BOARDIOC_INIT).
 ****************************************************************************/

int board_app_initialize(uintptr_t arg)
{
	px4_platform_init();

	frdm_imx8mp_wdog3_start();

#ifdef CONFIG_MX8MP_RPMSG
	/* RPMsg link to Linux on the A53s (shared memory 0x55000000, MU1).
	 * Linux is the master and attaches to the already-running M7. */
	int ret = mx8mp_rptun_init("imx8mp-shmem", "netcore");

	if (ret < 0) {
		syslog(LOG_ERR, "[boot] rptun init failed: %d\n", ret);
	}

#  ifdef CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY
	ret = frdm_imx8mp_rpmsg_tty_init("netcore", CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY_EPT,
					 CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY_PATH,
					 CONFIG_BOARD_FRDM_IMX8MP_RPMSG_TTY_BUFSIZE);

	if (ret < 0) {
		syslog(LOG_ERR, "[boot] rpmsg tty init failed: %d\n", ret);
	}

#  endif
#endif

	/* Configure the HW based on the manifest */
	px4_platform_configure();

	return OK;
}
