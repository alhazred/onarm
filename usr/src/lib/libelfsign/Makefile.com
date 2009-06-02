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
# ident	"@(#)Makefile.com	1.20	07/09/19 SMI"
#

LIBRARY = libelfsign.a
VERS = .1

OBJECTS = \
	elfcertlib.o \
	elfsignlib.o

include $(SRC)/lib/Makefile.lib

SRCDIR =	../common

LIBS =		$(DYNLIB) $(LINTLIB)
$(LINTLIB):=	SRCS = $(SRCDIR)/$(LINTSRC)

LDLIBS +=	-lmd -lelf -lkmf -lcryptoutil -lc

MAPFILE =	mapfile
MAPFILES =	$(MAPFILE)

CFLAGS +=	$(CCMT) $(CCVERBOSE)
CPPFLAGS +=	-D_REENTRANT -D_POSIX_PTHREAD_SEMANTICS

.KEEP_STATE:

all:		$(LIBS)

lint:		lintcheck

$(MAPFILE):	$(SRCDIR)/$(MAPFILE).map
		$(RM) $@
		$(CPP) $(SRCDIR)/$(MAPFILE).map > $@

CLEANFILES +=	$(MAPFILE)

include $(SRC)/lib/Makefile.targ
