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
 * Copyright (c) 2008-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)ne1/boot/boot_memlist.c"

/*
 * SDRAM configuration for NE1.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/bootconf.h>
#include <sys/platform.h>

static struct memlist	ne1_sdram0;
static struct memlist	ne1_sdram1;

static struct memlist	ne1_sdram0 = {
	(uint64_t)ARMPF_SDRAM0_PADDR,	/* address */
	(uint64_t)ARMPF_SDRAM0_SIZE,	/* size */
	&ne1_sdram1,			/* next */
	NULL				/* prev */
};

/*
 * X Frame buffer and ramdisk is located at the end of SDRAM1.
 * We want to treat it as device, not SDRAM.
 */
#if	defined(UFS_RAM_ROOTFS) && defined(RAMDISK_ROOT_PADDR) && \
	defined(RAMDISK_ROOT_SIZE)
#define	SDRAM1_SIZE							\
	(ARMPF_SDRAM1_SIZE - ARMPF_XWINDOW_SIZE - RAMDISK_ROOT_SIZE)
#else	/* !UFS_RAM_ROOTFS || !RAMDISK_ROOT_PADDR || !RAMDISK_ROOT_SIZE */
#define	SDRAM1_SIZE	(ARMPF_SDRAM1_SIZE - ARMPF_XWINDOW_SIZE)
#endif	/* UFS_RAM_ROOTFS && RAMDISK_ROOT_PADDR && RAMDISK_ROOT_SIZE */

static struct memlist	ne1_sdram1 = {
	(uint64_t)ARMPF_SDRAM1_PADDR,			/* address */
	(uint64_t)SDRAM1_SIZE,				/* size */
	NULL,						/* next */
	&ne1_sdram0					/* prev */
};

static struct bsys_mem	ne1_bsys_mem = {
	&ne1_sdram0,		/* physinstalled */
	&ne1_sdram0,		/* physavail */
	NULL,			/* virtavail (not used) */
	NULL,			/* pcimem (not used) */
	0			/* extend (not used) */
};

struct memlist	*armpf_boot_memlist = &ne1_sdram0;

/*
 * void
 * boot_memlist_init(bootops_t *bops)
 *	Initialize memory configuration in bootops.
 */
void
boot_memlist_init(bootops_t *bops)
{
	bops->boot_mem = &ne1_bsys_mem;
}
