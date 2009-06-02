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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef	_SYS_CONTRACT_KERNEL_H
#define	_SYS_CONTRACT_KERNEL_H

#pragma ident	"@(#)contract_kernel.h"

/*
 * contract_kernel.h: Kernel build tree private definitions for brand.
 */
#ifndef _SYS_CONTRACT_IMPL_H
#error  Do NOT include contract_kernel.h directly.
#endif  /* !_SYS_CONTRACT_IMPL_H */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/debug.h>
#include <sys/contract.h>
#include <sys/avl.h>
#include <sys/sysmacros.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	CONTRACT_DISABLE

#undef	CT_DEBUG
#ifdef	CONTRACT_DEBUG
#define	CT_DEBUG(args)	cmn_err args
#else
#define	CT_DEBUG(args)
#endif

/*
 * Contract template interfaces
 */
#define	ctmpl_free(template)		ASSERT(0)
#define	ctmpl_set(template, param, cr)	(ASSERT(0), EINVAL)
#define	ctmpl_get(template, param)	(ASSERT(0), EINVAL)
#define	ctmpl_dup(template)		(ASSERT(0), (ct_template_t *)NULL)

#define	contract_compar			((int (*)())0xdeadbeef)

/*
 * Contract functions
 */
#define	contract_init()							\
	avl_create(&p0.p_ct_held, contract_compar, sizeof(contract_t),	\
		offsetof(contract_t, ct_ctlist));
#define	contract_abandon(ct, p, explicit)	(0)
#define	contract_exit(p)

/*
 * Contract implementation interfaces
 */
#define	contract_rele(ct)
#define	contract_getzuniqid(ct)			(ASSERT(0), 0)
#define	contract_plookup(p, current, zuniqid)	(-1)
#define	contract_ptr(id, zuniqid)		((contract_t *)NULL)
#define	contract_owned(ct, cr, locked)		(ASSERT(0), 1)

/*
 * Type interfaces
 */
#define	ct_ntypes				(0)
#define	ct_types				((ct_type_t **)NULL)
#define	contract_type_ptr(type, id, zuniqid)	((contract_t *)NULL)

#endif	/* CONTRACT_DISABLE */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_CONTRACT_KERNEL_H */
