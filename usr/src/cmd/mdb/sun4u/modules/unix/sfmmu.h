/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
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
 * Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_SFMMU_H
#define	_SFMMU_H

#pragma ident	"@(#)sfmmu.h	1.3	05/06/08 SMI"

#ifdef	__cplusplus
extern "C" {
#endif

extern int sfmmu_vtop(uintptr_t addr, uint_t flags, int argc,
	const mdb_arg_t *argv);

extern int page_num2pp(uintptr_t addr, uint_t flags, int argc,
	const mdb_arg_t *argv);

extern int memseg_list(uintptr_t addr, uint_t flags, int argc,
	const mdb_arg_t *argv);

extern int memseg_walk_init(mdb_walk_state_t *);
extern int memseg_walk_step(mdb_walk_state_t *);
extern void memseg_walk_fini(mdb_walk_state_t *);

extern void tsbinfo_help(void);
extern int tsbinfo_list(uintptr_t, uint_t, int, const mdb_arg_t *);

#ifdef	__cplusplus
}
#endif

#endif	/* _SFMMU_H */
