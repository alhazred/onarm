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
## driver.conf file parser.
##

package StaticTool::DriverConfParser;

use strict;
use vars qw(@ISA);

use StaticTool;
use StaticTool::BindingLexer;

@ISA = qw(StaticTool::BindingLexer);

sub parse
{
	my $me = shift;

	my %prop;
	delete($me->{LAST_SYMBOL});
	while (1) {
		my ($name, $type) = $me->nextToken();
		return undef unless ($name);
		next if ($type == $me->NEWLINE);
		if ($type == $me->SEMI) {
			delete($me->{LAST_SYMBOL});
			last if (%prop);
			next;
		}
		if ($type == $me->COMMA) {
			$name = $me->{LAST_SYMBOL};
			$me->syntaxError("Unexpected ','.")
				unless ($name);
		}
		else {
			$me->{LAST_SYMBOL} = $name;
			$me->syntaxError("SYMBOL is required.")
				unless ($type == $me->SYMBOL);
			my ($equal, $etype) = $me->nextToken();
			if ($etype == $me->SYMBOL or
			    $etype == $me->SEMI) {
				# Treat as boolean value.
				delete($me->{LAST_SYMBOL});
				$me->{UNPARSED} = [$equal, $etype];
				$me->addProperty(\%prop, $name, 0, $me->INT);
				last if ($etype == $me->SEMI);
				next;
			}
			$me->syntaxError("EQUAL is required.")
				unless ($etype == $me->EQUAL);
		}
		my ($value, $type) = $me->nextToken();
		$me->syntaxError("Invalid value: $type")
			unless ($type == $me->STRING or $type == $me->INT);
		$me->addProperty(\%prop, $name, $value, $type);
	}

	my $name = $prop{name};
	my $class = $prop{class};
	my $parent = $prop{parent};
	$me->parseError("Missing name attribute.")
		if (!$name and ($class or $parent));
	$me->parseError("Missing parent or class attribute.")
		if ($name and (!$class and !$parent));
	return \%prop;
}

sub addProperty
{
	my $me = shift;
	my ($prop, $name, $value, $type) = @_;

	my $keys = $prop->{KEYS};
	my $values = $prop->{VALUES};
	my $types = $prop->{TYPES};
	unless ($keys) {
		$keys = [];
		$prop->{KEYS} = $keys;
	}
	unless ($values) {
		$values = {};
		$prop->{VALUES} = $values;
	}
	unless ($types) {
		$types = {};
		$prop->{TYPES} = $types;
	}
	if ($name =~ /^(parent|name|class)$/io) {
		# Use lower case for special keyword.
		$name = lc($name);
		$me->parseError("\"$name\" is already defined.")
			if ($prop->{$name});
		$me->parseError("Value of \"$name\" must be a STRING.")
			if ($type != $me->STRING);
		$prop->{$name} = $value;
	}
	else {
		my $v = $values->{$name};
		if (defined($v)) {
			$me->parseError("Mixed types in \"$name\".")
				if ($type != $types->{$name});
			push(@$v, $value);
		}
		else {
			push(@$keys, $name);
			$values->{$name} = [$value];
			$types->{$name} = $type;
		}
	}
}

1;
