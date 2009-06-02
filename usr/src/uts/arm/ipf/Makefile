#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# Copyright (c) 2006-2008 NEC Corporation
#

## This makefile drives the production of the ipf driver kernel module.
## ARM platform specific.

#
#	Path to the base of the uts directory tree (usually /usr/src/uts).
#
UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ipf
OBJECTS		= $(IPF_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(IPF_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(USR_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/inet/ipf

#
#	Include common rules.
#
include $(UTSBASE)/arm/Makefile.arm

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

MINOR=	    echo $(RELEASE) | cut -d. -f2
CPPFLAGS += -DIPFILTER_LKM -DIPFILTER_LOG -DIPFILTER_LOOKUP 
$(USE_INET6)CPPFLAGS	+= -DUSE_INET6
CPPFLAGS += -DSUNDDI -DSOLARIS2=$(MINOR:sh) -DIRE_ILL_CN


LDFLAGS_NEEDED			= -Ndrv/ip -Nmisc/md5 -Nmisc/neti
$(__GNULD)LDFLAGS_NEEDED	=
LDFLAGS				+= -dy $(LDFLAGS_NEEDED)

INC_PATH += -I$(UTSBASE)/common/inet/ipf

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
include $(UTSBASE)/arm/Makefile.targ
