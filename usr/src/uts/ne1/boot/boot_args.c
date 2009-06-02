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
 * Copyright (c) 2006-2009 NEC Corporation
 * All rights reserved.
 */

#ident	"@(#)ne1/boot/boot_args.c"

/*
 * NE1-specific default boot arguments.
 */

#include <sys/param.h>
#include <sys/boot_impl.h>
#include <sys/modctl.h>

#ifdef	ne1

/*
 * Determine default "fstype" and "bootpath".
 * Default is no default value.
 */
#define	BOP_DEFAULT_BOOTPATH

#ifdef	UFS_RAM_ROOTFS
#undef	BOP_DEFAULT_BOOTPATH
#define	BOP_DEFAULT_BOOTPATH			\
	{ "fstype",		"ufs" },	\
	{ "bootpath",		"/ramdisk:a" },
#endif	/* UFS_RAM_ROOTFS */

#ifdef	XRAMFS_ROOTFS
#undef	BOP_DEFAULT_BOOTPATH
#define	BOP_DEFAULT_BOOTPATH				\
	{ "fstype",		"xramfs" },		\
	{ "bootpath",		"/xramdev@0,0:root" },
#endif	/* XRAMFS_ROOTFS */


bootarg_t	default_bootargs[] = {
	{ "mfg-name",		"ne1" },
	{ "impl-arch-name",	"ne1" },
	BOP_DEFAULT_BOOTPATH
	{ NULL,			NULL }
};

/* NE1-specific module search path. */
#define	ROOT_MOD_DIR		"/kernel"
#define	USR_MOD_DIR		"/usr/kernel"
#define	ROOT_PSM_MOD_DIR	"/platform/ne1/kernel"
#define	USR_PSM_MOD_DIR		"/usr/platform/ne1/kernel"

#ifdef	KMODS_INST_USR
char	plat_mod_defpath[] = USR_PSM_MOD_DIR " " USR_MOD_DIR;
#else	/* !KMODS_INST_USR */
char	plat_mod_defpath[] =
	ROOT_PSM_MOD_DIR " " USR_PSM_MOD_DIR " " ROOT_MOD_DIR " " USR_MOD_DIR;
#endif	/* KMODS_INST_USR */

#else	/* !ne1 */
#error port me
#endif	/* ne1 */
