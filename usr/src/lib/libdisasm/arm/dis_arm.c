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
 * Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#pragma ident	"@(#)dis_arm.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/byteorder.h>
#include <libdisasm.h>

#include "dis_string.h"
#include "libdisasm_impl.h"

struct dis_handle {
	void		*dh_data;
	int		dh_flags;
	dis_lookup_f	dh_lookup;
	dis_read_f	dh_read;
	int		dh_disflags;
	uint64_t	dh_addr;
	uint64_t	dh_end;
};

dis_handle_t *
dis_handle_create(int flags, void *data, dis_lookup_f lookup_func,
		  dis_read_f read_func)
{
	dis_handle_t *dhp;

	/*
	 * Validate architecture flags
	 */
	if (flags & ~(DIS_OCTAL|DIS_NOIMMSYM|DIS_ARM_VFP2)) {
		(void)dis_seterrno(E_DIS_INVALFLAG);
		return (NULL);
	}

	/*
	 * Create and initialize the internal structure
	 */
	if ((dhp = dis_zalloc(sizeof (struct dis_handle))) == NULL) {
		(void)dis_seterrno(E_DIS_NOMEM);
		return (NULL);
	}

	dhp->dh_lookup = lookup_func;
	dhp->dh_read = read_func;
	dhp->dh_flags = flags;
	dhp->dh_data = data;
	dhp->dh_disflags = 0;

	if (flags & DIS_ARM_VFP2) {
		dhp->dh_disflags |= DIS_ARM_STR_VFP2;
	}
	if (flags & DIS_OCTAL) {
		dhp->dh_disflags |= DIS_ARM_STR_OCTAL;
	}

	return (dhp);
}

int
dis_disassemble(dis_handle_t *dhp, uint64_t addr, char *buf, size_t buflen)
{
	uint32_t	inst;
	uint32_t	target;

	if (dhp->dh_read(dhp->dh_data, addr, &inst, sizeof (inst)) !=
	    sizeof (inst)) {
		return (-1);
	}

	/* This allows ARM code to be tested on sparc. */
	inst = LE_32(inst);

	if (dis_arm_string(ARM_CPU_MPCORE, addr, inst, buf, buflen, &target,
			   dhp->dh_disflags) != 0) {
		(void)snprintf(buf, buflen, "*** invalid opcode ***");
		return (0);
	}

	if (target != (uint32_t)-1) {
		size_t	len;

		(void)strlcat(buf, " <", buflen);
		len = strlen(buf);
		dhp->dh_lookup(dhp->dh_data, target, buf + len,
			       buflen - len - 1, NULL, NULL);
		(void)strlcat(buf, ">", buflen);
	}

	return (0);
}

void
dis_handle_destroy(dis_handle_t *dhp)
{
	dis_free(dhp, sizeof (dis_handle_t));
}

void
dis_set_data(dis_handle_t *dhp, void *data)
{
	dhp->dh_data = data;
}

void
dis_flags_set(dis_handle_t *dhp, int f)
{
	dhp->dh_flags |= f;
}

void
dis_flags_clear(dis_handle_t *dhp, int f)
{
	dhp->dh_flags &= ~f;
}


/* ARGSUSED */
int
dis_max_instrlen(dis_handle_t *dhp)
{
	return (4);
}

/*
 * The dis_i386.c comment for this says it returns the previous instruction,
 * however, I'm fairly sure it's actually returning the _address_ of the
 * nth previous instruction.
 */
/* ARGSUSED */
uint64_t
dis_previnstr(dis_handle_t *dhp, uint64_t pc, int n)
{
	if (n <= 0) {
		return (pc);
	}

	if (pc < n) {
		return (pc);
	}

	return (pc - (n * 4));
}
