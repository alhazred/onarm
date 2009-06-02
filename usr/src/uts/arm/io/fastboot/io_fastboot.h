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

#ifndef _IO_FASTBOOT_H
#define _IO_FASTBOOT_H

/* status of module */
#define IO_STS_INCOMP   0x00 /* waiting of initialization */
#define IO_STS_BUSY     0x01 /* in processing */
#define IO_STS_COMP     0x02 /* Completion of initialization */

/* Error Code */
#define IO_ERR_DUPLICATION -9

#define IO_MAX_PARENT      20
#define IO_MAX_STRING      80

typedef struct io_modinfo {
	char*    name;    /* module name  */
	char     status;  /* status of module */
	major_t  major;   /* major number */
	kmutex_t mutex;
	int      pcnt;    /* The number of dependence module */
	char     *parents;
	struct io_modinfo *qnext; /* Pointer to the next element of queue */
} io_modinfo_t;

/* function declarat */
extern void fastboot_thread_create_initdrv_parallel(void);
extern int fastboot_usb_init(void);

#endif /* _IO_FASTBOOT_H */
