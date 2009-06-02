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
 * Copyright (c) 2006-2007 NEC Corporation
 * All rights reserved.
 */

#if defined(__ARM_EABI__)
#pragma weak __aeabi_idiv = __divsi3
#pragma weak __aeabi_uidiv = __udivsi3
#pragma weak __aeabi_idivmod = __divremsi3
#endif

/* zero divide  error output */
#define       ZERO_DIV \
     __asm__("bkpt  #0x12")

extern unsigned long long __udivrem(unsigned int, unsigned int);

typedef union u_div {
	unsigned long long	rtbuf;
	struct st {
	unsigned int	u_d;
	unsigned int	u_r;
	} u_res;
} u_div_t;

/*
 * signed divide and mod return routine
 */
long long
__divremsi3(int p_divi, int p_divs)
{
	u_div_t	w_div;
	unsigned int	w_divi, w_divs;
	int	wflag_d;
	int	wflag_r;

	if (p_divs == 0) { 
		ZERO_DIV;
		w_div.u_res.u_d = 0;
		w_div.u_res.u_r = p_divi;
		return(w_div.rtbuf);
	}
	wflag_r = wflag_d = 0;
	if (p_divi < 0) {
		wflag_d++;
		wflag_r++;
		w_divi = (unsigned int)(-(p_divi));
	} else {
		w_divi = p_divi;
	}
	if (p_divs < 0) {
		wflag_d++;
		w_divs = (unsigned int)(-(p_divs));
	} else {
		w_divs = p_divs;
	}
	w_div.rtbuf = __udivrem(w_divi, w_divs);
	if (wflag_d == 1) {
		w_div.u_res.u_d = (unsigned int)((-((int)(w_div.u_res.u_d))));
	}
	if (wflag_r == 1) {
		w_div.u_res.u_r = (unsigned int)((-((int)(w_div.u_res.u_r))));
	}
	return(w_div.rtbuf);
}

/*
 * signed divide
 */
int
__divsi3(int p_divi, int p_divs)
{
	u_div_t	w_div;
	unsigned int	w_divi, w_divs;
	int	wflag;

	if (p_divs == 0) { 
		ZERO_DIV;
		return (0);
	}

	wflag = 0;
	if (p_divi < 0) {
		wflag++;
		w_divi = (unsigned int)(-(p_divi));
	} else {
		w_divi = p_divi;
	}
	if (p_divs < 0) {
		wflag++;
		w_divs = (unsigned int)(-(p_divs));
	} else {
		w_divs = p_divs;
	}
	w_div.rtbuf = __udivrem(w_divi, w_divs);
	if (wflag == 1) {
		return (-((int)(w_div.u_res.u_d)));
	} else {
		return ((int)(w_div.u_res.u_d));
	}
}

/*
 * signed remainder
 */
int
__modsi3(int p_divi, int p_divs)
{
	u_div_t	w_div;
	unsigned int	w_divi, w_divs;
	int	wflag;

	if (p_divs == 0) {
		ZERO_DIV;
		return (0);
	}

	wflag = 0;
	if (p_divi < 0) {
		wflag++;
		w_divi = (unsigned int)(-(p_divi));
	} else {
		w_divi = p_divi;
	}
	if (p_divs < 0) {
		w_divs = (unsigned int)(-(p_divs));
	} else {
		w_divs = p_divs;
	}

	w_div.rtbuf = __udivrem(w_divi, w_divs);
	if (wflag == 1) {
		return (-((int)(w_div.u_res.u_r)));
	} else {
		return ((int)(w_div.u_res.u_r));
	}
}

/*
 * unsigned divide
 */
unsigned int
__udivsi3(unsigned int p_divi, unsigned int p_divs)
{
	u_div_t	w_div;

	if (p_divs == 0) { 
		ZERO_DIV;
		return (0);
	}

	w_div.rtbuf = __udivrem(p_divi, p_divs);
	return (w_div.u_res.u_d);
}

/*
 * unsigned remainder
 */
unsigned int
__umodsi3(unsigned int p_divi, unsigned int p_divs)
{
	u_div_t	w_div;

	if (p_divs == 0) { 
		ZERO_DIV;
		return (0);
	}

	w_div.rtbuf = __udivrem(p_divi, p_divs);
	return (w_div.u_res.u_r);
}

