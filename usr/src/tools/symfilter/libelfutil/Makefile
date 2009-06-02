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
# Copyright (c) 2009 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/symfilter/libelfutil/Makefile"

## Makefile for libelfutil library.

include	../../Makefile.tools

LIBELFUTILDIR	= .
ELFUTIL_LINK	= $(POUND_SIGN)

include	../Makefile.elfutil

SRCS	= $(ELFUTIL_SRCFILES)
OBJS	= $(SRCS:%.c=%.o)

LINTFLAGS	+= -lelf

CLEANFILES	+= $(OBJS)
CLOBBERFILES	= $(ELFUTIL_LIBRARY) $(ELFUTIL_LINTLIB)

.KEEP_STATE:

.PARALLEL:	$(OBJS)

all install:	$(ELFUTIL_LIBRARY)

$(ELFUTIL_LIBRARY):	$(OBJS)
	$(AR) cru $@ $(OBJS)

clean:
	$(RM) $(CLEANFILES)

lint:	$(ELFUTIL_LINTLIB)

include	../../Makefile.targ

$(ELFUTIL_LINTLIB):	$(SRCS)
	$(LINT) -y -o$(ELFUTIL_LIBNAME) $(LINTFLAGS) $(SRCS)
