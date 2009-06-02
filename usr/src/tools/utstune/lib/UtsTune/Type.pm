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
# Copyright (c) 2007 NEC Corporation
# All rights reserved.
#

##
## Classes for parameter type.
##

package UtsTune::Type;

use strict;
use vars qw(@ISA);

use UtsTune::Parser;

@ISA = qw(UtsTune::Parser);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($value) = @_;

	my $me = bless {VALUE => $value}, $class;

	return $me;
}

sub getValue
{
	my $me = shift;

	return $me->{VALUE};
}

sub setValue
{
	my $me = shift;
	my ($value) = @_;

	$me->{VALUE} = $value;
}

sub getType
{
	die "getType: Must be overridden";
}

sub checkValue
{
	die "checkValue: Must be overridden";
}

sub compare
{
	my $me = shift;
	my ($v1, $v2) = @_;

	return $v1 cmp $v2;
}

sub canCompare
{
	my $me = shift;
	my $type = shift;

	return ref($type) eq ref($me);
}

sub checkMax
{
	my $me = shift;
	my ($value, $max) = @_;

	return 1 unless (defined($max));

	my $type = $me->getType();
	die "\"max\" is not supported for $type type.\n";
}

sub checkMin
{
	my $me = shift;
	my ($value, $min) = @_;

	return 1 unless (defined($min));

	my $type = $me->getType();
	die "\"min\" is not supported for $type type.\n";
}

sub checkCandidates
{
	my $me = shift;
	my ($value, $cds) = @_;

	return 1 unless (defined($cds));

	my (@list) = (grep {$me->compare($_, $value) == 0} @$cds);
	return (@list > 0);
}

sub checkPower
{
	my $me = shift;
	my ($value, $power) = @_;

	return 1 unless (defined($power));

	my $type = $me->getType();
	die "\"power\" is not supported for $type type.\n";
}

sub checkAlign
{
	my $me = shift;
	my ($value, $align) = @_;

	return 1 unless (defined($align));

	my $type = $me->getType();
	die "\"align\" is not supported for $type type.\n";
}

sub dumpValue
{
	my $me = shift;
	my $value = shift || $me->getValue();

	return $value;
}

sub macroValue
{
	my $me = shift;
	my ($value) = @_;

	return $me->dumpValue($value);
}

sub dumpMacro
{
	my $me = shift;
	my ($fh, $name, $value) = @_;

	$value = $me->macroValue($value);
	$fh->print(<<OUT);
#define	$name	$value
OUT
}

sub valueFromArgument
{
	die "valueFromArgument: Must be overridden";
}

# Convert option argument into internal representation.
sub evalValue
{
	my $me = shift;
	my ($arg) = @_;

	my ($value, $type) = $me->valueFromArgument($arg);
	my $err = $me->checkValue($value, $type);
	die "$err\n" if ($err);

	return $value;
}

sub isTrue
{
	my $me = shift;
	my ($value) = @_;

	return ($value) ? 1 : undef;
}

1;
