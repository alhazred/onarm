#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

@ _START_

# Message file for cmd/sgs/libld - ARM specific.

@ MSG_REL_NOREG		"relocation error: REGISTER relocation not supported \
			 on ARM architecture"

@ MSG_REL_FORCEPLT	"file %s: input %s: symbol %s: off:0x%x: use %s \
			relocation type instead of %s"

@ MSG_ATTR_BADVER	"file %s: bad attribute format version"

@ MSG_ATTR_BADLEN	"file %s: bad attribute section length: 0x%x, 0x%x"

@ MSG_ATTR_BADSUBLEN	"file %s: bad attribute subsection length: 0x%x, 0x%x"

@ MSG_ATTR_UNKFTAG	"file %s: unknown file attribute tag: %d"

@ MSG_ATTR_DUPFTAG	"file %s: duplicated file attribute tag: %d"

@ MSG_ATTR_CFLFTAG	"file %s: file attribute conflicts: tag = %d, \
			 value = %d"

@ _END_

@ MSG_ARG_EABI		"eabi="
@ MSG_ARG_FORCEPLT	"forceplt"
@ MSG_ARG_FORCEPLT_EQ	"forceplt="
@ MSG_ARG_FPLT_NONE	"none"
@ MSG_ARG_FPLT_WARN	"warn"
@ MSG_ARG_FPLT_FORCE	"force"

# Symbol names
@ MSG_SYM_AEABI_READ_TP_UU	"__aeabi_read_tp"

# Section names
@ MSG_SCN_INIT_ARRAY		".init_array"
@ MSG_SCN_FINI_ARRAY		".fini_array"
@ MSG_SCN_RODATA		".rodata"
@ MSG_SCN_TDATA			".tdata"
@ MSG_SCN_DATA_REL_RO		".data.rel.ro"
@ MSG_SCN_DEBUG_INFO		".debug_info"

@ MSG_LINKONCE_PREFIX		".gnu.linkonce."

@ MSG_LINKONCE_TYPE_TEXT		"t."
@ MSG_LINKONCE_TYPE_RODATA		"r."
@ MSG_LINKONCE_TYPE_DATA		"d."
@ MSG_LINKONCE_TYPE_TDATA		"td."
@ MSG_LINKONCE_TYPE_TBSS		"tb."
@ MSG_LINKONCE_TYPE_DATA_REL_RO		"d.rel.ro."
@ MSG_LINKONCE_TYPE_BSS			"b."
@ MSG_LINKONCE_TYPE_DEBUG_INFO		"wi."
