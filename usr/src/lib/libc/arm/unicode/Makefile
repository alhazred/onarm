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

LIBRARY=	unicode.a
VERS=		.1
OBJECTS=	u8_textprep.o uconv.o

include $(SRC)/lib/Makefile.lib
include $(SRC)/lib/Makefile.rootfs

LIBS =		$(DYNLIB) $(LINTLIB)
MAPFILES =	unicode-mapfile-vers

$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)

COMDIR = 	$(SRC)/common/unicode

CFLAGS +=	$(CCVERBOSE)
LDLIBS +=	../libc.so.1
SRCS=	$(UNICODEOBJS:%.o=$(SRC)/common/unicode/%.c)

.KEEP_STATE:

all : $(LIBS) fnamecheck
install: all $(ROOTLIBS) $(ROOTLINKS) $(ROOTLINT)

lint :
	@echo $(LINT.c) ...
	@$(LINT.c) $(SRCS) $(LDLIBS)

pics/%.o:	 $(COMDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)


include $(SRC)/lib/Makefile.targ
