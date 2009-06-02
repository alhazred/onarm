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

# ident	"@(#)Makefile.com	1.5	07/05/14 SMI"
#

LIBRARY =	librestart.a
VERS =		.1
OBJECTS = \
	librestart.o

include ../../Makefile.lib
include ../../Makefile.rootfs

LIBS =		$(ARLIB) $(DYNLIB) $(LINTLIB)

START_LAZY =	$(ZLAZYLOAD)
END_LAZY =	$(ZNOLAZYLOAD)

lintcheck := START_LAZY =
lintcheck := END_LAZY =

LDLIBS +=	$(START_LAZY) -lpool -lproject $(END_LAZY) \
		-lsecdb -lnvpair -lsysevent -lscf -luutil -lc

SRCDIR =	../common
$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

CFLAGS +=	$(CCVERBOSE) -Wp,-xc99=%all

CPPFLAGS +=

.KEEP_STATE:

all:

lint: lintcheck

include ../../Makefile.targ
