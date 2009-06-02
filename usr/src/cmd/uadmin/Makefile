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
# Copyright (c) 2008 NEC Corporation
#
# ident	"@(#)Makefile	1.19	07/10/24 SMI"
#

PROG= uadmin
ROOTFS_PROG= $(PROG)

include ../Makefile.cmd

LDLIBS2			= -lbsm -lscf
LINT_PROG		= lint_PROG
$(ARM_BLD)LDLIBS2	=
$(ARM_BLD)LINT_PROG	=

OWNER = root
GROUP = sys
LDLIBS += $(LDLIBS2)
LINTFLAGS = -ux

.KEEP_STATE:

$(ARM_BLD)$(ROOTFS_PROG): $(PROG)_$(MACH).c
$(ARM_BLD)	$(LINK.c) -o $@ $(PROG)_$(MACH).c $(LDLIBS)
$(ARM_BLD)	$(POST_PROCESS)

all: $(ROOTFS_PROG)

install: all $(ROOTSBINPROG)
	-$(RM) $(ROOTETCPROG)
	-$(SYMLINK) ../sbin/$(PROG) $(ROOTETCPROG)
	-$(RM) $(ROOTUSRSBINPROG)
	-$(SYMLINK) ../../sbin/$(PROG) $(ROOTUSRSBINPROG)

clean:

lint:	$(LINT_PROG)
$(ARM_BLD)	$(LINT.c) $(PROG)_$(MACH).c $(LDLIBS)

include ../Makefile.targ
