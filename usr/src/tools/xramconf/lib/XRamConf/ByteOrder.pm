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
## Utilities related to byte order (endianness).
##

package XRamConf::ByteOrder;

use strict;
use vars qw(@ISA $HOST_ORDER @EXPORT);
use Config;
use Exporter;

use constant	LITTLE_ENDIAN	=> 1;
use constant	BIG_ENDIAN	=> 2;

@ISA = qw(Exporter);

# Determine host byte order.
$HOST_ORDER = ($Config{byteorder} =~ /^1234/) ? LITTLE_ENDIAN : BIG_ENDIAN;

sub get_host_byteorder
{
	return $HOST_ORDER;
}

sub bswap_16
{
	my ($value) = @_;

	$value = (($value & 0x00ff) << 8) | (($value & 0xff00) >> 8);

	return $value;
}

sub bswap_32
{
	my ($value) = @_;

	$value = (($value & 0x000000ff) << 24) |
		(($value & 0x0000ff00) << 8) |
		(($value & 0x00ff0000) >> 8) |
		(($value & 0xff000000) >> 24);

	return $value;
}

sub bswap_64
{
	my ($value) = @_;

	$value = (bswap_32($value) << 32) | bswap_32($value >> 32);

	return $value;
}

@EXPORT = qw(LITTLE_ENDIAN BIG_ENDIAN get_host_byteorder
	     bswap_16 bswap_32 bswap_64);

1;
