#
# Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2006-2009 NEC Corporation
#

## This makefile drives the production of the zmod kernel module.
## ARM platform specific.


UTSBASE	= ../..

MODULE		= zmod
OBJECTS		= $(ZMOD_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(ZMOD_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_MISC_DIR)/$(MODULE)

include $(UTSBASE)/arm/Makefile.arm

ALL_TARGET	= $(BINARY)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE)

MAPFILE				= $(UTSBASE)/common/zmod/mapfile
LDFLAGS_MAPFILE			= -B reduce -M $(MAPFILE)
$(__GNULD)LDFLAGS_MAPFILE	=
LDFLAGS				+= $(LDFLAGS_MAPFILE)

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
