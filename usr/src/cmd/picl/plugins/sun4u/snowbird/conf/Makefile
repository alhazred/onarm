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

#pragma ident	"%Z%%M%	%I%	%E% SMI"

include	$(SRC)/Makefile.psm

# include library definitions
include $(SRC)/lib/Makefile.lib

PLATFORM = SUNW,Netra-CP2300
ROOT_PLATFORM = $(USR_PLAT_DIR)/$(PLATFORM)

include	$(SRC)/cmd/picl/plugins/Makefile.com

ROOTLIBDIR = $(ROOT_PLAT_PLUGINDIR)
ROOTLIBDIR	:= OWNER = root
ROOTLIBDIR	:= GROUP = sys

CONF_2300 = SUNW,Netra-CP2300.conf SUNW,Netra-CP2300.RTM.conf envmond.conf

ROOTCONF_2300 =	$(CONF_2300:%=$(ROOTLIBDIR)/%)
$(ROOTCONF_2300)	:= FILEMODE = 0644
ROOTLINTDIR_2300 =	$(ROOTLIBDIR)

.KEEP_STATE:

SUBDIRS=

all	:=	TARGET= all
install :=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

all:

install:	$(ROOTLIBDIR) $(ROOTCONF_2300)

$(CONF_2300): FRC
	$(RM) $(ROOTLIBPLUGINDIR)/$@
	$(INS) -s -m 0644 -f $(ROOT_PLAT_PLUGINDIR) $@

# include library targets
include $(SRC)/lib/Makefile.targ
include	$(SRC)/cmd/picl/plugins/Makefile.targ

$(ROOTLINTDIR_2300)/%: ../%
	$(INS.file)

lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:

