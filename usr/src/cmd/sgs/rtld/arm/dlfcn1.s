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
 * Copyright (c) 2007-2008 NEC Corporation
 * All rights reserved.
 */

	.ident	"@(#)dlfcn1.s"

	.file	"dlfcn1.s"

/*
 * dlXXX functions entry point routines.
 *
 * These functions store the orginal caller address and pass as
 * a parmeter to the original dlXXX functions.
 * The oroginal dlXXX functions were redefined to `dlXXXf' with
 * one new argument (caddr_t pc) which is the dlXXX caller's pc.
 */

#include <sys/asm_linkage.h>

/*
 * dlopen and _dlopen entry
 */
	ANSI_PRAGMA_WEAK(dlopen, function)

	ENTRY(_dlopen)
	mov	r2, lr
	b	_dlopenf
	SET_SIZE(_dlopen)

/*
 * dlerror and _dlerror entry
 */
	ANSI_PRAGMA_WEAK(dlerror, function)

	ENTRY(_dlerror)
	mov	r0, lr
	b	_dlerrorf
	SET_SIZE(_dlerror)

/*
 * dlclose and _dlclose entry
 */
	ANSI_PRAGMA_WEAK(dlclose, function)

	ENTRY(_dlclose)
	mov	r1, lr
	b	_dlclosef
	SET_SIZE(_dlclose)

/*
 * dlmopen and _dlmopen entry
 */
	ANSI_PRAGMA_WEAK(dlmopen, function)

	ENTRY(_dlmopen)
	mov	r3, lr
	b	_dlmopenf
	SET_SIZE(_dlmopen)

/*
 * dlsym and _dlsym entry
 */
	ANSI_PRAGMA_WEAK(dlsym, function)

	ENTRY(_dlsym)
	mov	r2, lr
	b	_dlsymf
	SET_SIZE(_dlsym)

/*
 * dldump and _dldump entry
 */
	ANSI_PRAGMA_WEAK(dldump, function)

	ENTRY(_dldump)
	mov	r3, lr
	b	_dldumpf
	SET_SIZE(_dldump)

/*
 * dlinfo and _dlinfo entry
 */
	ANSI_PRAGMA_WEAK(dlinfo, function)

	ENTRY(_dlinfo)
	mov	r3, lr
	b	_dlinfof
	SET_SIZE(_dlinfo)

/*
 * _ld_libc entry
 */
	ENTRY(_ld_libc)
	mov	r1, lr
	b	_ld_libcf
	SET_SIZE(_ld_libc)
