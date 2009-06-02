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
 * Copyright (c) 2007 NEC Corporation
 * All rights reserved.
 */

	.ident	"@(#)strcmp_asm.s"

#include <sys/asm_linkage.h>

/*
 * int
 * strcmp(const char *s1, const char *s2)
 * {
 *	if (s1 == s2)
 *		return (0);
 *	while (*s1 == *s2++)
 *		if (*s1++ == '\0')
 *			return (0);
 *	return (*(unsigned char *)s1 - *(unsigned char *)--s2);
 * }
 */
	
	ENTRY(strcmp)
	mov	ip, r0		/* %ip = s1	*/
	subs	r0, ip, r1
	beq	.L_end

1:
	ldrb	r2, [ip], #1	/* r2 = *s1++;	*/
	ldrb	r3, [r1], #1	/* r3 = *s2++;	*/
	subs	r0, r2, r3
	bne	.L_end
	cmp	r2, #0
	bne	1b

.L_end:
	bx	lr
	SET_SIZE(strcmp)
