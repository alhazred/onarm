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
# Copyright 2003 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# cmd/picl/plugins/sun4u/ents/frutree/Makefile
#

include	$(SRC)/Makefile.psm

# include library definitions
include $(SRC)/lib/Makefile.lib

ROOT_PLATFORM = $(USR_PLAT_DIR)/SUNW,Sun-Fire-V250

include	$(SRC)/cmd/picl/plugins/Makefile.com

ROOTLIBDIR	= $(ROOT_PLAT_PLUGINDIR)
ROOTLIBDIR	:= OWNER = root
ROOTLIBDIR	:= GROUP = sys

INCDIR1		= .
INCDIR2		= ../frudr
INCS		= -I$(INCDIR1) -I$(INCDIR2)
CONFS		= piclfrutree.conf
ROOTCONF	= $(CONFS:%=$(ROOTLIBDIR)/%)
$(ROOTCONF)	:= FILEMODE = 0644
MAININFO	= $(CONFS:%.conf=%.info)
OTHERINFOS	= $(INCDIR1)/system-board.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD0_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD1_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD2_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD3_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD4_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD5_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD6_data.info
OTHERINFOS	+= $(INCDIR2)/fru_HDD7_data.info
OTHERINFOS	+= $(INCDIR2)/fru_PS0_data.info
OTHERINFOS	+= $(INCDIR2)/fru_PS1_data.info
OTHERINFOS	+= $(INCDIR2)/fru_SC_data.info
OTHERINFOS	+= $(INCDIR2)/fru_usb-a-1_data.info
OTHERINFOS	+= $(INCDIR2)/fru_usb-a-2_data.info
OTHERINFOS	+= $(INCDIR2)/fru_usb-b-1_data.info
OTHERINFOS	+= $(INCDIR2)/fru_usb-b-2_data.info

ROOTLINTDIR =   $(ROOTLIBDIR)

.KEEP_STATE:

SUBDIRS=

all	:=	TARGET= all
install :=	TARGET= install
clean	:=	TARGET= clean
clobber	:=	TARGET= clobber
lint	:=	TARGET= lint

all:	$(CONFS) 

install:	$(ROOTLIBDIR) $(ROOTCONF)

$(CONFS): $(MAININFO) $(OTHERINFOS)
	@$(RM) -f $@
	@echo "# Copyright 2003 Sun Microsystems, Inc. " \
	"All rights reserved." > $@
	@echo "# Use is subject to license terms." >> $@
	@echo "# This is a generated file. #ident lines identify sources" >> $@
	$(CPP) -P $(INCS) $(@:%.conf=%.info) | \
	$(SED) -e '/^[ 	]*$$/d' >> $@
	@$(CHMOD) 444 $@

# include library targets
include $(SRC)/lib/Makefile.targ
include	$(SRC)/cmd/picl/plugins/Makefile.targ

$(ROOTLINTDIR)/%: ../%
	$(INS.file)

lint:

$(SUBDIRS): FRC
	@cd $@; pwd; $(MAKE) $(TARGET)

FRC:
