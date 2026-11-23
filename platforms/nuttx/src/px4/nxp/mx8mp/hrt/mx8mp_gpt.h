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
 * @file mx8mp_gpt.h
 *
 * i.MX 8M Plus GPT for the PX4 high-resolution timer. The GPT is the same IP block as
 * on the i.MX RT, so hrt.c is shared with imxrt; this header maps its names.
 */

#pragma once

#include <stdint.h>
#include <arch/irq.h>

#define IMXRT_GPT1_BASE       0x302D0000
#define IMXRT_GPT2_BASE       0x302E0000
#define IMXRT_GPT3_BASE       0x302F0000
#define IMXRT_IRQ_GPT1        MX8MP_IRQ_GPT1

#define IMXRT_GPT_CR_OFFSET   0x0000
#define IMXRT_GPT_PR_OFFSET   0x0004
#define IMXRT_GPT_SR_OFFSET   0x0008
#define IMXRT_GPT_IR_OFFSET   0x000c
#define IMXRT_GPT_OCR1_OFFSET 0x0010
#define IMXRT_GPT_OCR2_OFFSET 0x0014
#define IMXRT_GPT_OCR3_OFFSET 0x0018
#define IMXRT_GPT_ICR1_OFFSET 0x001c
#define IMXRT_GPT_ICR2_OFFSET 0x0020
#define IMXRT_GPT_CNT_OFFSET  0x0024

#define GPT_CR_EN             (1 << 0)
#define GPT_CR_ENMOD          (1 << 1)
#define GPT_CR_CLKSRC_SHIFT   (6)
#define GPT_CR_CLKSRC_MASK    (7 << GPT_CR_CLKSRC_SHIFT)
#define GPT_CR_CLKSRC_IPG     (1 << GPT_CR_CLKSRC_SHIFT)   /* peripheral clock = gpt1 root */
#define GPT_CR_FRR            (1 << 9)
#define GPT_CR_IM1_SHIFT      (16)
#define GPT_CR_IM1_BOTH       (3 << GPT_CR_IM1_SHIFT)
#define GPT_CR_IM2_SHIFT      (18)
#define GPT_CR_IM2_BOTH       (3 << GPT_CR_IM2_SHIFT)
#define GPT_CR_OM1_DIS        (0 << 20)
#define GPT_CR_OM2_DIS        (0 << 23)
#define GPT_CR_OM3_DIS        (0 << 26)
#define GPT_SR_OF1            (1 << 0)
#define GPT_SR_OF2            (1 << 1)
#define GPT_SR_OF3            (1 << 2)
#define GPT_SR_IF1            (1 << 3)
#define GPT_SR_IF2            (1 << 4)
#define GPT_IR_OF1IE          (1 << 0)
#define GPT_IR_OF2IE          (1 << 1)
#define GPT_IR_OF3IE          (1 << 2)
#define GPT_IR_IF1IE          (1 << 3)
#define GPT_IR_IF2IE          (1 << 4)

/* GPT1 clock: root "gpt1" (CCM+0xb580) = 24 MHz oscillator, gate CCM+0x4100 on for the
 * M7's clock domain (domain 1), so Linux's clock framework cannot switch it off. */
static inline void imxrt_clockall_gpt_bus(void)
{
	*(volatile uint32_t *)(0x30380000 + 0xb580) = 1u << 28;
	*(volatile uint32_t *)(0x30380000 + 0x4100 + 4) = 3u << 4;
}
