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
# Copyright (c) 2007-2008 NEC Corporation
# All rights reserved.
#

ASFLAGS +=	-P -D_ASM

include ../Makefile.com

VERS_SCRIPT	= vers-script
DYNFLAGS	+= $(LDFLAGS.SO)

$(__GNULD)OBJECTS	= filter_symbol.o
$(__GNULD)DYNFLAGS	+= ./pics/$(OBJECTS)

$(__NON_GNULD)DYNFLAGS	+= -F /usr/lib/ld.so.1

all:	$(DYNLIB)

include ../Makefile.targ

objs/%.o pics/%.o:	%.s
	$(COMPILE.s) -o $@ $<
	$(POST_PROCESS_O)

install:	all $(ROOTFS_DYNLIB) $(VAR_LIBDL_ETCDYNLIB) \
		$(ROOTFS_LINKS) $(LINT_TARGET)
