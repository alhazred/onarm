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
 * Copyright (c) 2006 NEC Corporation
 * All rights reserved.
 */

	.ident	"@(#)byteorder.s"

	.file	"byteorder.s"

#include <sys/asm_linkage.h>

	ENTRY(htonl)
	ENTRY(ntohl)
	rev	r0, r0
	bx	lr
	SET_SIZE(htonl)
	SET_SIZE(ntohl)

	ENTRY(htons)
	ENTRY(ntohs)
	rev	r1, r0
	mov	r0, r1 , LSR #16
	bx	lr
	SET_SIZE(htons)
	SET_SIZE(ntohs)
