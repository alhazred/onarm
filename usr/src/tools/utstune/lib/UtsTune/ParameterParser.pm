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
## utstune parameter parser.
##

package UtsTune::ParameterParser;

use strict;
use vars qw(@ISA);

use Encode::Guess;

use UtsTune;
use UtsTune::AbstractIntegerParser;

@ISA = qw(UtsTune::AbstractIntegerParser);

# Must be overridden.
sub nextToken;
sub ungetToken;
sub tokenize;
sub getChar;
sub ungetChar;
sub isTokenChar;

sub parseString
{
	my $me = shift;
	my ($first) = @_;

	my $token = $me->parseStringImpl($first);
	while (my ($nt, $type) = $me->nextToken()) {
		unless ($type == $me->STRING) {
			$me->ungetToken($nt, $type);
			last;
		}

		# Concatenate string.
		$token = substr($token, 0, length($token) - 1);
		$nt = substr($nt, 1);
		$token .= $nt;
	}

	# Check whether this string is valid.
	$me->evalString($token);

	# Do not allow encoding other than US-ASCII.
	my $e = guess_encoding($token);
	$me->parseError("Invalid encoding in string.") unless (ref($e));

	return $token;
}

sub parseStringImpl
{
	my $me = shift;
	my ($first) = @_;

	my $token = $first;
	while (1) {
		my $c = $me->getChar();

		$me->parseError("Unterminated string.") unless (defined($c));

		$token .= $c;
		my $escape;
		if ($c eq '\\') {
			# Currently, we check double quote escape only.
			$c = $me->getChar();
			$me->parseError("Invalid backslash escape in string.")
				unless (defined($c));
			$token .= $c;
			$escape = 1;
		}
		last if (!$escape and $c eq '"');
	}

	return $token;
}

sub parseSymbol
{
	my $me = shift;
	my ($first) = @_;

	my $c = $first;
	my $token;
	while (defined($c)) {
		if ($me->isTokenChar($c) or $c =~ /^\s$/) {
			$me->ungetChar($c);
			last;
		}
		if ((!$token and $c !~ /^[a-zA-Z]$/) or $c !~ /^[\w-]$/) {
			$me->parseError("Invalid string in SYMBOL: [$c]");
		}

		$token .= $c;
		$c = $me->getChar();
	}

	return $token;
}

1;
