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
# Copyright (c) 2007 NEC Corporation
#

# ident	"%Z%%M%	%I%	%E% SMI"
#

LIBRARY=	libdevid.a
VERS=		.1
OBJECTS=	deviceid.o devid.o devid_scsi.o

include ../../Makefile.lib
include ../../Makefile.rootfs

SRCS =		../deviceid.c $(SRC)/common/devid/devid.c \
		$(SRC)/common/devid/devid_scsi.c
$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)
LIBS =		$(ARLIB) $(DYNLIB) $(LINTLIB)
#
# Libraries added to the next line must be present in miniroot
#
LDLIBS +=	-ldevinfo -lc

CFLAGS +=	$(CCVERBOSE)

.KEEP_STATE:

all: $(LIBS)

lint: lintcheck

include ../../Makefile.targ

objs/devid.o pics/devid.o:	 $(SRC)/common/devid/devid.c
	$(COMPILE.c) -o $@ $(SRC)/common/devid/devid.c
	$(POST_PROCESS_O)

objs/devid_scsi.o pics/devid_scsi.o:	 $(SRC)/common/devid/devid_scsi.c
	$(COMPILE.c) -o $@ $(SRC)/common/devid/devid_scsi.c
	$(POST_PROCESS_O)
