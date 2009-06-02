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
/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/


#ident	"@(#)data.c	1.6	05/06/08 SMI"	/* SVr4.0 1.1 */
/* LINTLIBRARY */

/*
 *  data.c
 *
 *	This file contains global data for the library "liboam".
 *
 *	FILE   *oam_devtab	The open file descriptor for the current
 *				device table.
 *	FILE   *oam_dgroup	The open file descriptor for the current
 *				device-group table.
 */


/*
 *  Header files needed:
 *	<stdio.h>	Standard I/O definitions
 */

#include	<stdio.h>



FILE		*oam_devtab = (FILE *) NULL;
FILE		*oam_dgroup = (FILE *) NULL;
