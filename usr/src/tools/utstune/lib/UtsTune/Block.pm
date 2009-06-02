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
## Base class for block statement.
##

package UtsTune::Block;

use strict;
use vars qw(@ISA);

use UtsTune::Parser;

@ISA = qw(UtsTune::Parser);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($type, $def) = (@_);

	my $me = {TYPE => $type, PARAM => {}, PARAMDEF => $def};
	return bless $me, $class;
}

sub getType
{
	my $me = shift;

	return $me->{TYPE};
}

sub getBlockDecl
{
	die "getBlockDecl: Must be overridden";
}

sub addParameter
{
	my $me = shift;
	my ($parser, $pname, $params) = @_;

	$parser->parseError("Duplicated parameter: $pname")
		if (exists($me->{PARAM}->{$pname}));

	my $def = $me->{PARAMDEF}->{$pname};
	my $dmap = $me->{PARAMDEF};
	$parser->parseError("Unknown parameter: $pname")
		if (!$def or
		    ($def->{nodef} and $parser->isDefinition()));

	my @values;
	my $typedef = $def->{type};
	my $load = "require $typedef; import $typedef;";
	eval $load;
	my $opttype = ($typedef) ? $typedef->new() : $me->getType();
	foreach my $a (@$params) {
		my ($token, $type) = (@$a);

		my $err = $opttype->checkValue($token, $type, $def);
		$parser->parseError("$pname: $err") if ($err);
		push(@values, $token);
	}
	if ($def->{list}) {
		$me->{PARAM}->{$pname} = \@values;
	}
	else {
		$parser->parseError("Too many parameter value for ",
				    "$pname.") if (@values > 1);
		$me->{PARAM}->{$pname} = $values[0];
	}
}

sub getParamType
{
	my $me = shift;
	my ($pname) = @_;

	my $type;
	my $def = $me->{PARAMDEF}->{$pname};
	my $typedef = $def->{type};
	if ($typedef) {
		my $load = "require $typedef; import $typedef;";
		eval $load;
		$type = $typedef->new();
	}
	else {
		$type = $me->getType();
	}

	return $type;
}

sub output
{
	my $me = shift;
	my ($fh) = @_;

	my $decl = $me->getBlockDecl();
	$fh->print("$decl\n{\n");
	foreach my $pname (sort keys %{$me->{PARAM}}) {
		my $def = $me->{PARAMDEF}->{$pname};
		my $value = $me->{PARAM}->{$pname};
		my $type = $me->getParamType($pname);
		$value = [$value] unless (ref($value) eq 'ARRAY');
		my $v = join(', ',
			     map {$type->dumpValue($_)} @$value);
		$fh->printf("\t%s: %s;\n", $pname, $v);
	}
	$fh->print("}\n");
}

sub getParameter
{
	my $me = shift;
	my ($pname) = @_;

	my $pmap = $me->{PARAM};
	return $pmap->{$pname};
}

sub setParameter
{
	my $me = shift;
	my ($pname, $value) = @_;

	my $pmap = $me->{PARAM};
	$pmap->{$pname} = $value;
}

sub fixup
{
	my $me = shift;
	my ($parser) = @_;

	my $dmap = $me->{PARAMDEF};
	my $pmap = $me->{PARAM};
	foreach my $k (%$dmap) {
		$parser->parseError("\"$k\" is not defined.")
			if ($dmap->{$k}->{mandatory} and !$pmap->{$k});
	}
}

sub getStringParameter
{
	my $me = shift;
	my ($key) = @_;

	my $value = $me->getParameter($key);
	$value = $me->evalString($value) if ($value);

	return $value;
}

sub setStringParameter
{
	my $me = shift;
	my ($key, $value) = @_;

	$me->setParameter($key, $me->stringfy($value));
}

sub getBool
{
	my $me = shift;
	my ($pname) = @_;

	my $type = $me->getParamType($pname);
	my $value = $me->getParameter($pname);

	return $type->isTrue($value);
}

1;
