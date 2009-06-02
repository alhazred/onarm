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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"@(#)Makefile	1.7	06/03/07 SMI"
#

include		$(SRC)/cmd/sgs/librtld/Makefile.com

CPPFLAGS +=	-I../../../../uts/arm/krtld
MAPFILES +=	../common/mapfile-32-vers

ASOBJ =		librtld_sym.o
ASFLAGS =	-P -D_ASM $(AS_CPPFLAGS)
ASFLAGS +=	$($(MACH)_AS_XARCH) $(AS_PICFLAGS)
BUILD.s =	$(AS) $(ASFLAGS)

$(__GNULD)MACHOBJS	+= $(ASOBJ)
$(__GNULD)ZDEFS		=

VERS_SCRIPT	= vers-script

.KEEP_STATE:

all:		$(DYNLIB) $(LIBLINKS)

install:	all $(ROOTFS_DYNLIB)

lint:		$(LINTLIB) $(LINTOUT32) $(SGSLINTOUT)

pics/%.o:	%.s
		$(BUILD.s) -o $@ $<
		$(POST_PROCESS_O)

include		$(SRC)/cmd/sgs/librtld/Makefile.targ
