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
# Copyright (c) 2007-2008 NEC Corporation
# All rights reserved.
#

##
## 32bit unsigned integer type
##

package UtsTune::Type::UnsignedInteger;

use strict;
use vars qw(@ISA);

use UtsTune::Type::Integer;

@ISA = qw(UtsTune::Type::Integer);

use constant	UINT_MAX	=> 4294967295;

sub getType
{
	return 'uint';
}

sub checkValue
{
	my $me = shift;
	my ($value, $type, $def) = @_;

	my $name = $me->getType();
	return "Invalid value for $name: $value"
		unless ($type == $me->INT);
	return "Too large for $name: $value" if ($value > UINT_MAX);
	return "Must be larger than zero."
		if ($def and $def->{natural} and $value <= 0);

	return undef;
}

sub getSuffix
{
	return 'U';
}

sub dumpValue
{
	my $me = shift;
	my $value = shift || $me->getValue();
	my $detailed = shift;

	my $ret = sprintf("0x%x", $value & 0xffffffff);
	$ret .= sprintf(" (%u)", $value) if ($detailed);

	return $ret;
}

1;
