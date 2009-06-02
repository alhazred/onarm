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

/* Common definition to manipulate static-linked kernel image */

#ifndef	_TOOLS_ELFDATAMOD_UNIXHACK_H
#define	_TOOLS_ELFDATAMOD_UNIXHACK_H

#include <gelf.h>
#include <string.h>

/*
 * Define section type for unix symbol hash.
 * Solaris kernel uses short for symbol hash entry. That's why we should
 * not use SHT_HASH as section type for symbol hash section. 
 * But local section type will make GNU objdump insane.
 * So we choose PROGBITS for symbol hash section type.
 */
#define	SHT_UNIX_SYMHASH		SHT_PROGBITS

/* Name of unix symbol hash section */
#define	SHNAME_UNIX_SYMHASH		".UNIX_hash"
#define	SHNAMELEN_UNIX_SYMHASH		10

/* Name of CTF data section */
#define	SHNAME_CTF			".SUNW_ctf"

/* Determine whether the specified section is unix symbol hash or not */
#define	SECTION_IS_UNIX_SYMHASH(shdr, sname)		\
	((shdr)->sh_type == SHT_UNIX_SYMHASH &&		\
	 strcmp(sname, SHNAME_UNIX_SYMHASH) == 0)

/*
 * Required address and size alignment for .bss section.
 * If you change this value, you must modify kernel bootstrap code.
 */
#define	UNIX_BSS_ALIGN	8

#endif	/* !_TOOLS_ELFDATAMOD_UNIXHACK_H */
