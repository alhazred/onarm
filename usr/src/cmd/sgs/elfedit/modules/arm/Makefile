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
# ident	"@(#)Makefile	1.1	07/09/18 SMI"
#

#
# Copyright (c) 2008 NEC Corporation
#

include		../Makefile.com

lint :=		CONV_LIB =	$(CONV_LIB32)
lint :=		LDDBG_LIB =	$(LDDBG_LIB32)

CFLAGS +=	$(C_PICFLAGS)

ZDEFS		=
MAPFILE		= mapfile
POST_PROCESS	= $(LOCALIZE_SYMBOL)

.KEEP_STATE:

all:		$(OBJDIR) $(PICDIR) .WAIT $(ELFEDITLIBS)

install \
package:	all .WAIT $(ROOTELFEDITDIR) .WAIT $(ROOTELFEDITLIBS)

include		../Makefile.targ
