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
# Copyright 1993,2001-2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2009 NEC Corporation
#
# ident	"@(#)Makefile	1.4	05/06/08 SMI"
#
# lib/nsswitch/dns/arm/Makefile

include		../Makefile.com

LIBS =		$(DYNLIB1)

# ARM compiler does not assign PLT for weak function call.
# So we must link libresolv.so explicitly because jump to weak symbol,
# such as res_gethostbyname() call, never calls runtime linker.
LDLIBS		+= -lresolv
CPPFLAGS	+= -DLIBRESOLV_NEEDED

VERS_SCRIPT	= vers-script

include		../../Makefile.targ

install:	all $(ROOTLIBS)
