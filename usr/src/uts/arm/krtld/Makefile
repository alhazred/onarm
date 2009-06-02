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
# Copyright (c) 2006-2007 NEC Corporation
#

## This makefile drives the production of the kernel linker/loader module.
## ARM architecture dependent

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= krtld
OBJECTS		= $(KRTLD_BOOT_OBJS:%=$(OBJS_DIR)/%) \
		  $(KRTLD_OBJS:%=$(OBJS_DIR)/%)

LINTS		= $(KRTLD_BOOT_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(KRTLD_OBJS:%.o=$(LINTS_DIR)/%.ln)
PICS		= $(KRTLD_BOOT_OBJS:%=$(OBJS_DIR)/%)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

INC_PATH	= -I$(UTSBASE)/common/krtld -I. -I$(COMMONBASE)

MODSUFFIX	=

#
#	Include common rules.
#
include $(UTSBASE)/arm/Makefile.arm

# If STATIC_UNIX is defined, krtld is statically linked to unix.
# So we don't need to build krtld here.

$(STATIC_UNIX)NO_KRTLD_MODULE	= $(POUND_SIGN)
$(STATIC_UNIX)OBJECTS		=
$(STATIC_UNIX)USE_MODINFO_OBJ	= $(POUND_SIGN)

#
#	Define targets
#
ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

#
#	Overrides.
#
CFLAGS		+= -Wu,-no_got_reloc
CPPFLAGS	+= -DELF_TARGET_ARM -D_KRTLD $(MODSUFFIX)
MAPFILE		= $(UTSBASE)/common/krtld/mapfile
LDFLAGS		= -e _kobj_boot -M$(MAPFILE) -Breduce -znoversion -zverbose
$(__GNULD)LDFLAGS	= -init _kobj_boot

$(__GNULD)LOCALIZE_SYMBOL_OPT	= -c $(STATIC_CONF)
$(__GNULD)$(BINARY)	:= LOCAL_POST_PROCESS = $(LOCALIZE_SYMBOL)

# krtld uses a mapfile during the link to hide some symbols, thus changing
# their scope from global to local.  This will fool ctfmerge (it won't record
# the changed symbols) unless we tell it to use fuzzy matching.
CTFMRGFLAGS	= -f

$(PICS)		:= CFLAGS += -K pic -_gcc=-fno-jump-tables	\
		  -_gcc=-fno-constant-pools

#
# krtld has its own private copies of these functions and objects
#
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcmp=$(MODULE)_strcmp
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrncmp=$(MODULE)_strncmp
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrlen=$(MODULE)_strlen
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcpy=$(MODULE)_strcpy
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrncpy=$(MODULE)_strncpy
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrcat=$(MODULE)_strcat
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrlcat=$(MODULE)_strlcat
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dstrchr=$(MODULE)_strchr
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dbzero=$(MODULE)_bzero
$(OBJECTS) $(LINTS)	:= CPPFLAGS += -Dbcopy=$(MODULE)_bcopy

#
#	Default build targets.
#
$(NO_KRTLD_MODULE).KEEP_STATE:

$(NO_KRTLD_MODULE)def:		$(DEF_DEPS)

$(NO_KRTLD_MODULE)all:		$(ALL_DEPS)

$(NO_KRTLD_MODULE)clean:		$(CLEAN_DEPS)

$(NO_KRTLD_MODULE)clobber:	$(CLOBBER_DEPS)

$(NO_KRTLD_MODULE)lint:		$(LINT_DEPS)

$(NO_KRTLD_MODULE)modlintlib:	$(MODLINTLIB_DEPS)

$(NO_KRTLD_MODULE)clean.lint:	$(CLEAN_LINT_DEPS)

$(NO_KRTLD_MODULE)install:	$(INSTALL_DEPS)

$(NO_KRTLD_MODULE)$(BINARY):	$(MAPFILE)

$(STATIC_UNIX)def:
$(STATIC_UNIX)all:
$(STATIC_UNIX)clean:
$(STATIC_UNIX)clobber:
$(STATIC_UNIX)lint:
$(STATIC_UNIX)modlintlib:
$(STATIC_UNIX)clean.lint:
$(STATIC_UNIX)install:
$(STATIC_UNIX)$(BINARY):

#
# krtld has variant targets, depending on what the kernel instruction
# set actually is
#
include	$(UTSBASE)/arm/Makefile.rules

#
#	Include common targets.
#
include $(UTSBASE)/arm/Makefile.targ
