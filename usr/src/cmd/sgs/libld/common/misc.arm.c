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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#include <stdio.h>
#include <string.h>
#include <sys/elf_ARM.h>
#include <debug.h>
#include <dwarf.h>
#include "msg.h"
#include "_libld.h"

/*
 * Miscellaneous ARM support utilities.
 */

/*
 * Determine whether EABI versions are compatible.
 * We know that EABI version 4 and 5 are compatible.
 */
#define	EABI_VERSION_IS_COMPAT(v1, v2)					\
	((v1) == (v2) ||						\
	 (((v1) == EF_ARM_EABI_VER4 || (v1) == EF_ARM_EABI_VER5) &&	\
	  ((v2) == EF_ARM_EABI_VER4 || (v2) == EF_ARM_EABI_VER5)))

/* Supported ARM EABI versions */
typedef enum {
	ARM_EABI_OABI		= 0,	/* OABI */
	ARM_EABI_VER4		= 4,	/* EF_ARM_EABI_VER4 */
	ARM_EABI_VER5		= 5	/* EF_ARM_EABI_VER5 */
} arm_eabiver_t;

typedef struct arm_eabiver_arg {
	const char		*ae_name;	/* symbolic name */
	const arm_eabiver_t	ae_value;	/* corresponding value */
} arm_eabiver_arg_t;

/* Supported arguments for "-zeabi=" */
static const arm_eabiver_arg_t	eabiver_args[] = {
	{ "gnu", ARM_EABI_OABI },
	{ "4", ARM_EABI_VER4 },
	{ "5", ARM_EABI_VER5 }
};

/* Default EABI mode is EABI version 4. */
static arm_eabiver_t	cur_eabiver = ARM_EABI_VER4;

/*
 * uintptr_t
 * ld_parse_eabi(char *arg, Ofl_desc *ofl)
 *	Parse "-zeabi=" option.
 *	1 is returned on success, S_ERROR on failure.
 *
 * Remarks:
 *	EABI mode given by "-zeabi=" option is used only if no input ELF
 *	object is specified.
 */
uintptr_t
ld_parse_eabi(char *arg, Ofl_desc *ofl)
{
	int	i;
	const arm_eabiver_arg_t	*ap;

	for (i = 0, ap = eabiver_args;
	     i < sizeof(eabiver_args) / sizeof(eabiver_args[0]); i++, ap++) {
		if (strcmp(arg, ap->ae_name) == 0) {
			cur_eabiver = ap->ae_value;
			return (1);
		}
	}

	eprintf(ofl->ofl_lml, ERR_FATAL, MSG_INTL(MSG_ARG_ILLEGAL),
		MSG_ORIG(MSG_ARG_EABI), arg);
	ofl->ofl_flags |= FLG_OF_FATAL;

	return (S_ERROR);
}

/*
 * void
 * ld_mach_ofl_init(Ofl_desc *ofl)
 *	ARM specific constructor of output file instance.
 */
void
ld_mach_ofl_init(Ofl_desc *ofl)
{
	Word	eflags = 0;

	/* Set EABI version flag. */
	if (cur_eabiver == ARM_EABI_VER4) {
		eflags |= EF_ARM_EABI_VER4;
	}
	else if (cur_eabiver == ARM_EABI_VER5) {
		eflags |= EF_ARM_EABI_VER5;
	}

	ofl->ofl_dehdr->e_flags = eflags;
}

/*
 * int
 * ld_mach_ifl_verify(Ehdr *ehdr, Ofl_desc *ofl, Rej_desc *rej)
 *	ARM specific version of ifl_verify().
 */
int
ld_mach_ifl_verify(Ehdr *ehdr, Ofl_desc * ofl, Rej_desc *rej)
{
	Word	iflags = ehdr->e_flags;
	Word	oflags = ofl->ofl_dehdr->e_flags;
	Word	iver, over;

	/*
	 * Simply copy EABI version flags if the given file is the first
	 * input file. Default EABI version specified by "-zeabi=" option
	 * is ignored.
	 * Floating point architecture flags are also copied if the given
	 * file is relocatable object.
	 */
	if (ofl->ofl_soscnt == 0 && ofl->ofl_objscnt == 0) {
		oflags &= ~EF_ARM_EABIMASK;
		oflags |= (iflags & EF_ARM_EABIMASK);
		if (ehdr->e_type != ET_DYN) {
			oflags |= (iflags & M_EF_FPFLAG_MASK);
		}
		ofl->ofl_dehdr->e_flags = oflags;

		return (1);
	}

	/* Check EABI version. */
	iver = M_EF_ARM_EABI_VERSION(iflags);
	over = M_EF_ARM_EABI_VERSION(oflags);
	if (!EABI_VERSION_IS_COMPAT(iver, over)) {
		goto fail;
	}

	/*
	 * Check floating point architecture flags if the given file is
	 * OABI relocatable object.
	 */
	if (iver == 0 && ehdr->e_type != ET_DYN &&
	    (iflags & M_EF_FPFLAG_MASK) != (oflags & M_EF_FPFLAG_MASK)) {
		goto fail;
	}

	return (1);

fail:
	rej->rej_type = SGS_REJ_MISFLAG;
	rej->rej_info = (uint_t)iflags;
	return (0);
}

/*
 * uintptr_t
 * ld_mach_update_oehdr(Ofl_desc *ofl)
 *	ARM specific version of update_oehdr().
 */
uintptr_t
ld_mach_update_oehdr(Ofl_desc *ofl)
{
	Ehdr	*ehdr = ofl->ofl_nehdr;

	if (ehdr->e_entry != NULL) {
		ehdr->e_flags |= EF_ARM_HASENTRY;
	}

	return (1);
}
