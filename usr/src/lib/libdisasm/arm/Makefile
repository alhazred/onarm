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
# ident	"@(#)Makefile	1.1	06/03/02 SMI"

#
# Copyright (c) 2008 NEC Corporation
#

ISASRCDIR=.

include ../Makefile.com

TYPES=library standalone cross

INSTALL_DEPS_library =		$(ROOTLINKS) $(ROOTLINT) $(ROOTLIBS)
INSTALL_DEPS_standalone =	$(ROOTLIBS)

CPPFLAGS	+= -I$(SRC)/common/dis/arm

ALWAYS_LINT_DEFS	+= -erroff=E_CONSTANT_CONDITION

include ../Makefile.targ

# install rule for ARM common source
objs/%.o pics/%.o:	$(SRC)/common/dis/arm/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

$(STANDLIBRARY)	:= ZDEFS=

# install rules for cross object
CROSS		= libcrossdisasm.o
CROSS_OBJECTS	= $(OBJECTS:%=cross-objs/%)
CLEANFILES	+= $(CROSS_OBJECTS)
CLOBBERFILES	+= $(CROSS)

cross:	cross-objs .WAIT $(CROSS)

all.cross.targ install.cross.targ:	cross

$(CROSS):	$(CROSS_OBJECTS)
	$(NATIVELD) $(LDFLAGS) -r -o $@ $(CROSS_OBJECTS)

cross	:=	CC = GNU_ROOT=$(NATIVE_GNU_ROOT) $($(NATIVE_MACH)_CC)
cross	:=	CFLAGS = $(NATIVE_CFLAGS)
cross	:=	CPPFLAGS += -D__arm

cross-objs:
	-@mkdir -p $@

cross-objs/%.o:	$(ISASRCDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

cross-objs/%.o:	$(COMDIR)/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)

cross-objs/%.o:	$(SRC)/common/dis/arm/%.c
	$(COMPILE.c) -o $@ $<
	$(POST_PROCESS_O)
