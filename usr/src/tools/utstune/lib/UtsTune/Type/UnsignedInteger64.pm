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
## 64bit unsigned long long type
##

package UtsTune::Type::UnsignedInteger64;

use strict;
use vars qw(@ISA);

use UtsTune::Type::Integer64;

@ISA = qw(UtsTune::Type::Integer64);

sub getType
{
	return 'uint64';
}

sub checkValue
{
	my $me = shift;
	my ($value, $type, $def) = @_;

	my $name = $me->getType();
	return "Invalid value for $name: $value"
		unless ($type == $me->INT);
	return "Must be larger than zero."
		if ($def and $def->{natural} and $value <= 0);

	# Range check will be done by parser.
	return undef;
}

sub getSuffix
{
	return 'ULL';
}

sub dumpValue
{
	my $me = shift;
	my $value = shift || $me->getValue();
	my $detailed = shift;

	my $ret = sprintf("0x%llx", $value);
	$ret .= sprintf(" (%llu)", $value) if ($detailed);

	return $ret;
}

1;
