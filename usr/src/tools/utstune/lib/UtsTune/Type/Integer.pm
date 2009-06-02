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
## 32bit integer type
##

package UtsTune::Type::Integer;

use strict;
use vars qw(@ISA);

use Scalar::Util qw(blessed);

use UtsTune;
use UtsTune::Type;
use UtsTune::IntegerParser;

@ISA = qw(UtsTune::Type);

use constant	INT_MAX	=> 2147483647;
use constant	INT_MIN	=> -2147483648;

sub getType
{
	return 'int';
}

sub checkValue
{
	my $me = shift;
	my ($value, $type, $def) = @_;

	my $name = $me->getType();
	return "Invalid value for $name: $value"
		unless ($type == $me->INT or $type == $me->NEGATIVE);
	return "Too large for $name: $value" if ($value > INT_MAX);
	return "Too small for $name: $value" if ($value < INT_MIN);
	return "Must be larger than zero."
		if ($def and $def->{natural} and $value <= 0);

	return undef;
}

sub checkMax
{
	my $me = shift;
	my ($value, $max) = @_;

	return (!defined($max) or $value <= $max);
}

sub checkMin
{
	my $me = shift;
	my ($value, $min) = @_;

	return (!defined($min) or $value >= $min);
}

sub checkPower
{
	my $me = shift;
	my ($value, $power) = @_;

	return 1 unless (defined($power));

	return undef if ($value <= 0);
	my $n = int(log($value) / log($power));

	return ($power ** $n == $value);
}

sub checkAlign
{
	my $me = shift;
	my ($value, $align) = @_;

	return 1 unless (defined($align));

	return (($value % $align) == 0);
}

sub compare
{
	my $me = shift;
	my ($v1, $v2) = @_;

	return $v1 <=> $v2;
}

sub canCompare
{
	my $me = shift;
	my $type = shift;

	return blessed($type) and $type->isa('UtsTune::Type::Integer');
}

sub dumpValue
{
	my $me = shift;
	my $value = shift || $me->getValue();
	my $detailed = shift;

	return ($detailed)
		? sprintf("0x%x (%d)", $value & 0xffffffff, $value)
		: sprintf("%d", $value);
}

sub macroValue
{
	my $me = shift;
	my ($value) = @_;

	my $v = $me->dumpValue($value);
	my $suffix = $me->getSuffix();
	if ($suffix) {
		return CONCAT() . "($v, $suffix)";
	}

	return $v;
}

sub getSuffix
{
	return undef;
}

sub valueFromArgument
{
	my $me = shift;
	my ($arg) = @_;

	my $parser = UtsTune::IntegerParser->new($arg);
	return $parser->parse();
}

1;
