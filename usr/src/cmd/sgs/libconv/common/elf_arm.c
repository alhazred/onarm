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
 * Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007 NEC Corporation
 */

#ident	"@(#)cmd/sgs/libconv/common/elf_arm.c"

/*
 * String conversion routine for ELF header.
 */
#include <stdio.h>
#include <sys/sysmacros.h>
#include <sys/elf_ARM.h>
#include "_conv.h"
#include "elf_arm_msg.h"

/*
 * ARM specific ELF header flags.
 */

#define	EFLAGSZ_ARM_EABI						\
	MAX(MSG_EF_ARM_EABI_VER5_SIZE,					\
	    MAX(MAX(MSG_EF_ARM_EABI_VER1_SIZE, MSG_EF_ARM_EABI_VER2_SIZE), \
		MAX(MSG_EF_ARM_EABI_VER3_SIZE, MSG_EF_ARM_EABI_VER4_SIZE)))

#define	EFLAGSZ								\
	(CONV_EXPN_FIELD_DEF_PREFIX_SIZE +				\
	 MSG_EF_ARM_RELEXEC_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_HASENTRY_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_INTERWORK_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_APCS_26_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_APCS_FLOAT_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_PIC_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +		\
	 MSG_EF_ARM_ALIGN8_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_NEW_ABI_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_OLD_ABI_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_SOFT_FLOAT_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_VFP_FLOAT_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +	\
	 MSG_EF_ARM_MAVERICK_FLOAT_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE + \
	 MSG_EF_ARM_BE8_SIZE + CONV_EXPN_FIELD_DEF_SEP_SIZE +		\
	 EFLAGSZ_ARM_EABI + CONV_EXPN_FIELD_DEF_SEP_SIZE +		\
	 CONV_INV_STRSIZE + CONV_EXPN_FIELD_DEF_SUFFIX_SIZE)

/*
 * const char *
 * conv_ehdr_ARM_flags(Word flags, Conv_fmt_flags_t fmt_flags,
 *		       Conv_ehdr_flags_buf_t *flags_buf)
 *	Return string representation of the specified ELF header flags.
 */
const char *
conv_ehdr_ARM_flags(Word flags, Conv_fmt_flags_t fmt_flags,
		    Conv_ehdr_flags_buf_t *flags_buf)
{
	static Val_desc vda[] = {
		{ EF_ARM_RELEXEC, MSG_ORIG(MSG_EF_ARM_RELEXEC) },
		{ EF_ARM_HASENTRY, MSG_ORIG(MSG_EF_ARM_HASENTRY) },
		{ EF_ARM_INTERWORK, MSG_ORIG(MSG_EF_ARM_INTERWORK) },
		{ EF_ARM_APCS_26, MSG_ORIG(MSG_EF_ARM_APCS_26) },
		{ EF_ARM_APCS_FLOAT, MSG_ORIG(MSG_EF_ARM_APCS_FLOAT) },
		{ EF_ARM_PIC, MSG_ORIG(MSG_EF_ARM_PIC) },
		{ EF_ARM_ALIGN8, MSG_ORIG(MSG_EF_ARM_ALIGN8) },
		{ EF_ARM_NEW_ABI, MSG_ORIG(MSG_EF_ARM_NEW_ABI) },
		{ EF_ARM_OLD_ABI, MSG_ORIG(MSG_EF_ARM_OLD_ABI) },
		{ EF_ARM_SOFT_FLOAT, MSG_ORIG(MSG_EF_ARM_SOFT_FLOAT) },
		{ EF_ARM_VFP_FLOAT, MSG_ORIG(MSG_EF_ARM_VFP_FLOAT) },
		{ EF_ARM_MAVERICK_FLOAT, MSG_ORIG(MSG_EF_ARM_MAVERICK_FLOAT) },
		{ EF_ARM_BE8, MSG_ORIG(MSG_EF_ARM_BE8) },
		{ 0, 0 }
	};
	static const char *leading_str_arr[2];
	static CONV_EXPN_FIELD_ARG conv_arg = {
		NULL, sizeof (flags_buf->buf), vda, leading_str_arr
	};
	const char **lstr = leading_str_arr;
	Word	eabiver = (flags & EF_ARM_EABIMASK);

	if (eabiver != 0) {
		switch (eabiver) {
		case EF_ARM_EABI_VER1:
			*lstr = MSG_ORIG(MSG_EF_ARM_EABI_VER1);
			break;

		case EF_ARM_EABI_VER2:
			*lstr = MSG_ORIG(MSG_EF_ARM_EABI_VER2);
			break;

		case EF_ARM_EABI_VER3:
			*lstr = MSG_ORIG(MSG_EF_ARM_EABI_VER3);
			break;

		case EF_ARM_EABI_VER4:
			*lstr = MSG_ORIG(MSG_EF_ARM_EABI_VER4);
			break;

		case EF_ARM_EABI_VER5:
			*lstr = MSG_ORIG(MSG_EF_ARM_EABI_VER5);
			break;

		default:
			*lstr = "EABI??";
			break;
		}
		flags &= ~EF_ARM_EABIMASK;
		lstr++;
	}
	*lstr = NULL;
	conv_arg.buf = flags_buf->buf;
	conv_arg.oflags = conv_arg.rflags = flags;
	(void)conv_expn_field(&conv_arg, fmt_flags);

	return conv_arg.buf;
}
