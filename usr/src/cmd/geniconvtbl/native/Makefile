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
#

#
# Copyright (c) 2007-2008 NEC Corporation
#

.KEEP_STATE:

NOT_NATIVE = $(POUND_SIGN)              # tell Makefile.com we're native

include ../Makefile.com

CC =		$(NATIVECC)
LD =		$(NATIVELD)
CFLAGS =	$(NATIVE_CFLAGS)
CPPFLAGS =	$(MY_NATIVE_CPPFLAGS)
LDFLAGS =	$(MY_NATIVE_LDFLAGS)
LDLIBS =	$(MY_NATIVE_LDLIBS)
POST_PROCESS =

$(ARM_BLD)GNUC_SYSROOT=
$(ARM_BLD)CC	= GNU_ROOT=$(SFW_ROOT) $(arm_CC)

all install:	$(PROG)

catalog:
