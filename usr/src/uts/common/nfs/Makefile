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
# ident	"%Z%%M%	%I%	%E% SMI"
#
# uts/common/nfs/Makefile
#
# include global definitions
#
include ../../../Makefile.master

HDRS=	export.h	lm.h						\
	mount.h		nfs.h		nfssys.h	nfs_acl.h	\
	nfs_clnt.h	nfs_log.h 	nfs_sec.h	nfs4.h		\
	nfs4_attr.h	nfs4_clnt.h	rnode.h		rnode4.h	\
	nfs4_kprot.h	nfs4_db_impl.h	nfs4_idmap_impl.h \
	nfsid_map.h	auth.h

$(CLOSED_BUILD)CLOSEDHDRS=	lm_impl.h	lm_nlm.h	lm_server.h

ALLHDRS= $(HDRS) $(CLOSEDHDRS)

ROOTDIRS= $(ROOT)/usr/include/nfs

ROOTHDRS= $(ALLHDRS:%=$(ROOTDIRS)/%)

CHECKHDRS= $(HDRS:%.h=%.check) \
	$(CLOSEDHDRS:%.h=$(CLOSED)/uts/common/nfs/%.check)

$(ROOTDIRS)/%: %
	$(INS.file)

$(ROOTDIRS)/%: $(CLOSED)/uts/common/nfs/%
	$(INS.file)

.KEEP_STATE:

.PARALLEL: $(CHECKHDRS)

install_h: $(ROOTDIRS) $(ROOTHDRS)

$(ROOTDIRS):
	$(INS.dir)

check:	$(CHECKHDRS)
