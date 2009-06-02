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

# ident	"@(#)Makefile.com	1.8	06/08/02 SMI"
#

LIBRARY=	libadt_jni.a
VERS=		.1

OBJECTS=	adt_jni.o	\
		adt_jni_event.o

include		$(SRC)/lib/Makefile.lib

LIBS =		$(ARLIB) $(DYNLIB) $(LINTLIB)

SRCDIR =	../common

CPPFLAGS +=	-I$(JAVA_ROOT)/include -I$(JAVA_ROOT)/include/solaris
CPPFLAGS +=	-D_REENTRANT

DYNFLAGS +=
LDLIBS +=	-lc -lbsm

CLEANFILES=	$(LINTOUT) $(LINTLIB)
CLOBBERFILES +=

$(LINTLIB) :=	SRCS=../common/llib-ladt_jni

LINTSRC=	$(LINTLIB:%.ln=%)
ROOTLINTDIR=	$(ROOTLIBDIR)
ROOTLINT=	$(LINTSRC:%=$(ROOTLINTDIR)/%)

.KEEP_STATE:

lint:		lintcheck

include		$(SRC)/lib/Makefile.targ

objs/%.o pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

# install rule for lint library target
$(ROOTLINTDIR)/%: ../common/%
	$(INS.file)
