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
 ****************************************************************************/

/**
 * @file micro_hal.h
 *
 * i.MX 8M Plus Cortex-M7 (NuttX custom chip "mx8mp", see boards/nxp/frdm-imx8mp).
 */

#pragma once

/* Cortex-M7 L1 cache line; NuttX's mx8mp chip.h (unlike imxrt) does not define these */
#ifndef ARMV7M_DCACHE_LINESIZE
#  define ARMV7M_DCACHE_LINESIZE 32
#endif
#ifndef ARMV7M_ICACHE_LINESIZE
#  define ARMV7M_ICACHE_LINESIZE 32
#endif

#include "../../../nxp_common/include/px4_arch/micro_hal.h"

__BEGIN_DECLS

#define PX4_SOC_ARCH_ID             PX4_SOC_ARCH_ID_NXPIMX8MP

#define PX4_NUMBER_I2C_BUSES        6

#define GPIO_OUTPUT_SET             GPIO_OUTPUT_ONE
#define GPIO_OUTPUT_CLEAR           GPIO_OUTPUT_ZERO

#include <chip.h>
#include <mx8mp_gpio.h>
#include <mx8mp_i2c.h>
#include <mx8mp_ecspi.h>

#define PX4_CPU_UUID_BYTE_LENGTH                8
#define PX4_CPU_UUID_WORD32_LENGTH              (PX4_CPU_UUID_BYTE_LENGTH/sizeof(uint32_t))
#define PX4_CPU_MFGUID_BYTE_LENGTH              PX4_CPU_UUID_BYTE_LENGTH
#define PX4_CPU_UUID_WORD32_FORMAT              "%08x"
#define PX4_CPU_UUID_WORD32_SEPARATOR           ":"
#define PX4_CPU_UUID_WORD32_UNIQUE_H            0
#define PX4_CPU_UUID_WORD32_UNIQUE_M            1
#define PX4_CPU_UUID_WORD32_FORMAT_SIZE         (PX4_CPU_UUID_WORD32_LENGTH-1+(2*PX4_CPU_UUID_BYTE_LENGTH)+1)
#define PX4_CPU_MFGUID_FORMAT_SIZE              ((2*PX4_CPU_MFGUID_BYTE_LENGTH)+1)

#define PX4_BUS_OFFSET       0                  /* buses are 1 based, no adjustment needed */

#define px4_spibus_initialize(bus_num_1based)   mx8mp_spibus_initialize(PX4_BUS_NUMBER_FROM_PX4(bus_num_1based))
#define px4_i2cbus_initialize(bus_num_1based)   mx8mp_i2cbus_initialize(PX4_BUS_NUMBER_FROM_PX4(bus_num_1based))
#define px4_i2cbus_uninitialize(pdev)           mx8mp_i2cbus_uninitialize(pdev)

#define px4_arch_configgpio(pinset)             mx8mp_gpio_config(pinset)
#define px4_arch_unconfiggpio(pinset)
#define px4_arch_gpioread(pinset)               mx8mp_gpio_read(pinset)
#define px4_arch_gpiowrite(pinset, value)       mx8mp_gpio_write(pinset, value)
#define px4_arch_gpiosetevent(pinset,r,f,e,fp,a) (-1)

#define PX4_MAKE_GPIO_INPUT(gpio)               (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | GPIO_INPUT)
#define PX4_MAKE_GPIO_PULLED_INPUT(gpio, pull)  (PX4_MAKE_GPIO_INPUT((gpio)) | (pull))
#define PX4_MAKE_GPIO_OUTPUT_CLEAR(gpio)        (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | GPIO_OUTPUT | GPIO_OUTPUT_ZERO)
#define PX4_MAKE_GPIO_OUTPUT_SET(gpio)          (((gpio) & (GPIO_PORT_MASK | GPIO_PIN_MASK)) | GPIO_OUTPUT | GPIO_OUTPUT_ONE)

__END_DECLS
