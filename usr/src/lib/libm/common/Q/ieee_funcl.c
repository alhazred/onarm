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

#pragma ident	"@(#)ieee_funcl.c	1.11	06/01/31 SMI"

#if defined(ELFOBJ)
#pragma weak isinfl = __isinfl
#pragma weak isnormall = __isnormall
#pragma weak issubnormall = __issubnormall
#pragma weak iszerol = __iszerol
#pragma weak signbitl = __signbitl
#endif

#include "libm.h"

#if defined(__sparc)
int
isinfl(long double x) {
	int *px = (int *) &x;
	return ((px[0] & ~0x80000000) == 0x7fff0000 && px[1] == 0 &&
		px[2] == 0 && px[3] == 0);
}

int
isnormall(long double x) {
	int *px = (int *) &x;
	return ((unsigned) ((px[0] & 0x7fff0000) - 0x10000) < 0x7ffe0000);
}

int
issubnormall(long double x) {
	int *px = (int *) &x;
	px[0] &= ~0x80000000;
	return (px[0] < 0x00010000 && (px[0] | px[1] | px[2] | px[3]) != 0);
}

int
iszerol(long double x) {
	int *px = (int *) &x;
	return (((px[0] & ~0x80000000) | px[1] | px[2] | px[3]) == 0);
}

int
signbitl(long double x) {
	unsigned *px = (unsigned *) &x;
	return (px[0] >> 31);
}
#elif defined(__i386)
int
isinfl(long double x) {
	int *px = (int *) &x;
#if defined(HANDLE_UNSUPPORTED)
	return ((px[2] & 0x7fff) == 0x7fff &&
		((px[1] ^ 0x80000000) | px[0]) == 0);
#else
	return ((px[2] & 0x7fff) == 0x7fff &&
		((px[1] & ~0x80000000) | px[0]) == 0);
#endif
}

int
isnormall(long double x) {
	int *px = (int *) &x;
#if defined(HANDLE_UNSUPPORTED)
	return ((unsigned) ((px[2] & 0x7fff) - 1) < 0x7ffe &&
		(px[1] & 0x80000000) != 0);
#else
	return ((unsigned) ((px[2] & 0x7fff) - 1) < 0x7ffe);
#endif
}

int
issubnormall(long double x) {
	int *px = (int *) &x;
	return ((px[2] & 0x7fff) == 0 && (px[0] | px[1]) != 0);
}

int
iszerol(long double x) {
	int *px = (int *) &x;
	return (((px[2] & 0x7fff) | px[0] | px[1]) == 0);
}

int
signbitl(long double x) {
	unsigned *px = (unsigned *) &x;
	return ((px[2] >> 15) & 1);
}
#endif	/* defined(__sparc) || defined(__i386) */
