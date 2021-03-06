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
# Copyright (c) 2006-2008 NEC Corporation
#

## This makefile drives the production of the ctf kernel module.
## ARM platform specific.

UTSBASE	= ../..

MODULE		= ctf
OBJECTS		= $(CTF_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(CTF_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/arm/Makefile.arm

ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

CPPFLAGS	+= -I$(SRC)/common/ctf -DCTF_OLD_VERSIONS
MAPFILE		= $(UTSBASE)/common/ctf/mapfile
LDFLAGS_CTF	= -Breduce -M$(MAPFILE)
$(__GNULD)LDFLAGS_CTF		=
LDFLAGS				+= -dy $(LDFLAGS_CTF)

$(__GNULD)LOCALIZE_SYMBOL_OPT	= -c $(STATIC_CONF)
$(__GNULD)$(BINARY)	:= LOCAL_POST_PROCESS = $(LOCALIZE_SYMBOL)

.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

include $(UTSBASE)/arm/Makefile.targ
