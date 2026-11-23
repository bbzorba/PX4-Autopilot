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
#include <nuttx/irq.h>
#include <arch/irq.h>
#include <nuttx/wqueue.h>
#include <nuttx/clock.h>
#include <syslog.h>
#include <stdint.h>

#include "board_config.h"
#include "arm_internal.h"
#include "mpu.h"
#include "mx8mp_ccm.h"
#include "mx8mp_clockconfig.h"
#include "hardware/mx8mp_ccm.h"

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

/* GPIO banks are shared with Linux. NuttX's mx8mp up_irqinitialize() runs
 * mx8mp_gpio_irq_initialize(), which clears GPIO_IMR of all five banks and
 * enables all ten GPIO bank interrupts on the M7. A GPIO interrupt Linux
 * enables then also fires on the M7, has no handler there and panics NuttX;
 * an M7 restart would also wipe Linux's interrupt masks. Save the masks here
 * (before the IRQ init) and undo both in board_early_initialize(). */

#define FRDM_GPIO_BANKS 5
#define FRDM_GPIO_IMR(n) (*(volatile uint32_t *)(0x30200000u + 0x10000u * (n) + 0x14u))

static uint32_t g_gpio_imr[FRDM_GPIO_BANKS];

void board_early_initialize(void)
{
	for (int irq = MX8MP_IRQ_GPIO1_0_15; irq <= MX8MP_IRQ_GPIO5_16_31; irq++) {
		up_disable_irq(irq);
		irq_detach(irq);
	}

	for (int n = 0; n < FRDM_GPIO_BANKS; n++) {
		FRDM_GPIO_IMR(n) = g_gpio_imr[n];
	}
}

/* Clock gates of the peripherals the M7 touches. Each CCM_CCGR gate has per-domain
 * request bits; Linux (domain 0) only clears its own [1:0]. Requesting the gate for
 * the M7's domain 1 ([5:4] = 3, "needed always") keeps it running whatever Linux does.
 * Without this, an M7 access to a gate Linux switched off (OCOTP is enabled by Linux
 * only while it reads fuses) never completes and freezes the whole SoC.
 * The clock roots behind GPT1, WDOG3 and UART4 must also stay on: init-on-array in
 * the Linux device tree. OCOTP, MU and GPIO run from ipg_root, which is always on. */

/* CCGR index n is at CCM + 0x4000 + 0x10 * n */
#define CCGR(n) (0x4000 + 0x10 * (n))

static const uint16_t g_m7_ccgr[] = {
	CCGR(27), CCGR(28), CCGR(29), CCGR(30), /* IOMUX, IPMUX1-3 */
	CCGR(35), CCGR(49), /* OCRAM, RDC */
	CCGR(65), CCGR(66), CCGR(67), CCGR(68), /* SIM_M/MAIN/S/WAKEUP: M7 bus while the A53s sleep */
	CCGR(4), CCGR(60), /* DEBUG, SEC_DEBUG: JTAG */
	0x4220, /* OCOTP: UUID / GUID */
	0x4210, /* MU: RPMsg doorbells */
	0x40b0, 0x40c0, 0x40d0, 0x40e0, 0x40f0, /* GPIO1-5: IMR at IRQ init */
	0x4100, /* GPT1: HRT */
	0x4550, /* WDOG3 */
	0x44c0, /* UART4: console */
};

static void frdm_imx8mp_claim_clocks(void)
{
	for (unsigned i = 0; i < sizeof(g_m7_ccgr) / sizeof(g_m7_ccgr[0]); i++) {
		REG32(BOARD_CCM_BASE + g_m7_ccgr[i] + 4) = 3u << 4; /* CCGR_SET, domain 1 */
	}
}

/****************************************************************************
 * Name: mx8mp_clockconfig
 *
 * Replaces NuttX's arch/arm/src/mx8mp/mx8mp_clockconfig.c: the board links with
 * -Wl,--wrap=mx8mp_clockconfig (src/CMakeLists.txt). The upstream version comes
 * from an M7-owns-audio
 * example: at every M7 start it maps the AUDIOMIX power domain to the M7, powers
 * it up and reprograms AUDIO_PLL1/2 and the audio bus clocks. Here Linux owns
 * audio and the HiFi4 DSP and is often already running when the M7 (re)starts;
 * changing its PLLs/power domain under it brought the A53 side down. Configure
 * only what belongs to the M7: its own clock root and its domain-1 gate requests
 * (frdm_imx8mp_claim_clocks()). PLLs, bus roots and power domains stay Linux's.
 ****************************************************************************/

void __wrap_mx8mp_clockconfig(void);
void __wrap_mx8mp_clockconfig(void)
{
	mx8mp_ccm_gate_pll(SYSTEM_PLL1_CLK, CLK_ALWAYS_NEEDED);
	mx8mp_ccm_configure_clock(ARM_M7_CLK_ROOT, SYSTEM_PLL1_CLK, 1, 1); /* 800 MHz */
}

void mx8mp_board_initialize(void)
{
	frdm_imx8mp_claim_clocks();
	frdm_imx8mp_mpu_initialize();

	for (int n = 0; n < FRDM_GPIO_BANKS; n++) {
		g_gpio_imr[n] = FRDM_GPIO_IMR(n);
	}

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
