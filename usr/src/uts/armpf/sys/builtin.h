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
 * Copyright (c) 2008 NEC Corporation
 * All rights reserved.
 */

#ifndef	_SYS_BUILTIN_H
#define	_SYS_BUILTIN_H

#ident	"@(#)armpf/sys/builtin.h"

/*
 * Definitions to create device nodes for built-in devices.
 * Kernel build tree private.
 */

#include <sys/types.h>
#include <sys/dditypes.h>

/* Device properties for built-in device */
typedef struct builtin_prop {
	const char	*bp_name;	/* property name */
	const void	*bp_value;	/* property value */
	const uint_t	bp_nelems;	/* number of value elements */
} builtin_prop_t;

/* Device definition */
typedef struct builtin_dev {
	const char		*bd_name;	/* node name */

	/* Property definitions */
	const builtin_prop_t	*bd_prop_int;
	const builtin_prop_t	*bd_prop_int64;
	const builtin_prop_t	*bd_prop_string;
	const builtin_prop_t	*bd_prop_byte;
	const char		**bd_prop_boolean;

	/* Number of properties */
	const uint8_t		bd_nprops_int;
	const uint8_t		bd_nprops_int64;
	const uint8_t		bd_nprops_string;
	const uint8_t		bd_nprops_byte;
	const uint8_t		bd_nprops_boolean;
} builtin_dev_t;

/* Property names */
#define	BUILTIN_PROPNAME_UART_PORT	"motherboard-serial-ports"
#define	BUILTIN_PROPNAME_REG		"reg"

/*
 * Helper macros to set properties for built-in device.
 */
#ifdef	__STDC__
#define	BUILTIN_PROP(dev, type)		(dev)->bd_prop_##type
#define	BUILTIN_NPROPS(dev, type)	(dev)->bd_nprops_##type
#define	BUILTIN_PROP_SET(type)		ndi_prop_update_##type##_array
#else	/* !__STDC__ */
#define	BUILTIN_PROP(dev, type)		(dev)->bd_prop_/**/type
#define	BUILTIN_NPROPS(dev, type)	(dev)->bd_nprops_/**/type
#define	BUILTIN_PROP_SET(type)		ndi_prop_update_/**/type/**/_array
#endif	/* __STDC__ */

extern const builtin_dev_t	builtin_dev[];
extern const uint_t		builtin_ndevs;
extern const int		builtin_uart_port[];
extern const uint_t		builtin_uart_nports;

/* Prototypes */
extern void	builtin_uart_device_init(void);
extern void	builtin_device_create(dev_info_t *parent,
				      const builtin_dev_t *dev);

#endif	/* !_SYS_BUILTIN_H */
