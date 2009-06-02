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
# Copyright (c) 2007-2009 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/symfilter/Makefile.com"

## Makefile for symfilter utility.

PROG		= symfilter
COMMON_SRCFILES	= symfilter.c rule.c symtab.c zio.c ctfutil.c inthash.c
ARCH_SRCFILES	= reloc.c
SRCFILES	= $(COMMON_SRCFILES) $(ARCH_SRCFILES)
OBJS		= $(SRCFILES:%.c=%.o)

COMMON_SRCDIR	= ../common
ARCH_SRCDIR	= .
LIBELFUTILDIR	= ../libelfutil

SRCS		= $(COMMON_SRCFILES:%=$(COMMON_SRCDIR)/%)
SRCS		+= $(ARCH_SRCFILES:%=$(ARCH_SRCDIR)/%)

ELFDATAMODDIR	= ../../elfdatamod

include ../../Makefile.tools

CPPFLAGS	+= -I$(COMMON_SRCDIR) -I$(ELFDATAMODDIR)
CFLAGS		+= $(CCVERBOSE)
CFLAGS		+= -g
LDLIBS		+= -lelf -lz -lc

LINTFLAGS	+= $(LDLIBS)

CLEANFILES	+= $(OBJS)

.KEEP_STATE:
.SUFFIXES:

.PARALLEL:	$(OBJS)

all:	$(PROG)

include ../Makefile.elfutil

install: all .WAIT $(ROOTONBLDMACHPROG)

clean:
	$(RM) $(CLEANFILES)

$(PROG):	$(ELFUTIL_LIBFILE) $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

%.o:	$(ARCH_SRCDIR)/%.c
	$(COMPILE.c) $<

%.o:	$(COMMON_SRCDIR)/%.c
	$(COMPILE.c) $<

lint:	$(ELFUTIL_LINTLIBFILE) lint_SRCS

include ../../Makefile.targ

FRC:
