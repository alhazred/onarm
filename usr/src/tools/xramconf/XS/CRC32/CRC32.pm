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
# Copyright (c) 2008 NEC Corporation
# All rights reserved.
#

##
## CRC32 generator.
##

package XRamConf::Digest::CRC32;

use strict;
use vars qw(@ISA);

use Exporter;
use DynaLoader;

use XRamConf::Struct::WbDigest;

@ISA = qw(XRamConf::Digest Exporter DynaLoader);

bootstrap XRamConf::Digest::CRC32;

sub hexdigest
{
	my $me = shift;

	my $crc = $me->getValue();
	return sprintf("%08x\n", $crc);
}

sub getDigestType
{
	return WBDT_CRC32;
}

1;
