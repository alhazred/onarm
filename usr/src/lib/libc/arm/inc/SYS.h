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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2007-2008 NEC Corporation
 */

#ifndef	_LIBC_ARM_INC_SYS_H
#define	_LIBC_ARM_INC_SYS_H

#pragma ident	"@(#)SYS.h	1.8	05/06/08 SMI"

#include <sys/asm_linkage.h>
#include <sys/syscall.h>
#include <sys/trap.h>
#include <sys/errno.h>
#include "synonyms.h"

#define	_prologue_
#define	_epilogue_

#define	_fref_(name)	name(PLT)


/*
 * Define the external symbols __cerror and __cerror64 for all files.
 */
	.globl	__cerror
	.globl	__cerror64

/*
 * __SYSCALLINT provides the basic trap sequence.  It assumes that an entry
 * of the form SYS_name exists (probably from sys/syscall.h).
 */

#define	__SYSCALLINT(name)	\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;

#define	SYSTRAP_RVAL1(name)	\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;

#define	SYSTRAP_RVAL2(name)	\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;

#define	SYSTRAP_2RVALS(name)	\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;

#define	SYSTRAP_64RVAL(name)	\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;

/*
 * SYSFASTTRAP provides the fast system call trap sequence.  It assumes
 * that an entry of the form T_name exists (probably from sys/trap.h).
 */
#define	SYSFASTTRAP(name)	\
	mov	ip, $T_##name;	\
	orr	ip, ip, #T_SWI_FASTCALL;	\
	swi	#(T_##name|T_SWI_FASTCALL);

/*
 * SYSCERROR provides the sequence to branch to __cerror if an error is
 * indicated by the carry-bit being set upon return from a trap.
 */
#define	SYSCERROR		\
	movcs	r0, ip;		\
	bcs	_fref_(__cerror)

/*
 * SYSCERROR64 provides the sequence to branch to __cerror64 if an error is
 * indicated by the carry-bit being set upon return from a trap.
 */
#define	SYSCERROR64		\
	movcs	r0, ip;		\
	bcs	_fref_(__cerror64)

/*
 * SYSLWPERR provides the sequence to return 0 on a successful trap
 * and the error code if unsuccessful.
 * Error is indicated by the carry-bit being set upon return from a trap.
 */
#define	SYSLWPERR		\
	bcc	1f;		\
	mov	r0, ip;		\
	cmp	r0, $ERESTART;	\
	bne	2f;		\
	mov	r0, $EINTR;	\
	b	2f;		\
1:				\
	mov	r0, #0;		\
2:

/*
 * SYSREENTRY provides the entry sequence for restartable system calls.
 */
#define	SYSREENTRY(name)	\
/* CSTYLED */			\
.restart_##name:		\
	ENTRY(name)


/*
 * SYSRESTART provides the error handling sequence for restartable
 * system calls.
 */
#define	SYSRESTART(name)		\
	bcc	1f;			\
	cmp	ip, $ERESTART;		\
	beq	name;			\
	mov	r0, ip;			\
	b	_fref_(__cerror);	\
1:


/*
 * SYSINTR_RESTART provides the error handling sequence for restartable
 * system calls in case of EINTR or ERESTART.
*/
#define	SYSINTR_RESTART(name)	\
	bcc	1f;		\
	cmp	ip, $ERESTART;	\
	beq	name;		\
	cmp	ip, $EINTR;	\
	beq	name;		\
	mov	r0, ip;		\
	b	2f;		\
1:				\
	mov	r0, #0;		\
2:

/*
 * SYSCALL provides the standard (i.e.: most common) system call sequence.
 */
#define	SYSCALL(name)		\
	ENTRY(name);		\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;	\
	SYSCERROR

#define	SYSCALL_RVAL1(name)	\
	ENTRY(name);		\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;	\
	SYSCERROR

/*
 * SYSCALL64 provides the standard (i.e.: most common) system call sequence
 * for system calls that return 64-bit values.
 */
#define	SYSCALL64(name)		\
	ENTRY(name);		\
	mov	ip, $SYS_##name;\
	swi	$SYS_##name;	\
	SYSCERROR64

/*
 * SYSCALL_RESTART provides the most common restartable system call sequence.
 */
#define	SYSCALL_RESTART(name)	\
.restart_##name:		\
	ENTRY(name);		\
	SYSTRAP_2RVALS(name);	\
	SYSRESTART(.restart_##name)

#define	SYSCALL_RESTART_RVAL1(name)	\
.restart_##name:			\
	ENTRY(name);			\
	mov	ip, $SYS_##name;	\
	swi	$SYS_##name;		\
	SYSRESTART(.restart_##name)

/*
 * SYSCALL2 provides a common system call sequence when the entry name
 * is different than the trap name.
 */
#define	SYSCALL2(entryname, trapname)	\
	ENTRY(entryname);		\
	SYSTRAP_2RVALS(trapname);	\
	SYSCERROR

#define	SYSCALL2_RVAL1(entryname, trapname)	\
	ENTRY(entryname);			\
	mov	ip, $SYS_##trapname;		\
	swi	$SYS_##trapname;		\
	SYSCERROR

/*
 * SYSCALL2_RESTART provides a common restartable system call sequence when the
 * entry name is different than the trap name.
 */
#define	SYSCALL2_RESTART(entryname, trapname)	\
.restart_##entryname:				\
	ENTRY(entryname);			\
	SYSTRAP_2RVALS(trapname);		\
	/* CSTYLED */				\
	SYSRESTART(.restart_##entryname)

#define	SYSCALL2_RESTART_RVAL1(entryname, trapname)	\
.restart_##entryname:					\
	ENTRY(entryname);				\
	mov	ip, $SYS_##trapname;			\
	swi	$SYS_##trapname;			\
	SYSRESTART(.restart_##entryname)

/*
 * SYSCALL_NOERROR provides the most common system call sequence for those
 * system calls which don't check the error return (carry bit).
 */
#define	SYSCALL_NOERROR(name)	\
	ENTRY(name);		\
	SYSTRAP_2RVALS(name)

#define	SYSCALL_NOERROR_RVAL1(name)	\
	ENTRY(name);			\
	mov	ip, $SYS_##name;	\
	swi	$SYS_##name;

/*
 * Standard syscall return sequence, return code equal to rval1.
 */
#define	RET			\
	mov	pc, lr

/*
 * Syscall return sequence, return code equal to rval2.
 */
#define	RET2			\
	mov	r0, r1;		\
	mov	pc, lr

/*
 * Syscall return sequence with return code forced to zero.
 */
#define	RETC			\
	mov	r0, #0;		\
	mov	pc, lr

#endif	/* _LIBC_ARM_INC_SYS_H */
