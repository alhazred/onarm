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
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Copyright (c) 2008 NEC Corporation
 */

#ifndef _CZFS_SPA_H
#define	_CZFS_SPA_H

#pragma ident	"@(#)czfs:spa.h"

#include <sys/czfs_conf.h>

#ifdef	__cplusplus
extern "C" {
#endif

#define	BP_SHOULD_BYTESWAP(bp)	0
#define	SPA_MAXBLOCKSHIFT	16
#define	DVAWORDLEN		1
#define	SPA_BLKPTRSHIFT		6
#define	SPA_DVAS_PER_BP		2

/*
 * Each block is described by its DVAs, time of birth, checksum, etc.
 * The word-by-word, bit-by-bit layout of the blkptr is as follows:
 *
 *	64	56	48	40	32	24	16	8	0
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 0	|G|vdev|          ASIZE      |           offset1                |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 1	|G|vdev|          ASIZE      |           offset2                |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 2	|E| lvl	| type	| cksum | comp	|     PSIZE     |     LSIZE     |
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 3	|	birth txg		|	fill count		|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 4	|			checksum[0]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 5	|			checksum[1]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 6	|			checksum[2]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 * 7	|			checksum[3]				|
 *	+-------+-------+-------+-------+-------+-------+-------+-------+
 *
 * Legend:
 *
 * vdev		virtual device ID
 * offset	offset into virtual device
 * LSIZE	logical size
 * PSIZE	physical size (after compression)
 * ASIZE	allocated size (including RAID-Z parity and gang block headers)
 * GRID		RAID-Z layout information (reserved for future use)
 * cksum	checksum function
 * comp		compression function
 * G		gang block indicator
 * E		endianness
 * type		DMU object type
 * lvl		level of indirection
 * birth txg	transaction group in which the block was born
 * fill count	number of non-zero blocks under this bp
 * checksum[4]	256-bit checksum of the data this bp describes
 */

#define	DVA_GET_ASIZE(dva)	\
	BF64_GET_SB((dva)->dva_word[0], 34, 24, SPA_MINBLOCKSHIFT, 0)
#define	DVA_SET_ASIZE(dva, x)	\
	BF64_SET_SB((dva)->dva_word[0], 34, 24, SPA_MINBLOCKSHIFT, 0, x)

#define	DVA_GET_VDEV(dva)	BF64_GET((dva)->dva_word[0], 58, 5)
#define	DVA_SET_VDEV(dva, x)	BF64_SET((dva)->dva_word[0], 58, 5, x)

#define	DVA_GET_OFFSET(dva)	\
	BF64_GET_SB((dva)->dva_word[0], 0, 34, SPA_MINBLOCKSHIFT, 0)
#define	DVA_SET_OFFSET(dva, x)	\
	BF64_SET_SB((dva)->dva_word[0], 0, 34, SPA_MINBLOCKSHIFT, 0, x)

#define	DVA_GET_GANG(dva)	BF64_GET((dva)->dva_word[0], 63, 1)
#define	DVA_SET_GANG(dva, x)	BF64_SET((dva)->dva_word[0], 63, 1, x)

#define	BP_GET_ASIZE(bp)	\
	(DVA_GET_ASIZE(&(bp)->blk_dva[0]) + DVA_GET_ASIZE(&(bp)->blk_dva[1]))

#define	BP_GET_NDVAS(bp)	\
	(!!DVA_GET_ASIZE(&(bp)->blk_dva[0]) + \
	!!DVA_GET_ASIZE(&(bp)->blk_dva[1]))

#define	BP_COUNT_GANG(bp)	\
	(DVA_GET_GANG(&(bp)->blk_dva[0]) + \
	DVA_GET_GANG(&(bp)->blk_dva[1]))

#define	DVA_EQUAL(dva1, dva2)	\
	((dva1)->dva_word[0] == (dva2)->dva_word[0])

#define	BP_ZERO_DVAS(bp)			\
{						\
        (bp)->blk_dva[0].dva_word[0] = 0;	\
        (bp)->blk_dva[0].dva_word[1] = 0;	\
        (bp)->blk_dva[1].dva_word[0] = 0;	\
        (bp)->blk_dva[1].dva_word[1] = 0;	\
        (bp)->blk_birth = 0;			\
}

#define	BP_ZERO(bp)				\
{						\
	(bp)->blk_dva[0].dva_word[0] = 0;	\
	(bp)->blk_dva[1].dva_word[0] = 0;	\
	(bp)->blk_prop = 0;			\
	(bp)->blk_birth = 0;			\
	(bp)->blk_fill = 0;			\
	ZIO_SET_CHECKSUM(&(bp)->blk_cksum, 0, 0, 0, 0); \
}

#include <../zfs/sys/spa.h>

#undef	spa_config_load_arch

extern int spa_config_load_arch(nvlist_t **);

#ifdef	__cplusplus
}
#endif

#endif	/* _CZFS_SPA_H */
