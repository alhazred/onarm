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

## This makefile drives the production of the ip driver kernel module.
## ARM platform specific.

UTSBASE	= ../..

#
#	Define the module and object file sets.
#
MODULE		= ip
OBJECTS		= $(IP_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(IP_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
ROOTLINK	= $(ROOT_STRMOD_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/inet/ip

#
#	Include common rules.
#
include $(UTSBASE)/arm/Makefile.arm

#       Use makeopt for dynamic configuration.
USE_MAKEOPT	=

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOTLINK) $(ROOT_CONFFILE)

CINLINEFLAGS = -xinline=tcp_set_ws_value

CFLAGS += $(CINLINEFLAGS)

#
# Depends on md5 and swrand (for SCTP). SCTP needs to depend on
# swrand as it needs random numbers early on during boot before
# kCF subsystem can load swrand.
#
LDFLAGS_NEEDED			= -Nmisc/md5 -Ncrypto/swrand
LDFLAGS_NEEDED			+= -Nmisc/hook -Nmisc/neti
$(__GNULD)LDFLAGS_NEEDED	=
LDFLAGS				+= -dy $(LDFLAGS_NEEDED)

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

$(ROOTLINK):	$(ROOT_STRMOD_DIR) $(ROOTMODULE)
	$(INS_ROOTLINK)

#
#	Include common targets.
#
include $(UTSBASE)/arm/Makefile.targ

#
# The ip CTF data is merged into the genunix module because these types are
# complex and heavily shared.  The genunix build will execute one of the
# rules below to create an archive, ipctf.a, containing the ip objects.  The
# real ip will be uniquified against genunix later in the build, and will
# emerge containing very few types.
#
$(OBJS_DIR)/ipctf.a: $(OBJECTS)
	-$(RM) $@
	$(AR) -r $@ $(OBJECTS)

$(OBJECTS): $(OBJS_DIR)

CLOBBERFILES += $(OBJS_DIR)/ipctf.a

ipctf.obj32: FRC
	@BUILD_TYPE=OBJ32 VERSION='$(VERSION)' $(MAKE) obj32/ipctf.a

ipctf.debug32: FRC
	@BUILD_TYPE=DBG32 VERSION='$(VERSION)' $(MAKE) debug32/ipctf.a
