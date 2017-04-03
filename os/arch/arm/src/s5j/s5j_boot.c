/****************************************************************************
 *
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the License.
 *
 ****************************************************************************/
/****************************************************************************
 * arch/arm/src/s5j/s5j_boot.c
 *
 *   Copyright (C) 2009-2010, 2014-2015 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
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
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <tinyara/config.h>

#include <stdint.h>
#include <assert.h>
#include <debug.h>

#include <tinyara/init.h>

#include "up_arch.h"
#include "up_internal.h"

#include <chip.h>
#include "s5j_watchdog.h"
#include "arm.h"
#ifdef CONFIG_ARMV7M_MPU
#include "mpu.h"
#endif
#include "cache.h"
#include "fpu.h"

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/****************************************************************************
 * Private Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/
extern uint32_t _vector_start;
extern uint32_t _vector_end;

/****************************************************************************
 * Public Functions
 ****************************************************************************/
void up_copyvectorblock(void)
{
	uint32_t *src = (uint32_t *)&_vector_start;
	uint32_t *end = (uint32_t *)&_vector_end;
	uint32_t *dest = (uint32_t *) VECTOR_BASE;

	while (src < end) {
		*dest++ = *src++;
	}
}

#ifdef CONFIG_ARMV7M_MPU
int s5j_mpu_initialize(void)
{
#ifdef CONFIG_ARCH_CHIP_S5JT200
	/*
	 * Vector Table	0x02020000	0x02020FFF	4
	 * Reserved	0x02021000	0x020217FF	2
	 * BL1		0x02021800	0x020237FF	8
	 * TinyARA	0x02023800	0x020E7FFF	786(WBWA)
	 * Reserved	0x020E8000	0x020FFFFF	96 (WBWA)
	 * Reserved	0x02100000	0x021FFFFF	64 (NCNB)
	 * WIFI		0x02110000	0x0215FFFF	320(NCNB)
	 */

	/* Region 0, Set read only for memory area */
	mpu_priv_flash(0x0, 0x80000000);

	/* Region 1, for ISRAM(0x0200_0000++2048KB, RW-WBWA */
	mpu_user_intsram_wb(0x02000000, 0x200000);

	/* Region 2, wifi driver needs non-$(0x0211_0000++320KB, RW-NCNB */
	mpu_priv_noncache(0x02100000, 0x80000);

	/* Region 3, for FLASH area, default to set WBWA */
	mpu_user_intsram_wb(CONFIG_S5J_FLASH_BASE, CONFIG_S5J_FLASH_SIZE);

	/* region 4, for Sflash Mirror area to be read only */
	mpu_priv_flash(CONFIG_S5J_FLASH_MIRROR_BASE, CONFIG_S5J_FLASH_SIZE);

	/* Region 5, for SFR area read/write, strongly-ordered */
	mpu_priv_stronglyordered(0x80000000, 0x10000000);

	/*
	 * Region 6, for vector table,
	 * s5j uses high vector in 0xFFFF_0000++4KB, read only
	 */
	mpu_priv_flash(0xFFFF0000, 0x1000);

	mpu_control(true);
#endif
	return 0;
}
#endif

void arm_boot(void)
{
#ifdef CONFIG_S5J_DEBUG_BREAK
	__asm__ __volatile__("b  .");
#endif

	up_copyvectorblock();

	/* Disable the watchdog timer */
	s5j_watchdog_disable();

#ifdef CONFIG_ARMV7R_MEMINIT
	/* Initialize the .bss and .data sections as well as RAM functions
	 * now after RAM has been initialized.
	 *
	 * NOTE that if SDRAM were supported, this call might have to be
	 * performed after returning from tms570_board_initialize()
	 */
	arm_data_initialize();
#endif

#ifdef CONFIG_ARMV7M_MPU
	s5j_mpu_initialize();
	arch_enable_icache();
	arch_enable_dcache();
#endif

	cal_init();

#ifdef USE_EARLYSERIALINIT
	up_earlyserialinit();
#endif

	/*
	 * Perform board-specific initialization. This must include:
	 *
	 * - Initialization of board-specific memory resources (e.g., SDRAM)
	 * - Configuration of board specific resources (GPIOs, LEDs, etc).
	 *
	 * NOTE: we must use caution prior to this point to make sure that
	 * the logic does not access any global variables that might lie
	 * in SDRAM.
	 */
	s5j_board_initialize();

	os_start();
}
