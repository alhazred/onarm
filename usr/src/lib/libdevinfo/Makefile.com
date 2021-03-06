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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

# ident	"%Z%%M%	%I%	%E% SMI"
#

include ../../../Makefile.master

LIBRARY=	libdevinfo.a
VERS=		.1

DEVINFO_RETIRE_OBJ =	devinfo_retire.o
$(ARM_BLD)DEVINFO_RETIRE_OBJ =
OBJECTS=	devfsinfo.o devinfo.o devinfo_prop_decode.o devinfo_devlink.o \
		devinfo_devperm.o devfsmap.o devinfo_devname.o \
		devinfo_finddev.o devinfo_dli.o devinfo_dim.o \
		devinfo_realpath.o $(DEVINFO_RETIRE_OBJ)

include ../../Makefile.lib
include ../../Makefile.rootfs

LIBS =		$(ARLIB) $(DYNLIB) $(LINTLIB)
LDLIBS +=	-lnvpair -lsec -lc -lgen
$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

CFLAGS +=	$(CCVERBOSE)
CPPFLAGS +=	-I..

.KEEP_STATE:

all: $(LIBS)

lint: lintcheck

include ../../Makefile.targ
