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

#
# Copyright (c) 2006-2009 NEC Corporation
#

## This makefile drives the production of the kernel linker/loader to be
## static-linked to vmunix. Note that this build environment produces
## relocatable object, not loadable module.
##
## ARM architecture dependent

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= krtld
OBJECTS		= $(KRTLD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(KRTLD_OBJS:%.o=$(LINTS_DIR)/%.ln)

INC_PATH	= -I$(UTSBASE)/common/krtld -I$(UTSBASE)/arm/krtld \
		  -I$(COMMONBASE)

MODSUFFIX	=

#
#	Include common rules.
#
include $(UTSBASE)/ne1/Makefile.ne1

#
#	Overrides.
#
MAPFILE			= $(UTSBASE)/ne1/krtld/mapfile
CPPFLAGS		+= -DELF_TARGET_ARM -D_KRTLD $(MODSUFFIX)
USE_CPPFLAGS_FILE	= $(POUND_SIGN)
USE_MODINFO_OBJ		= $(POUND_SIGN)
MOD_SYMFILTER		=

$(__GNULD)LOCALIZE_SYMBOL_OPT	= -c $(STATIC_CONF)
$(__GNULD)$(BINARY)	:= LOCAL_POST_PROCESS = $(LOCALIZE_SYMBOL)

# Override object file to be linked.
KRTLD_OBJS	=		\
	doreloc.o		\
	kobj.o			\
	kobj_convrelstr.o	\
	kobj_crt.o		\
	kobj_isa.o		\
	kobj_kdi.o		\
	kobj_lm.o		\
	kobj_reloc.o		\
	kobj_subr.o

# Disable ctfmerge.
CTFMERGE_UNIQUIFY_AGAINST_GENUNIX	=

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY)		# Do NOT install any file.

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)

clobber:	$(CLOBBER_DEPS)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/ne1/Makefile.targ

$(BINARY):	$(MAPFILE)
