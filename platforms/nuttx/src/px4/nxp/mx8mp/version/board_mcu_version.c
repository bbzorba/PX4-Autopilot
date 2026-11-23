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
 * @file board_mcu_version.c
 *
 * i.MX 8M Plus silicon revision from CCM_ANALOG_DIGPROG.
 */

#include <px4_platform_common/px4_config.h>
#include <px4_platform_common/defines.h>
#include "arm_internal.h"

#define MX8MP_ANATOP_DIGPROG  0x30360800

#define CHIP_TAG     "i.MX 8M Plus M7 r?.?"
#define CHIP_TAG_LEN sizeof(CHIP_TAG)-1

int board_mcu_version(char *rev, const char **revstr, const char **errata)
{
	uint32_t info = getreg32(MX8MP_ANATOP_DIGPROG);   /* 0x82400x: x = minor revision */
	static char chip[sizeof(CHIP_TAG)] = CHIP_TAG;

	chip[CHIP_TAG_LEN - 3] = '1' + ((info >> 4) & 0xf);
	chip[CHIP_TAG_LEN - 1] = '0' + (info & 0xf);
	*revstr = chip;
	*rev = '0' + (info & 0xf);

	if (errata) {
		*errata = NULL;
	}

	return 0;
}
