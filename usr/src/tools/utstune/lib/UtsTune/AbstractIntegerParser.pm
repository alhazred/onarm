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
## Base class for integer parser
##

package UtsTune::AbstractIntegerParser;

use strict;
use vars qw(@ISA);

use UtsTune::Parser;

@ISA = qw(UtsTune::Parser);

sub getChar
{
	die "getChar: Must be overridden";
}

sub ungetChar
{
	die "getChar: Must be overridden";
}

sub parseError
{
	die "getChar: Must be overridden";
}

sub parseInt
{
	my $me = shift;
	my ($c) = (@_);

	my $arg = $c;
	my ($base, $type);
	if ($c eq '-') {
		# Negative value
		$type = $me->NEGATIVE;
		$c = $me->getChar();
		$arg .= $c;
	}
	else {
		$type = $me->INT;
	}

	if ($c eq '0') {
		$c = $me->getChar();
		return (0, $me->INT) if (!defined($c));

		$arg .= $c;
		if ($c eq 'x' or $c eq 'X') {
			$base = 16;
			$c = $me->getChar();
			$arg .= $c;
		}
		else {
			$base = 8;
		}
	}
	else {
		$base = 10;
	}

	my $value;
	while (defined($c)) {
		my $v;
		if ($c =~ /^\d$/o) {
			$v = $c + 0;
		}
		elsif ($c =~ /^[a-f]$/io) {
			$v = ord(lc($c)) - ord('a') + 10;
		}
		else {
			$me->ungetChar($c);
			my $len = length($arg);
			$arg = substr($arg, 0, $len - 1) if ($len > 0);
			last;
		}
		
		$me->parseError("Invalid character in integer: $c")
			if ($v >= $base);

			$value = $value * $base + $v;
		$me->parseError("Integer overflow: $arg")
			if ($value =~ /\+/o);
		$c = $me->getChar();
		$arg .= $c;
	}

	unless (defined($value)) {
		if ($arg eq '0') {
			$value = 0;
		}
		else {
			$me->parseError("No integer value is ",
					"specified after \"$arg\".");
		}
	}
	$type = $me->INT if ($value == 0);

	if ($type == $me->NEGATIVE) {
		$value *= -1;
		$me->parseError("Integer overflow: $arg")
			if ($value =~ /\+/o);
	}

	return ($value, $type);
}

1;
