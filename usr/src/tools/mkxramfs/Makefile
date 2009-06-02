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
# ident	"@(#)Makefile	1.22	06/04/23 SMI"
#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

PROG=		mkxramfs
OBJS=		$(PROG).o utils.o
SRCS=		$(OBJS:%.o=%.c)
CLEANFILES +=	$(OBJS)

include		../Makefile.tools

OWNER=  root
GROUP=  bin
CFLAGS += -I../../uts/common $(CCVERBOSE)
CPPFLAGS += -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64

.KEEP_STATE:

all:    $(PROG)

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

install: all .WAIT $(ROOTONBLDPROG)

lint:		lint_SRCS

clean:
	$(RM) $(CLEANFILES)

include ../Makefile.targ
