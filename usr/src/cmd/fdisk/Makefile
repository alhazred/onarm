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
#
# Copyright (c) 2007-2008 NEC Corporation
#
# ident	"@(#)Makefile	1.11	05/06/08 SMI"
#

#       Makefile for fdisk

PROG= fdisk
ROOTFS_PROG= $(PROG)

$(ARM_BLD)MBOOT= arm/mboot
$(ARM_BLD)MBOOTDIR = $(ROOTLIB)/fs/ufs
$(ARM_BLD)MBOOTMODE = 0444

ROOTSBINLINK=	$(ROOTUSRSBIN)/$(PROG)

include ../Makefile.cmd

LIBEFI =	-lefi
$(ARM_BLD)LIBEFI =
LDLIBS +=	-ladm $(LIBEFI)
$(ARM_BLD)CFLAGS +=	-DNO_SUPPORT_EFI

all:	$(ROOTFS_PROG)

install: all $(ROOTSBINPROG) $(ROOTSBINLINK) $(ARM_BLD)installmboot

$(ARM_BLD)installmboot: $(MBOOTDIR) $(MBOOT)
	$(ARM_BLD) -$(RM) $(MBOOTDIR)/mboot
	$(ARM_BLD) $(INS) -s -m $(MBOOTMODE) -f $(MBOOTDIR) $(MBOOT)

$(ARM_BLD)$(MBOOTDIR):
	$(ARM_BLD)$(INS.dir)

$(ROOTSBINLINK):	FRC
	-$(RM) $@; $(SYMLINK) ../../sbin/$(PROG) $@

FRC:

clean:

lint:	lint_PROG

include ../Makefile.targ
