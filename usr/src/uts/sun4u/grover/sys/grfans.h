/*
 * Copyright (c) 2000 by Sun Microsystems, Inc.
 * All rights reserved.
 */

#ifndef _GRFANS_H
#define	_GRFANS_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#ifdef	__cplusplus
extern "C" {
#endif

#define	UNKNOWN_OUT	-1

#define	MINOR_TO_DEVINST(x) ((x & 0xf00) >> 8)
#define	MINOR_TO_CHANNEL(x) (x & 0x00f)

#define	CPU_FAN_CHANNEL		0x0
#define	SYSTEM_FAN_CHANNEL	0x1

#define	CHANNEL_TO_MINOR(x) (x)
#define	DEVINST_TO_MINOR(x) (x << 8)

#define	FANS_NODE_TYPE "ddi_env:fan"

#define	CPU_FAN_0	0x01
#define	CPU_FAN_25	0x05
#define	CPU_FAN_50	0x09
#define	CPU_FAN_75	0x0d
#define	CPU_FAN_100	0x00

#define	CPU_FAN_MASK	0x0d

#define	SYS_FAN_OFF	0x02
#define	SYS_FAN_ON	0x00

struct grfans_unit {
	kmutex_t	mutex;
	uint8_t		flags;
	int8_t		sysfan_output;
	int8_t		cpufan_output;
	uint16_t	oflag[2];
	ddi_acc_handle_t cpufan_rhandle;
	ddi_acc_handle_t sysfan_rhandle;
	uint8_t		*cpufan_reg;
	uint8_t		*sysfan_reg;
};

#ifdef	__cplusplus
}
#endif

#endif /* _GRFANS_H */
