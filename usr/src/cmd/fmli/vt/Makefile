#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License, Version 1.0 only
# (the "License").  You may not use this file except in compliance
# with the License.
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
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"
#
# cmd/fmli/vt/Makefile
#
LIBRARY = libvt.a

OBJECTS = fits.o hide.o highlight.o indicator.o lp.o makebox.o \
	message.o move.o offscreen.o physical.o redraw.o system.o \
	vclose.o vcolor.o vcreate.o vctl.o vcurrent.o vdebug.o \
	vflush.o vfork.o vinit.o vmark.o vreshape.o wclrwin.o \
	wdelchar.o wgetchar.o wgo.o winschar.o winprintf.o wputchar.o \
	winputs.o wreadchar.o wscrollwin.o showmail.o showdate.o \
	working.o

# include library definitions
include ../../../lib/Makefile.lib

C99MODE=	$(C99_DISABLE)

# Specifically request the construction of a static library.
# This library is not installed in the proto area.
LIBS = $(LIBRARY)

# inc must be first.
CPPFLAGS = -I../inc $(CPPFLAGS.master)

.KEEP_STATE:

all: $(LIBS)

install: all
	@echo "$(LIBRARY) is not installed."

# include library targets
include ../../../lib/Makefile.targ
