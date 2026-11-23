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

int board_reset(int status)
{
#if defined(BOARD_HAS_ON_RESET)
	board_on_reset(status);
#endif
	up_systemreset();
	return 0;
}
