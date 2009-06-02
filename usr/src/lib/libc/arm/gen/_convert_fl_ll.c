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
 * Copyright (c) 2007-2009 NEC Corporation
 * All rights reserved.
 */

#pragma ident	"@(#)_convert_fl_ll.c"

#include "synonyms.h"
#include "base_conversion.h"

#if defined(__ARM_EABI__)
#pragma weak __aeabi_d2lz = __fixdfdi
#pragma weak __aeabi_d2ulz = __fixunsdfdi
#pragma weak __aeabi_f2lz = __fixsfdi
#pragma weak __aeabi_f2ulz = __fixunssfdi
#endif

	union {
		double_formatted conv_data ;
		double  in_data ;
	} conv_dbl_tbl ;
	union {
		single_formatted conv_data ;
		float  in_data ;
	} conv_flt_tbl ;
	union {
		u_longlong_t  u_ll_data ;
		longlong_t  ll_data ;
		long        l_data[2] ;
	} ll_buf ;

/*
 * convert double to long long
 *
 * exponent < 1023  ->  0
 * high word
 *   (exponent-32) 
 *   (long)indata -> high word
 * low  word
 * if (sign == 1 && high word == 0)  ::: indata <= 2**32 - 1
 *    (long)indata -> low word
 * else 
 *    tmp = indata - (high word data)
 *    (unsigned long)tmp -> low word
 */
longlong_t
__fixdfdi(double pdata) {
	int	stat = 0;
	int	sign_fg = 0;

	conv_dbl_tbl.in_data = pdata ;
	if (conv_dbl_tbl.conv_data.msw.exponent < 1023) return(0ll) ;
	if (conv_dbl_tbl.conv_data.msw.exponent == 0x7ff) {
		if ((conv_dbl_tbl.conv_data.msw.significand == 0) &&
		    (conv_dbl_tbl.conv_data.significand2 == 0)       ) {
			if (conv_dbl_tbl.conv_data.msw.sign == 0) {
				stat = 1;	/*  Inf */
			} else {
				stat = 2;	/* -Inf */
			}
		} else {
			stat = 3;		/*  NaN */
		}
	} else {
		if (conv_dbl_tbl.conv_data.msw.sign == 0) {
			if (conv_dbl_tbl.conv_data.msw.exponent >= 63 + 0x3ff) {
				stat = 1;	/*  Inf (longlong over flow) */
			}
		} else {
			if (conv_dbl_tbl.conv_data.msw.exponent >= 63 + 0x3ff) {
				stat = 2;	/* -Inf (longlong over flow) */
			}
		}
	}

	if (conv_dbl_tbl.conv_data.msw.sign != 0) {
		sign_fg = 1;
		conv_dbl_tbl.conv_data.msw.sign = 0;
		pdata = conv_dbl_tbl.in_data;
	}

	conv_dbl_tbl.conv_data.msw.exponent -= 32 ;
	ll_buf.l_data[1] = (long)conv_dbl_tbl.in_data ;
	ll_buf.l_data[0] = 0 ;
	ll_buf.l_data[0] = (unsigned long)(pdata - (double)ll_buf.ll_data) ;

	if (sign_fg != 0) {
		ll_buf.ll_data = -ll_buf.ll_data;
	}

	switch (stat) {
	case 1:
		return(0x7fffffffffffffffll);	/*  Inf */
	case 2:
		return(0x8000000000000000ll);	/* -Inf */
	case 3:
		return(0x0000000000000000ll);	/* NaN  */
	}
	return(ll_buf.ll_data) ;		/* normal */
}

/*
 * convert double to unsigned long long
 *
 * exponent < 1023  ->  0
 * high word
 *   (exponent-32) 
 *   (unsigned long)indata -> high word
 * low  word
 * if (high word == 0)  ::: indata <= 2**32 - 1
 *    (unsigned long)indata -> low word
 * else
 *    tmp = indata - (high word data)
 *    (unsigned long)tmp -> low word
 *
 */
u_longlong_t
__fixunsdfdi(double pdata) {
	int	stat = 0;
	int	sign_fg = 0;

	conv_dbl_tbl.in_data = pdata ;
	if (conv_dbl_tbl.conv_data.msw.exponent < 1023) return(0ll) ;
	if (conv_dbl_tbl.conv_data.msw.exponent == 0x7ff) {
		if ((conv_dbl_tbl.conv_data.msw.significand == 0) &&
		    (conv_dbl_tbl.conv_data.significand2 == 0)       ) {
			if (conv_dbl_tbl.conv_data.msw.sign == 0) {
				stat = 1;	/*  Inf */
			} else {
				stat = 2;	/* -Inf */
			}
		} else {
			stat = 3;		/*  NaN */
		}
	} else {
		if (conv_dbl_tbl.conv_data.msw.sign == 0) {
			if (conv_dbl_tbl.conv_data.msw.exponent >= 64 + 0x3ff) {
				stat = 1;	/*  Inf (longlong over flow) */
			}
		} else {
			stat = 2;		/*  0 (ulonglong is not minus) */
		}
	}

	if (conv_dbl_tbl.conv_data.msw.sign != 0) {
		sign_fg = 1;
		conv_dbl_tbl.conv_data.msw.sign = 0;
		pdata = conv_dbl_tbl.in_data;
	}

	conv_dbl_tbl.conv_data.msw.exponent -= 32 ;
	ll_buf.l_data[1] = (unsigned long)conv_dbl_tbl.in_data ;
	if (ll_buf.l_data[1] == 0) {
		conv_dbl_tbl.conv_data.msw.exponent += 32 ;
		ll_buf.l_data[0] = (unsigned long)conv_dbl_tbl.in_data ;
	} else {
		ll_buf.l_data[0] = 0 ;
		ll_buf.l_data[0] = 
			(unsigned long)(pdata - (double)ll_buf.u_ll_data) ;
	}

	if (sign_fg != 0) {
		ll_buf.ll_data = -ll_buf.ll_data;
	}

	switch (stat) {
	case 1:
		return(0xffffffffffffffffll);	/*  Inf */
	case 2:
		return(0x0000000000000000ll);	/*  0   */
	case 3:
		return(0x0000000000000000ll);	/* NaN  */
	}
	return(ll_buf.u_ll_data) ;		/* normal */
}

/*
 * convert float to long long
 *
 * exponent < 127  ->  0
 * high word
 *   (exponent-32) 
 *   (long)indata -> high word
 * low  word
 * if (sign == 1 && high word == 0)
 *    (long)indata -> low word
 * else 
 *    tmp = indata - (high word data)
 *    (unsigned long)tmp -> low word
 */
longlong_t
__fixsfdi(float pdata) {
	int	stat = 0;
	int	sign_fg = 0;

	conv_flt_tbl.in_data = pdata ;
	if (conv_flt_tbl.conv_data.msw.exponent < 127) return(0ll) ;
	if (conv_flt_tbl.conv_data.msw.exponent == 0xff) {
		if (conv_flt_tbl.conv_data.msw.significand == 0) {
			if (conv_flt_tbl.conv_data.msw.sign == 0) {
				stat = 1;	/*  Inf */
			} else {
				stat = 2;	/* -Inf */
			}
		} else {
			stat = 3;		/*  NaN */
		}
	} else {
		if (conv_flt_tbl.conv_data.msw.sign == 0) {
			if (conv_flt_tbl.conv_data.msw.exponent >= 63 + 0x7f) {
				stat = 1;	/*  Inf (longlong over flow) */
			}
		} else {
			if (conv_flt_tbl.conv_data.msw.exponent >= 63 + 0x7f) {
				stat = 2;	/* -Inf (longlong over flow) */
			}
		}
	}

	if (conv_flt_tbl.conv_data.msw.sign != 0) {
		sign_fg = 1;
		conv_flt_tbl.conv_data.msw.sign = 0;
		pdata = conv_flt_tbl.in_data;
	}

	conv_flt_tbl.conv_data.msw.exponent -= 32 ;
	ll_buf.l_data[1] = (long)conv_flt_tbl.in_data ;
	ll_buf.l_data[0] = 0 ;
	ll_buf.l_data[0] = (unsigned long)(pdata - (float)ll_buf.ll_data) ;

	if (sign_fg != 0) {
		ll_buf.ll_data = -ll_buf.ll_data;
	}

	switch (stat) {
	case 1:
		return(0x7fffffffffffffffll);	/*  Inf */
	case 2:
		return(0x8000000000000000ll);	/* -Inf */
	case 3:
		return(0x0000000000000000ll);	/* NaN  */
	}
	return(ll_buf.ll_data) ;		/* normal */
}

/*
 * convert float to unsigned long long
 *
 * exponent < 127  ->  0
 * high word
 *   (exponent-32) 
 *   (unsigned long)indata -> high word
 * low  word
 * if (high word == 0)
 *    (unsigned long)indata -> low word
 * else 
 *    tmp = indata - (high word data)
 *    (unsigned long)tmp -> low word
 */
u_longlong_t
__fixunssfdi(float pdata) {
	int	stat = 0;
	int	sign_fg = 0;

	conv_flt_tbl.in_data = pdata ;
	if (conv_flt_tbl.conv_data.msw.exponent < 127) return(0ll) ;
	if (conv_flt_tbl.conv_data.msw.exponent == 0xff) {
		if (conv_flt_tbl.conv_data.msw.significand == 0) {
			if (conv_flt_tbl.conv_data.msw.sign == 0) {
				stat = 1;	/*  Inf */
			} else {
				stat = 2;	/* -Inf */
			}
		} else {
			stat = 3;		/*  NaN */
		}
	} else {
		if (conv_flt_tbl.conv_data.msw.sign == 0) {
			if (conv_flt_tbl.conv_data.msw.exponent >= 64 + 0x7f) {
				stat = 1;	/*  Inf (longlong over flow) */
			}
		} else {
			stat = 2;		/*  0 (ulonglong is not minus) */
		}
	}

	if (conv_flt_tbl.conv_data.msw.sign != 0) {
		sign_fg = 1;
		conv_flt_tbl.conv_data.msw.sign = 0;
		pdata = conv_flt_tbl.in_data;
	}

	conv_flt_tbl.conv_data.msw.exponent -= 32 ;
	ll_buf.l_data[1] = (unsigned long)conv_flt_tbl.in_data ;
	if ( ll_buf.l_data[1] == 0 ) {
		conv_flt_tbl.conv_data.msw.exponent += 32 ;
		ll_buf.l_data[0] = (unsigned long)conv_flt_tbl.in_data ;
	} else {
		ll_buf.l_data[0] = 0 ;
		ll_buf.l_data[0] = 
			(unsigned long)(pdata - (float)ll_buf.u_ll_data) ;
	}

	if (sign_fg != 0) {
		ll_buf.ll_data = -ll_buf.ll_data;
	}

	switch (stat) {
	case 1:
		return(0xffffffffffffffffll);	/*  Inf */
	case 2:
		return(0x0000000000000000ll);	/*  0   */
	case 3:
		return(0x0000000000000000ll);	/* NaN  */
	}
	return(ll_buf.u_ll_data) ;		/* normal */
}

