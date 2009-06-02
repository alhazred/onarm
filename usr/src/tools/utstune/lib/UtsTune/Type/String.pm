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
## String type
##

package UtsTune::Type::String;

use strict;
use vars qw(@ISA);

use UtsTune::Type;

@ISA = qw(UtsTune::Type);

sub getType
{
	return 'string';
}

sub checkValue
{
	my $me = shift;
	my ($value, $type) = @_;

	my $name = $me->getType();
	return "Invalid value for $name: $value"
		unless ($type == $me->STRING);
	return undef;
}

sub valueFromArgument
{
	my $me = shift;
	my ($arg) = @_;

	return ($me->stringfy($arg), $me->STRING);
}

sub isTrue
{
	my $me = shift;
	my ($value) = @_;

	my $v = $me->evalString($value);
	return ($value) ? 1 : undef;
}

1;
