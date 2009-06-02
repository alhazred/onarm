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
# Copyright (c) 2006-2007 NEC Corporation
# All rights reserved.
#

#ident	"@(#)tools/elfdatamod/Makefile"

## Makefile for elfdatamod utility.

PROG		= elfdatamod
SRCS		= elfdatamod.c
OBJS		= $(SRCS:%.c=%.o)
LINTFILES	= $(SRCS:%.c=%.ln)

include ../Makefile.tools

CFLAGS	+= $(CCVERBOSE)
LDLIBS	+= -lelf

LINTFLAGS	+= $(LDLIBS)

ELFDATAMOD_POD	= elfdatamod.pod
MAN1FILES	= $(ELFDATAMOD_POD:.pod=.1)
CLOBBERFILES	+= $(MAN1FILES)

$(ROOTONBLDMAN1FILES)	:= FILEMODE = 644

.KEEP_STATE:

all:	$(PROG) $(MAN1FILES)

install: all .WAIT $(ROOTONBLDMACHPROG) $(ROOTONBLDMAN1FILES)

lint:	lint_SRCS

clean:
	$(RM) $(CLEANFILES)

elfdatamod.1:	$(ELFDATAMOD_POD)
	$(POD2MAN_BUILD)

include ../Makefile.targ
