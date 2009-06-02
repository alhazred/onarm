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
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
# Copyright (c) 2008 NEC Corporation
#
#
# ident	"@(#)Makefile	1.3	07/06/25 SMI"
#

PROG=		czfs
PROG_MIN=	czfs_min
OBJS=		zfs_main.o zfs_iter.o
OBJS_MIN=	zfs_main_min.o zfs_iter_min.o
SRCS=		$(OBJS:%.o=../zfs/%.c)
POFILES=	$(OBJS:%.o=%.po)
POFILE=		czfs.po

include ../Makefile.cmd

FSTYPE=         czfs
LINKPROGS=	mount umount
ROOTETCFSTYPE=  $(ROOTETC)/fs/$(FSTYPE)
USRLIBFSTYPE=	$(ROOTLIB)/fs/$(FSTYPE)

LIBUMEM = -lumem
$(ARM_BLD)LIBUMEM =
LDLIBS += -lczfs -luutil $(LIBUMEM) -lnvpair -lavl
$(__ARLIB)$(ARM_BLD)LDLIBS += -lnsl -ldevid -lgen -lm -ldevinfo -lmd

C99MODE=	-xc99=%all
C99LMODE=	-Xc99=%all

CFLAGS += -I../../uts/common/fs/czfs -I./
CPPFLAGS += -D_LARGEFILE64_SOURCE=1 -D_REENTRANT

CLOBBERFILES = $(POFILE) $(POFILES:../zfs/%=%) $(PROG_MIN)

$(ARM_BLD)CPPFLAGS += -DMNTFS_DISABLE

# lint complains about unused _umem_* functions
LINTFLAGS += -xerroff=E_NAME_DEF_NOT_USED2 
LINTFLAGS64 += -xerroff=E_NAME_DEF_NOT_USED2 

ROOTUSRSBINLINKS = $(PROG:%=$(ROOTUSRSBIN)/%)
USRLIBFSTYPELINKS = $(LINKPROGS:%=$(USRLIBFSTYPE)/%)
ROOTETCFSTYPELINKS = $(LINKPROGS:%=$(ROOTETCFSTYPE)/%)
ROOTSBINPROG_MIN= $(PROG_MIN:%=$(ROOTSBIN)/%)

.KEEP_STATE:

.PARALLEL:

all: $(PROG) $(PROG_MIN)

%.po: ../zfs/%.c
	$(CPPFORPO) $< > $(<:../zfs/%=%).i
	$(BUILD.po)

BUILD.po= $(XGETTEXT) $(XGETFLAGS) -d $(<F) $(<:../zfs/%=%).i ;\
	$(RM)	$(@:../zfs/%=%) ;\
	sed "/^domain/d" < $(<F).po > $(@:../zfs/%=%) ;\
	$(RM) $(<F).po $(<:../zfs/%=%).i

$(PROG): $(OBJS)
	$(LINK.c) -o $@ $(OBJS) $(LDLIBS)
	$(POST_PROCESS)

$(PROG_MIN): $(OBJS_MIN)
	$(LINK.c) -o $@ $(OBJS_MIN) $(LDLIBS)
	$(POST_PROCESS)

install: all $(ROOTSBINPROG) $(ROOTUSRSBINLINKS) $(USRLIBFSTYPELINKS) \
	$(ROOTETCFSTYPELINKS) $(ROOTSBINPROG_MIN)

$(POFILE): $(POFILES)
	$(RM) $@
	cat $(POFILES) > $@

clean:
	$(RM) $(OBJS) $(OBJS_MIN)

lint:	lint_SRCS

# Links from /usr/sbin to /sbin
$(ROOTUSRSBINLINKS):
	-$(RM) $@; $(SYMLINK) ../../sbin/$(PROG) $@

# Links from /usr/lib/fs/zfs to /sbin
$(USRLIBFSTYPELINKS):
	-$(RM) $@; $(SYMLINK) ../../../../sbin/$(PROG) $@

# Links from /etc/fs/zfs to /sbin
$(ROOTETCFSTYPELINKS):
	-$(RM) $@; $(SYMLINK) ../../../sbin/$(PROG) $@

FRC:

%.o: ../zfs/%.c
	$(COMPILE.c) -c -o $@	\
	$(OUTPUT_OPTION) -DZFS_COMPACT $< $(CTFCONVERT_HOOK) 
	$(POST_PROCESS_O)

%_min.o: ../zfs/%.c
	$(COMPILE.c) -c -o $@	\
	$(OUTPUT_OPTION) -DZFS_COMPACT -DZFS_CMD_MINIMUMSET $< \
	$(CTFCONVERT_HOOK)
	$(POST_PROCESS_O)

include ../Makefile.targ
