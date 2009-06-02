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
## Type for octal integer
##

package XRamConf::Type::Octal;

use strict;
use vars qw(@ISA);

use UtsTune::Type;
use UtsTune::Parser;

use constant	OCTAL		=> (100 | UtsTune::Parser::TOKEN_VALUE);

@ISA = qw(UtsTune::Type);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my $value = shift;

	my $me = $class->SUPER::new($value);
	return $me;
}

sub getType
{
	return 'octal';
}

sub checkValue
{
	# Check will be done by tokenize().
	return undef;
}

sub tokenize
{
	my $me = shift;
	my ($parser, $c) = @_;

	my $token = '';
	while (defined($c)) {
		if ($c eq ';' or $c eq '{' or $c eq '}' or
		    $c eq ':' or $c eq '"' or $c eq '#' or $c eq ',' or
		    $c =~ /^\s$/) {
			$parser->ungetChar($c);
			last;
		}
		$parser->parseError("Invalid character in OCTAL: [$c]")
			unless ($c =~ /^[0-7]$/o);
		$token .= $c;
		$c = $parser->getChar();
	}

	$token = oct('0' . $token);

	return ($token, OCTAL);
}

1;
