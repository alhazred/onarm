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
# Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2006-2009 NEC Corporation
#

UTSBASE = ..

include	Makefile.ne1

#
#	The following are arm specific (rather than ne1) specific modules
#	which are required for the ne1 kernel to completely lint. They are
#	not involved in the build in any other way. In order to minimize
#	build time, it is assumed that they are up to date.
#
ARM_LIB_DIR	 = $(UTSBASE)/arm/lint-libs/$(OBJS_DIR)

ARM_LINTS	 = genunix

LINT_LIBS	 = $(LINT_LIB) \
		   $(KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(CLOSED_KMODS:%=$(LINT_LIB_DIR)/llib-l%.ln) \
		   $(ARM_LINTS:%=$(ARM_LIB_DIR)/llib-l%.ln)

#
#
#
def		:=	TARGET= def
all		:=	TARGET= all
install		:=	TARGET= install
clean		:=	TARGET= clean
clobber		:=	TARGET= clobber
lint		:=	TARGET= lint
lintlib		:=	TARGET= lintlib
machmodlintlib	:=	TARGET= modlintlib
modlist		:=	TARGET= modlist
modlist	modlist.arm :=	NO_STATE= -K $$MODSTATE$$$$
clean.lint	:=	TARGET= clean.lint
check		:=	TARGET= check

.KEEP_STATE:

.PARALLEL:	$(PARALLEL_KMODS) $(CLOSED_KMODS) $(XMODS) $(CLOSED_XMODS) \
		modlist modlist.arm

INITIAL_TARGETS	=		\
	genassym		\
	unix

EXTRA_CLEANUP_DIRS	= krtld

def all:	genassym unix .WAIT $(KMODS) $(CLOSED_KMODS) $(XMODS)	\
	$(CLOSED_XMODS)

clean clobber clean.lint:	genassym unix $(KMODS) $(CLOSED_KMODS)	\
	 $(XMODS) $(CLOSED_XMODS) $(EXTRA_CLEANUP_DIRS)

install:	install_platforms genassym unix .WAIT		\
	$(KMODS) $(CLOSED_KMODS) $(XMODS) $(CLOSED_XMODS)

# list the modules under ne1.
modlist: unix $(KMODS) $(CLOSED_KMODS) $(XMODS) $(CLOSED_XMODS)

# list the modules for Install -k ne1.
modlist.karch: modlist modlist.arm

modlist.arm:
	@cd $(SRC)/uts/arm; pwd; $(MAKE) $(NO_STATE) modlist

lintlib:	unix

modlintlib:	$(KMODS) $(CLOSED_KMODS)

genassym unix $(KMODS) $(EXTRA_CLEANUP_DIRS):	FRC
	@cd $@; pwd; $(MAKE) $(NO_STATE) $(TARGET)

$(XMODS):	FRC
	@if [ -f $@/Makefile  ]; then \
		cd $@; pwd; $(MAKE) $(NO_STATE) $(TARGET); \
	else \
		true; \
	fi

$(CLOSED_KMODS):	FRC
	cd $(CLOSED)/uts/ne1/$@; pwd; $(MAKE) $(NO_STATE) $(TARGET)

$(CLOSED_XMODS):	FRC
	@if [ -f $(CLOSED)/uts/ne1/$@/Makefile  ]; then \
		cd $(CLOSED)/uts/ne1/$@; pwd; \
		    $(MAKE) $(NO_STATE) $(TARGET); \
	else \
		true; \
	fi

install_h check:	FRC
	@cd $(UTSBASE)/armpf/sys; pwd;	\
	 ARM_PLATFORM=$(PLATFORM) $(MAKE) $(TARGET)
	@cd $(UTSBASE)/armpf/vm; pwd;	\
	 ARM_PLATFORM=$(PLATFORM) $(MAKE) $(TARGET)
	@cd sys; pwd; $(MAKE) $(TARGET)

OEM_LINKS	=

OEM_USR_PLAT_LINKS	= $(OEM_LINKS:%=$(USR_PLAT_DIR)/%)

$(OEM_USR_PLAT_LINKS):
	$(INS.slink1)

#
# Definitions for the /platform directory aliases.
# Currently none for ne1.
#
PLAT_LINKS	=

#
# Make the /platform directories.  This is hardwired here because
# the first stage of the project (KBI) only implements the userland
# changes, but the only reasonable place to record the aliases is
# here in kernel land.
#
install_platforms:	$(ROOT_PSM_DIR) $(USR_PSM_DIR) \
			$(ROOT_PLAT_LINKS) $(USR_PLAT_LINKS) \
			$(OEM_USR_PLAT_LINKS)

#
#	Full kernel lint target.
#
LINT_TARGET	= globallint

# workaround for multiply defined errors
globallint := LINTFLAGS += -erroff=E_NAME_MULTIPLY_DEF2

globallint:
	@-$(ECHO) "\nNE1 KERNEL: global crosschecks:"
	@-$(LINT) $(LINTFLAGS) $(LINT_LIBS) 2>&1 | $(LGREP.2)

lint:	lintlib .WAIT modlintlib .WAIT $(ARM_LINTS) $(LINT_DEPS)

$(ARM_LINTS):	FRC
	@cd $(UTSBASE)/arm/$@; pwd; $(MAKE) modlintlib

#
# Special rules to support tunable parameters.
#
$(USE_UTSTUNE)include	Makefile.tune

include ../Makefile.targ

#
# Cross-reference customization: build a cross-reference over all of the
# ne1-related directories.
#
SHARED_XRDIRS	= ../ne1 ../armpf ../arm ../common
XRDIRS		= $(SHARED_XRDIRS)
$(CLOSED_BUILD)XRDIRS	= $(SHARED_XRDIRS:../%=../% ../../../closed/uts/%)
XRPRUNE	= sun4u sun4 i86pc

cscope.out tags: FRC
	$(XREF) -x $@
