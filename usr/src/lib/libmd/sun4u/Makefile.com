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
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# ident	"%Z%%M%	%I%	%E% SMI"

LIBRARY= libmd_psr.a

include $(SRC)/Makefile.psm
include ../Makefile.links
include ../../Makefile.com

LIBS= $(DYNLIB)

CFLAGS += -xarch=v8plusa
CPPFLAGS += -D$(PLATFORM) -DVIS_SHA1
ASFLAGS	= -P $(ASDEFS)

INLINES= $(COMDIR)/md5/$(MACH)/$(PLATFORM)/byteswap.il

# XXX This seems wrong since we explicitly set LIBS to be DYNLIB only
$(LINTLIB):= SRCS= ../../common/llib-lmd
