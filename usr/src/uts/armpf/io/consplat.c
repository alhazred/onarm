/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2006-2008 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)armpf/io/consplat.c"

/*
 * ARMPF specific console configuration routines
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/platform.h>

int
plat_use_polled_debug()
{
	return 0;
}

int
plat_support_serial_kbd_and_ms()
{
	return 0;
}

int
plat_stdin_is_keyboard(void)
{
	return 0;
}

int
plat_stdout_is_framebuffer(void)
{
	return 0;
}

/*
 * Return generic path to keyboard device from the alias.
 */
char *
plat_kbdpath(void)
{
	return NULL;
}

/*
 * Return generic path to display device from the alias.
 */
char *
plat_fbpath(void)
{
	return NULL;
}

char *
plat_mousepath(void)
{
	return NULL;
}

/*
 * ARMPF_CONSOLE_DDIPATH is defined in sys/platform.h.
 */
char *
plat_stdinpath(void)
{
	return ARMPF_CONSOLE_DDIPATH;
}

char *
plat_stdoutpath(void)
{
	return ARMPF_CONSOLE_DDIPATH;
}

/*
 * If VIS_PIXEL mode will be implemented on ARM, these following
 * functions should be re-considered. Now these functions are
 * unused on ARM.
 */
void
plat_tem_get_inverses(int *inverse, int *inverse_screen)
{
	*inverse = 0;
	*inverse_screen = 0;
}

void
plat_tem_get_prom_font_size(int *charheight, int *windowtop)
{
	*charheight = 0;
	*windowtop = 0;
}

void
plat_tem_get_prom_size(size_t *height, size_t *width)
{
	*height = 25;
	*width = 80;
}

void
plat_tem_hide_prom_cursor(void)
{
}

void
plat_tem_get_prom_pos(uint32_t *row, uint32_t *col)
{
	*row = 0;
	*col = 0;
}
