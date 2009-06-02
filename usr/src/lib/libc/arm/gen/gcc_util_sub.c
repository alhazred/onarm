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

#pragma ident	"@(#)gcc_util_sub.c"

#include "synonyms.h"
#include "base_conversion.h"

#if defined(__ARM_EABI__)
#pragma weak __aeabi_dcmpun = __unorddf2
#pragma weak __aeabi_fcmpun = __unordsf2
#endif

	union {
		double_formatted conv_data ;
		double  in_data ;
	} ddata;
	union {
		single_formatted conv_data ;
		float  in_data ;
	} fdata;

/*
 * __unordsf2(float A,float B)
 */
int
__unordsf2(float a,float b)
{
	fdata.in_data = a;
	if ((fdata.conv_data.msw.exponent == 0xff) && 
	    (fdata.conv_data.msw.significand != 0)) {
		return(1) ;
	}
	fdata.in_data = b;
	if ((fdata.conv_data.msw.exponent == 0xff) && 
	    (fdata.conv_data.msw.significand != 0)) {
		return(1) ;
	}
	return(0) ;
}

/*
 * __unorddf2(double A,double B)
 */
int
__unorddf2(double a,double b)
{
	ddata.in_data = a;
	if ((ddata.conv_data.msw.exponent == 0x7ff) && 
	    ((ddata.conv_data.msw.significand != 0) ||
	     (ddata.conv_data.significand2 != 0))) {
		return(1) ;
	}
	ddata.in_data = b;
	if ((ddata.conv_data.msw.exponent == 0x7ff) && 
	    ((ddata.conv_data.msw.significand != 0) ||
	     (ddata.conv_data.significand2 != 0))) {
		return(1) ;
	}
	return(0) ;
}

/*
 * __unordtf2(long double A,long double B)
 *   sizeof(long double) == sizeof(double)
 *   So. call at __unorddf2
 */
int
__unordtf2(long double a,long double b)
{
	return(__unorddf2((double)a,(double)b)) ;
}
