#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#ident	"%Z%%M%	%I%	%E% SMI"
#
#cmd/ipf/Makefile.ipf
#

#
# Copyright (c) 2008 NEC Corporation
#

LIBIPF=		../../lib/$(MACH)/libipf.a
LIBIPF64=		../../lib/$(MACH64)/libipf.a

COMMONIPF=	$(SRC)/uts/common/inet/ipf
KERNELIPF=	$(SRC)/uts/common/inet/pfil

CPPFLAGS	+= -I$(COMMONIPF) -I$(KERNELIPF) -DSUNDDI \
		   -DSOLARIS2=$(RELEASE_MINOR)
$(USE_INET6)CPPFLAGS	+= -DUSE_INET6
