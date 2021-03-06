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

LIBRARY = watchmalloc.a
VERS = .1

OBJECTS = malloc.o

# include library definitions
include ../../Makefile.lib

SRCDIR = ../common

LIBS = $(ARLIB) $(DYNLIB)
LDLIBS += -lc
CFLAGS += $(CCVERBOSE)
CFLAGS64 += $(CCVERBOSE)
CPPFLAGS += -I../common -I../../common/inc -D_REENTRANT
DYNFLAGS += $(ZINTERPOSE)

.KEEP_STATE:

all: $(LIBS)

lint:
	$(LINT.c) $(SRCS) $(LDLIBS)

# include library targets
include ../../Makefile.targ

pics/%.o: ../common/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
