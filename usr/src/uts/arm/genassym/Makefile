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
# Copyright (c) 2008-2009 NEC Corporation
#

UTSBASE	= ../..

ASSYM_H		= $(GENASSYM_DIR)/$(OBJS_DIR)/assym.h
OFFSET_H	= $(GENASSYM_DIR)/$(OBJS_DIR)/offset.h.in
CONST_H		= $(GENASSYM_DIR)/$(OBJS_DIR)/constants.h.in

#
#	Include common rules.
#
include $(UTSBASE)/arm/Makefile.arm

#
#	Define targets
#
ALL_TARGET	= $(ASSYM_H)

#
# This is GENASSYM_DIR.  Use a short path.
#
GENASSYM_DIR		= .

#
#	Overrides
#
CLEANFILES	= Nothing_to_remove
CLOBBERFILES	= $(ASSYM_H) $(OFFSET_H) $(CONST_H)

USE_CPPFLAGS_FILE	= $(POUND_SIGN)
USE_MODTUNE		= $(POUND_SIGN)
ASSYM_AUTOGEN		= $(POUND_SIGN)

#
#	Default build targets.
#
.KEEP_STATE:

def:	$(DEF_DEPS)

all:	$(ALL_DEPS)

clean:	$(CLEAN_DEPS)
	$(RM)

clobber:	$(CLOBBER_DEPS)

clean.lint:

install:	def

#
# Create assym.h
#
ASSYM_H_DEPS	= $(ARCH_MODTUNE_H) $(OFFSET_H) $(CONST_H)

$(OFFSET_H):	$(OFFSETS_IN)
	$(OFFSETS_CREATE) < $(OFFSETS_IN) > $@ || ($(RM) $@; exit 1)

$(CONST_H):	$(CONSTANTS_IN)
	$(CONSTANTS_CREATE) -o $@ $(CONSTANTS_IN) $(GENCONST_COMPILER)

$(ASSYM_H):	$(ASSYM_H_DEPS)
	$(CAT) $(OFFSET_H) $(CONST_H) > $@ || ($(RM) $@; exit 1)

#
#	Include common targets.
#
include $(UTSBASE)/arm/Makefile.targ
