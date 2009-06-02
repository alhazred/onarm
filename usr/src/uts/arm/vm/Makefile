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
# Copyright (c) 2006 NEC Corporation
#

#ident	"@(#)uts/arm/vm/Makefile"

UTSBASE = ../..

#
# include global definitions
#
include ../Makefile.arm

#
# Override defaults.
#
FILEMODE = 644
GROUP = bin

HDRS		= hat_arm.h hat_armpt.h hment.h

ROOTDIR		= $(ROOT)/usr/include/vm
ROOTDIRS	= $(ROOTDIR)
ROOTHDRS	= $(HDRS:%=$(ROOTDIR)/%)
$(CLOSED_BUILD)ROOTHDRS	+= $(CLOSEDHDRS:%=$(ROOTDIR)/%)

CHECKHDRS	= $(HDRS:%.h=%.check)
$(CLOSED_BUILD)CHECKHDRS += $(CLOSEDHDRS:%.h=$(CLOSED)/uts/arm/vm/%.check)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS) $(ROOTHDRS)

install_h: $(ROOTDIRS) .WAIT $(ROOTHDRS)

check:	$(CHECKHDRS)

#
# install rules
#
$(ROOTDIR)/%:	%
	$(INS.file)

$(ROOTDIRS)/%:	$(CLOSED)/uts/arm/vm/%
	$(INS.file)

$(ROOTDIRS):
	$(INS.dir)

include ../../Makefile.targ
