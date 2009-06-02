#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
# Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.

#
# Copyright (c) 2007-2008 NEC Corporation
#

#pragma ident	"@(#)rtld.arm.msg"

@ _START_

# Message file for cmd/sgs/rtld (ld.so.1) - 32bit arm specific.

@ _END_

@ MSG_SYM_CTYPE		"__ctype"
@ MSG_SYM_IOB		"__iob"
@ MSG_SYM_LAARMPLTENTER	"la_arm_pltenter"
@ MSG_PTH_LIBC		"/usr/lib/libc.so.1"
#error message for doreloc.c support
@ MSG_REL_NOFIT		"relocation error: %s: file %s: symbol %s: \
			 value 0x%llx does not fit"

@ MSG_FIL_LIBRTLD_V	"librtld.so.1"
@ MSG_FIL_LIBRTLD	"librtld.so"

@ MSG_FIL_LIBLDDBG_V	"liblddbg.so.4"
@ MSG_FIL_LIBLDDBG	"liblddbg.so"

@ MSG_FIL_LIBLD_V	"libld.so.4"
@ MSG_FIL_LIBLD		"libld.so"

@ MSG_PT_SUNWCAP	"PT_SUNWCAP"
