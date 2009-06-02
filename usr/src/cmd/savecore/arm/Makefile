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
# Copyright (c) 2007 NEC Corporation
# All rights reserved.
#

#ident	"@(#)cmd/savecore/arm/Makefile"

PROG	= savecore
XPROG	= xsavecore
SRCS	= ../savecore.c ../../../uts/common/os/compress.c
OBJS	= savecore.o compress.o
XOBJS	= savecore_x.o compress_x.o

XSTRUCT_H_IN	= xstruct.h.in
XCONST_H_IN	= xconst.h.in
XSTRUCT_H	= xstruct.h
XCONST_H	= xconst.h

XAUTOHEADER	= $(XSTRUCT_H) $(XCONST_H)

include	../../Makefile.cmd
include	../../Makefile.cmd.cross

XPREFIX		= __x_
XCROSSDEF	= XSAVECORE
XHEADER_ARGS	=  -x $(XCROSSDEF) -p $(XPREFIX)
XHEADER_COMPILER	+= -D_KERNEL $(KBUILD_CPPFLAGS)

LDLIBS		+= -lcmd
CFLAGS		+= $(CCVERBOSE)
CPPFLAGS	+= -D_LARGEFILE64_SOURCE=1

XCPPFLAGS	= -D$(XCROSSDEF) -I$(ROOT)/usr/include -I. $(CCOPTFLAG)
XLDLIBS		= -lcmd
CLOBBERFILES	+= $(XPROG) *~

XCC	= GNU_ROOT=$(NATIVE_GNU_ROOT) $($(NATIVE_MACH)_CC)

.KEEP_STATE:

all:	$(PROG) $(XPROG)

$(PROG):	$(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(XPROG):	$(XOBJS)
	$(XCC) -o $@ $(XOBJS) $(XLDLIBS)
	$(POST_PROCESS)

clean:
	$(RM) $(OBJS) $(XOBJS) $(XAUTOHEADER)

lint:	lint_SRCS

include	../../Makefile.targ

%.o:	%.c $(XAUTOHEADER)
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

%.o:	../../../uts/common/os/%.c $(XAUTOHEADER)
	$(COMPILE.c) $<
	$(POST_PROCESS_O)

%_x.o:	%.c $(XAUTOHEADER)
	$(XCC) -c -o $@ $(XCPPFLAGS) $<
	$(POST_PROCESS_O)

%_x.o:	../../../uts/common/os/%.c $(XAUTOHEADER)
	$(XCC) -c -o $@ $(XCPPFLAGS) $<
	$(POST_PROCESS_O)

install: all $(ROOTPROG) $(CROSSROOTPROG)

$(XSTRUCT_H):	$(XSTRUCT_H_IN)
	$(XHEADER_CREATE) $(XHEADER_ARGS) -o $@ $(XSTRUCT_H_IN)	\
		$(XHEADER_COMPILER)

$(XCONST_H):	$(XCONST_H_IN)
	$(CONSTANTS_CREATE) -p $(XPREFIX) -o $@ $(XCONST_H_IN)	\
		$(GENCONST_COMPILER)
