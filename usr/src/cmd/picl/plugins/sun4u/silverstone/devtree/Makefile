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
#ident	"%Z%%M%	%I%	%E% SMI"
#
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# lib/picl/plugins/silverstone/devtree/Makefile
#
include	$(SRC)/Makefile.psm

# include library definitions
include $(SRC)/lib/Makefile.lib

include	$(SRC)/cmd/picl/plugins/Makefile.com

ROOT_PLATFORM = $(USR_PLAT_DIR)/SUNW,Sun-Fire-V890

ROOTLIBDIR	= $(ROOT_PLAT_PLUGINDIR)
ROOTLIBDIR	:= OWNER = root
ROOTLIBDIR	:= GROUP = sys

install		:= FILEMODE = 0444

CONF=           picldevtree.conf
ROOTCONF=       $(CONF:%=$(ROOTLIBDIR)/%)

ROOTLINTDIR =   $(ROOTLIBDIR)

.KEEP_STATE:

SUBDIRS=

all	:=	TARGET= all
install :=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

all: 

install:	$(ROOTLIBDIR) $(ROOTCONF)

# include library targets
include $(SRC)/lib/Makefile.targ
include	$(SRC)/cmd/picl/plugins/Makefile.targ

lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
