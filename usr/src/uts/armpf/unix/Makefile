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
# Copyright (c) 2006-2009 NEC Corporation
#

# Makefile for ARMPF specific kernel.

UTSBASE	= ../..

#
#	Define the module and object file sets.
#
UNIX		= unix
VMUNIX		= vmunix

OBJECTS		= $(SPECIAL_OBJS:%=$(OBJS_DIR)/%) \
		  $(CORE_OBJS:%=$(OBJS_DIR)/%) \
		  $(CORE_OBJS_EX:%=$(OBJS_DIR)/%) \
		  $(MACH_NOT_YET_KMODS:%=$(OBJS_DIR)/%)

LINTS		= $(SPECIAL_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CORE_OBJS:%.o=$(LINTS_DIR)/%.ln) \
		  $(CORE_OBJS_EX.o:%=$(LINTS_DIR)/%.ln) \
		  $(MACH_NOT_YET_KMODS:%.o=$(LINTS_DIR)/%.ln) \
		  $(LINTS_DIR)/vers.ln \
		  $(LINTS_DIR)/modstubs.ln

ROOTMODULE	= $(ROOT_PSM_KERN_DIR)/$(UNIX)
$(STATIC_UNIX)ROOTMODULE	= $(ROOT_PSM_KERN_DIR)/$(VMUNIX)

UNIX_BIN	= $(OBJS_DIR)/$(UNIX)
VMUNIX_BIN	= $(OBJS_DIR)/$(VMUNIX)
VMUNIX_O	= $(OBJS_DIR)/$(VMUNIX).o
VMUNIX_REL	= $(VMUNIX_BIN).rel
VMUNIX_SYM	= $(VMUNIX_BIN).sym

# U-boot image
UIMAGE		= $(OBJS_DIR)/uImage
KIMAGE		= $(OBJS_DIR)/kImage

KRTLD		= misc/krtld

LIBS		= $(GENLIB)

GENUNIX		= genunix
GENUNIX_DIR	= ../../arm/$(GENUNIX)

LIBOPTS		= -L $(GENUNIX_DIR)/$(OBJS_DIR) -l $(GENUNIX)

CTFEXTRAOBJS	= $(OBJS_DIR)/vers.o

#
#	Include common rules.
#
include $(UTSBASE)/$(PLATFORM)/Makefile.$(PLATFORM)

# Use makeopt for dynamic configuration.
USE_MAKEOPT     =

#
#	Define targets
#
ALL_TARGET				= $(UNIX_BIN)
INSTALL_TARGET				= $(UNIX_BIN) $(ROOTMODULE)
$(STATIC_UNIX)ALL_TARGET		= $(UIMAGE)
$(STATIC_UNIX)INSTALL_TARGET		= $(UIMAGE)

LINT_TARGET	= $(LINT_LIB)

#
#	This is UNIX_DIR. Use a short path.
#
UNIX_DIR	= .

#
#	Overrides
#
CLEANFILES	+= $(UNIX_O) $(MODSTUBS_O) $(OBJS_DIR)/vers.c \
		   $(OBJS_DIR)/vers.o $(ZLIB_OBJS:%=$(OBJS_DIR)/%) \
		   $(DTRACESTUBS_O) $(DTRACESTUBS)

CLOBBERFILES	= $(CLEANFILES) $(UNIX_BIN)
CLEANLINTFILES	+= $(LINT_LIB)

USE_MODINFO_OBJ		= $(POUND_SIGN)

$(STATIC_UNIX)CLEANFILES	+= $(VMUNIX_O)

ARCH_STATIC_CONF	= $(UTSBASE)/arm/static-$(PLATFORM).conf
ARCH_STATIC_CONF_STAMP	= $(UTSBASE)/arm/.static_$(PLATFORM)_$(OBJS_DIR)_stamp
STATIC_CONF_FLAGS	= -c $(STATIC_CONF) -c $(ARCH_STATIC_CONF)
STATIC_CPPFLAGS_COMMAND	= $(STATICTOOL) modstubs_cppflags $(STATIC_CONF_FLAGS)
STATIC_CPPFLAGS_DEPEND	= $(STATIC_CONF) $(ARCH_STATIC_CONF) $(OBJS_DIR) \
			  $(STATIC_CONF_STAMP) $(ARCH_STATIC_CONF_STAMP)

KERNADDR	= $(DSF_DIR)/$(OBJS_DIR)/kernaddr

# KERNELBASE, KERNELPHYSBASE and KERNELTEXTBASE are derived from kernaddr file.
# They are required to link kernel and create kernel image file.
KERNELBASE_COMMAND	=	\
	$(NAWK) '/^\#define[ \t]+KERNELBASE[ \t]+/ {print $$3}' $(KERNADDR)
KERNELBASE		= $(KERNELBASE_COMMAND:sh)
KERNELPHYSBASE_COMMAND	=	\
	$(NAWK) '/^\#define[ \t]+KERNELPHYSBASE[ \t]+/ {print $$3}' $(KERNADDR)
KERNELPHYSBASE		= $(KERNELPHYSBASE_COMMAND:sh)
KERNELTEXTBASE_COMMAND	=	\
	$(NAWK) '/^\#define[ \t]+KERNELTEXTBASE[ \t]+/ {print $$3}' $(KERNADDR)
KERNELTEXTBASE		= $(KERNELTEXTBASE_COMMAND:sh)

TNFPROBE_NEXT_OFFSET_COMMAND	=	\
	$(NAWK) '/^\#define[ \t]+TNFPROBE_NEXT_OFFSET[ \t]+/ {print $$3}' \
		$(KERNADDR)
TNFPROBE_NEXT_OFFSET	= $(TNFPROBE_NEXT_OFFSET_COMMAND:sh)

# Write CTF address range into kernel executable.
CTF_SECTION			= .SUNW_ctf
CTF_START_ADDRESS		= ctf_start_address
CTF_END_ADDRESS			= ctf_end_address
SET_CTF_ADDRESS_RANGE_CMD	=	\
	$(ELFDATAMOD) -B $(KERNELPHYSBASE) -S -q \
		-m '^$(CTF_SECTION):&$(CTF_START_ADDRESS)' \
		-m '$$$(CTF_SECTION):&$(CTF_END_ADDRESS)' $@
SET_CTF_ADDRESS_RANGE		=	\
	echo $(SET_CTF_ADDRESS_RANGE_CMD); $(SET_CTF_ADDRESS_RANGE_CMD)
$(NO_UNIX_CTF)SET_CTF_ADDRESS_RANGE	= :

XRAMDEV_RELOCATE_CMD	= $(XRAMCONF) -R -r -e $(ELFDATAMOD) $@
XRAMDEV_RELOCATE	= echo $(XRAMDEV_RELOCATE_CMD); $(XRAMDEV_RELOCATE_CMD)

VMUNIX_POST_PROCESS	= $(SET_CTF_ADDRESS_RANGE); $(XRAMDEV_RELOCATE)

$(VMUNIX_BIN)	:=	LOCAL_POST_PROCESS = $(VMUNIX_POST_PROCESS)

# Include rules to create uImage.
UIMAGE_KERNEL		= $(VMUNIX_BIN)
$(STATIC_UNIX)include $(UTSBASE)/Makefile.uts.uimage

# Use GNU ld script to set kernel text/data addresses.
GNULD_SCRIPT_SRC	= $(UTSBASE)/armpf/conf/unix.lds
GNULD_SCRIPT		= $(OBJS_DIR)/unix.lds
$(__GNULD)MAPFILE	= $(GNULD_SCRIPT)
$(__GNULD)CLOBBERFILES	+= $(GNULD_SCRIPT)

# Define filename that contains symbol filtering rules
SYMFILTER_RULE	= $(UTSBASE)/armpf/conf/unix_symfilter.rules

# Linker flags to build unix.
UNIX_LDFLAGS	= -b -e _start -I $(KRTLD) -M $(MAPFILE)

# -q is required to leave sections for relocation in executable.
# They are used to setup tnf_probe_control list.
$(__GNULD)UNIX_LDFLAGS	= -q -init _start -T $(MAPFILE)

# unix dependency
UNIX_BIN_DEP	= $(UNIX_O) $(MODSTUBS_O) $(GENLIB) $(DTRACESTUBS) $(MAPFILE)

# use cyclecount_log system call.
$(CYCLECOUNT)$(OBJS_DIR)/sysarm.o := CPPFLAGS += -DUSE_CYCLECOUNT

# Default target for other module directory.
STATIC_MODULE_TARGET	= def module_ctf

# Definitions for xramfs device configuration.
XRAMCONF	= $(ONBLD_TOOLS)/bin/xramconf
XRAMDEV_CONF	= $(UTSBASE)/$(PLATFORM)/conf/xramdev.conf
XRAMDEV_CONF_O	= $(OBJS_DIR)/xramdev_conf.o
XRAMDEV_CONF_C	= $(XRAMDEV_CONF_O:%.o=%.c)
CLEANFILES	+= $(XRAMDEV_CONF_C)

# Definitions for built-in device configuration.
DEVCONF			= $(ONBLD_TOOLS)/bin/devconf
BUILTIN_CONF		= $(UTSBASE)/$(PLATFORM)/conf/builtin.conf
BUILTIN_CONF_OBJ	= builtin_conf.o
BUILTIN_CONF_O		= $(OBJS_DIR)/$(BUILTIN_CONF_OBJ)
BUILTIN_CONF_C		= $(BUILTIN_CONF_O:%.o=%.c)
CLEANFILES		+= $(BUILTIN_CONF_C)
CORE_OBJS		+= $(BUILTIN_CONF_OBJ)

# 
# Targets
#
.KEEP_STATE:

def: $(DEF_DEPS)

all: $(ALL_DEPS)

clean: $(CLEAN_DEPS)

clobber: $(CLOBBER_DEPS)

lint: $(LINT_DEPS)

clean.lint: $(CLEAN_LINT_DEPS)

install: $(INSTALL_DEPS)

$(UNIX_BIN):	$(UNIX_BIN_DEP)
	$(LD) $(UNIX_LDFLAGS) -o $@ $(UNIX_O) $(MODSTUBS_O)	\
		$(LIBOPTS) $(DTRACESTUBS)
	$(CTFMERGE_UNIQUIFY_AGAINST_GENUNIX)
	$(POST_PROCESS)

$(UNIX_O):	$(OBJECTS) $(OBJS_DIR)/vers.o
	$(LD) -r -o $@ $(OBJECTS) $(OBJS_DIR)/vers.o

##
## Static linking
##

# Build directories for modules to be statically linked.
STATIC_DIR_COMMAND	= $(STATICTOOL) directory $(STATIC_CONF_FLAGS)
$(STATIC_UNIX)STATIC_MODULE_DIRS	= $(STATIC_DIR_COMMAND:sh)

# Static link modules
STATIC_MODULE_COMMAND	= $(STATICTOOL) modpath $(STATIC_CONF_FLAGS)
$(STATIC_UNIX)STATIC_MODULES	= $(STATIC_MODULE_COMMAND:sh)

# Archives for static modules.
# This file is used to merge all CTF data into vmunix.
$(STATIC_UNIX)STATIC_CTF_OBJECTS	= $(GENUNIX_DIR)/$(OBJS_DIR)/$(GENUNIX)
$(STATIC_UNIX)STATIC_CTF_OBJECTS	+=	\
	$(STATIC_MODULE_DIRS:%=%/$(MODULE_CTF_A))
$(STATIC_UNIX)STATIC_CTF_OBJECTS	+= $(DRVINFO_O) $(STATIC_DTRACESTUBS_O)
$(STATIC_UNIX)STATIC_CTF_OBJECTS	+= $(OBJECTS) $(OBJS_DIR)/vers.o

# Create static module information and embed it into kernerl.
BINDFILE_SRCDIR		= $(UTSBASE)/arm/os
NAME_TO_MAJOR		= $(BINDFILE_SRCDIR)/name_to_major
NAME_TO_SYSNUM		= $(BINDFILE_SRCDIR)/name_to_sysnum
DRIVER_ALIASES		= $(BINDFILE_SRCDIR)/driver_aliases
DRIVER_CLASSES		= $(BINDFILE_SRCDIR)/driver_classes
DACF_CONF		= $(BINDFILE_SRCDIR)/dacf.conf
DRVINFO_COMMAND		= $(STATICTOOL) drvinfo $(STATIC_CONF_FLAGS)	\
			  -n $(NAME_TO_MAJOR) -s $(NAME_TO_SYSNUM)	\
			  -A $(DRIVER_ALIASES) -C $(DRIVER_CLASSES)	\
			  -D $(DACF_CONF)
DRVINFO_C		= $(OBJS_DIR)/_drvinfo.c
DRVINFO_O		= $(DRVINFO_C:.c=.o)
DRVINFO_DEPEND		= $(STATIC_CONF) $(ARCH_STATIC_CONF)		\
			  $(STATIC_CONF_STAMP) $(ARCH_STATIC_CONF_STAMP) \
			  $(NAME_TO_MAJOR) $(NAME_TO_SYSNUM)		\
			  $(DRIVER_ALIASES) $(DRIVER_CLASSES)		\
			  $(DACF_CONF)
$(STATIC_UNIX)CLEANFILES	+= $(DRVINFO_O)
$(STATIC_UNIX)CLOBBERFILES	+= $(DRVINFO_O) $(DRVINFO_C)	\
				   $(VMUNIX_BIN) $(VMUNIX_REL) $(VMUNIX_SYM) \
				   $(STATIC_DTRACESTUBS_C)

VMUNIX_DEPEND	= utstune-init $(UNIX_O) $(GENLIB) $(DRVINFO_O)	\
		  $(STATIC_MODULES) $(MODSTUBS_O)
UTSTUNE_INIT_HOOK	= :

# Import fastboot definitions.
FABOOT_DIR=$(UTSBASE)/arm/io/fastboot
include $(FABOOT_DIR)/Makefile.fastboot

.PARALLEL:	$(STATIC_MODULE_DIRS)

# Initialize utstune environment.
utstune-init:	FRC
	@cd $(UTSBASE); pwd; $(MAKE) tune-init
	$(UTSTUNE_INIT_HOOK)

# Build modules to be linked to static vmunix, and create archive file
# to merge CTF section into vmunix.
$(STATIC_MODULE_DIRS):	FRC
	@cd $@; pwd; ARM_PLATFORM=$(PLATFORM) $(MAKE) $(STATIC_MODULE_TARGET)

$(STATIC_MODULES):	$(STATIC_MODULE_DIRS)

$(VMUNIX_O):	$(VMUNIX_DEPEND) FRC
	$(LD) -r -o $@ $(UNIX_O) $(DRVINFO_O) $(MODSTUBS_O)	\
		$(GENUNIX_DIR)/$(OBJS_DIR)/$(GENUNIX)		\
		$(STATIC_MODULES)

$(VMUNIX_REL):	$(VMUNIX_O) $(STATIC_DTRACESTUBS_O) $(MAPFILE)
	$(LD) $(UNIX_LDFLAGS) -o $(VMUNIX_REL)	\
		$(VMUNIX_O) $(STATIC_DTRACESTUBS_O)
	$(CTFMERGE_VMUNIX_MERGE)

$(VMUNIX_SYM):	$(VMUNIX_REL)
	$(STATIC_UNIXSYMS) -o $@ $(VMUNIX_REL)

$(VMUNIX_BIN):	$(VMUNIX_SYM)
	$(STATIC_UNIX_FIXUP)
	$(POST_PROCESS)

#
#	Special rules for generating kernaddr for creating U-boot image.
#
$(KERNADDR):	FRC
	@cd $(DSF_DIR); $(MAKE) all.targ

$(GENLIB): FRC
	@(cd $(GENLIB_DIR); pwd; ARM_PLATFORM=$(PLATFORM) $(MAKE) all.targ)
	@pwd

#
#	The global lint target builds the kernel lint library (llib-lunix.ln)
#	which is equivalent to a lint of /unix.o. Then all kernel modules for
#	this architecture are linted against the kernel lint library.
#
#	Note:	lint errors in the kernel lint library will be repeated for
#		each module. It is important that the kernel lint library
#		be clean to keep the textual output to a reasonable level.
#

$(LINT_LIB):	$(LINT_LIB_DIR) $(LINTS)
	@-$(ECHO) "\n$(UNIX): (library construction):"
	@$(LINT) -o$(UNIX) $(LINTFLAGS) $(LINTS)
	@$(MV) $(@F) $@

lintlib:	$(LINT_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/$(PLATFORM)/Makefile.targ
$(STATIC_UNIX)include $(UTSBASE)/Makefile.targ.uimage

include $(FABOOT_DIR)/Makefile.targ

# Localize symbols in zlib_obj.o.
ZLIB_OBJ_LDFLAGS	=
ZLIB_LOCALIZE_SYMBOL	= $(LOCALIZE_SYMBOL)
$(OBJS_DIR)/zlib_obj.o  := MAPFILE = $(ZLIB_MAPFILE)

$(OBJS_DIR)/static_cppflags:	$(STATIC_CONF) $(ARCH_STATIC_CONF)

$(MODSTUBS_O):	$(STATIC_CPPFLAGS_FILE)

$(DRVINFO_O):	$(DRVINFO_C)
	$(COMPILE.c) -o $@ $(DRVINFO_C)
	$(CTFCONVERT_O)

$(DRVINFO_C):	$(DRVINFO_DEPEND)
	$(DRVINFO_COMMAND) > $@ || ($(RM) $@; exit 1)

# Embed KERNELBASE value into GNU ld script.
$(GNULD_SCRIPT):	$(GNULD_SCRIPT_SRC) $(KERNADDR)
	$(SED) -e 's/%KERNELTEXTBASE%/$(KERNELTEXTBASE)/g'	\
		$(GNULD_SCRIPT_SRC) > $@ || ($(RM) $@; exit 1)

# Create xramfs device configuration.
$(XRAMDEV_CONF_C):	$(XRAMDEV_CONF)
	$(XRAMCONF) -C -r -f $(XRAMDEV_CONF) -o $@

$(XRAMDEV_CONF_O):	$(XRAMDEV_CONF_C)
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)

# Create built-in device configuration.
$(BUILTIN_CONF_C):	$(BUILTIN_CONF)
	$(DEVCONF) -r -o $@ $(BUILTIN_CONF)

$(BUILTIN_CONF_O):	$(BUILTIN_CONF_C)
	$(COMPILE.c) -o $@ $<
	$(CTFCONVERT_O)
