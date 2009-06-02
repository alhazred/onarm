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
# Copyright (c) 2007-2008 NEC Corporation
# All rights reserved.
#

## This makefile drives the production of header file that provides
## i86pc-specific tunable parameters.

UTSBASE		= ../..

#
# Include common rules.
#
include $(UTSBASE)/i86pc/Makefile.i86pc

#
# Define targets
#
ALL_TARGET	= $(MODTUNE_H)

#
#	Overrides
#
CLEANFILES		= Nothing_to_remove
CLOBBERFILES		= $(MODTUNE_DUMMY) Nothing_to_remove
USE_CPPFLAGS_FILE	= $(POUND_SIGN)
OBJECTS			= Nothing_to_build

#
#	Default build targets.
#
.KEEP_STATE:

def:	$(DEF_DEPS)

all:	$(ALL_DEPS)

clean:	$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

clean.lint:

install:	def

#
# Include common targets.
#
include $(UTSBASE)/i86pc/Makefile.targ
