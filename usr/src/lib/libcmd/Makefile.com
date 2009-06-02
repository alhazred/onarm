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
# ident	"@(#)Makefile.com	1.21	07/07/26 SMI"
#

SHELL=/usr/bin/ksh

LIBRARY =	libcmd.a
VERS =		.1
OBJECTS =	\
	basename.o \
	cat.o \
	chgrp.o \
	chmod.o \
	chown.o \
	cmdinit.o \
	cmp.o \
	comm.o \
	cp.o \
	cut.o \
	date.o \
	dirname.o \
	expr.o \
	fds.o \
	fmt.o \
	fold.o \
	getconf.o \
	head.o \
	id.o \
	join.o \
	ln.o \
	logname.o \
	mkdir.o \
	mkfifo.o \
	mv.o \
	paste.o \
	pathchk.o \
	rev.o \
	revlib.o \
	rm.o \
	rmdir.o \
	stty.o \
	sync.o \
	tail.o \
	tee.o \
	tty.o \
	uname.o \
	uniq.o \
	wc.o \
	wclib.o

include ../../Makefile.astmsg

include ../../Makefile.lib

# mapfile-vers does not live with the sources in in common/ to make
# automated code updates easier.
MAPFILES=       ../mapfile-vers

# Set common AST build flags (e.g., needed to support the math stuff).
include ../../../Makefile.ast

LIBS =		$(DYNLIB) $(LINTLIB)

$(LINTLIB) :=	SRCS = $(SRCDIR)/$(LINTSRC)
LDLIBS += \
	-z lazyload -last -lsocket -lnsl -z nolazyload \
	-lc

SRCDIR =	../common

# We use "=" here since using $(CPPFLAGS.master) is very tricky in our
# case - it MUST come as the last element but future changes in -D options
# may then cause silent breakage in the AST sources because the last -D
# option specified overrides previous -D options so we prefer the current
# way to explicitly list each single flag.
# Notes:
#   - "-D_BLD_DLL" comes from ${mam_cc_DLL} in Mamfile
CPPFLAGS = \
	$(DTEXTDOM) $(DTS_ERRNO) \
	-I../common \
	-Isrc/lib/libcmd \
	-I$(ROOT)/usr/include/ast \
	-D_BLD_cmd \
	-D_PACKAGE_ast \
	-D_BLD_DLL \
	'-DERROR_CATALOG="libcmd"' \
	'-DUSAGE_LICENSE=\
		"[-author?Glenn Fowler <gsf@research.att.com>]"\
		"[-author?David Korn <dgk@research.att.com>]"\
		"[-copyright?Copyright (c) 1992-2007 AT&T Knowledge Ventures]"\
		"[-license?http://www.opensource.org/licenses/cpl1.0.txt]"\
		"[--catalog?libcmd]"'

CFLAGS += \
	$(CCVERBOSE) \
	-xstrconst
CFLAGS64 += \
	$(CCVERBOSE) \
	-xstrconst

pics/sync.o	:= CERRWARN += -erroff=E_END_OF_LOOP_CODE_NOT_REACHED

.KEEP_STATE:

all: $(LIBS) 

#
# libcmd is not lint-clean yet; fake up a target.  (You can use
# "make lintcheck" to actually run lint; please send all lint fixes
# upstream (to AT&T) so the next update will pull them into ON.)
#
lint:
	@ print "usr/src/lib/libcmd is not lint-clean: skipping"
	@ $(TRUE)

include ../../Makefile.targ
