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

## Binding file parser that contains string binding name and integer value.
## This class is used to parse name_to_major and name_to_sysnum.

package StaticTool::IntBindParser;

use strict;
use vars qw(@ISA);

use StaticTool::BindingLexer;

@ISA = qw(StaticTool::BindingLexer);

use constant	S_BEGIN		=> 1;
use constant	S_NAME		=> 2;
use constant	S_VALUE		=> 3;

sub parse
{
	my $me = shift;

	return () if ($me->{EOF});
	my ($bname, $value);
	my $state = S_BEGIN;

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken();
		unless (defined($token)) {
			$me->{EOF} = 1;
			last;
		}

		if ($type == $me->SYMBOL or $type == $me->STRING) {
			$token =~ s/^\x22(.*)\x22$/$1/o
				if ($type == $me->STRING);
			if ($state == S_BEGIN) {
				$bname = $token;
				$state = S_NAME;
			}
			elsif ($state == S_VALUE) {
				$me->{UNPARSED} = [$token, $type];
				last LOOP;
			}
		}
		elsif ($type == $me->INT) {
			if ($state == S_NAME) {
				$value = $token;
				$state = S_VALUE;
			}
			else {
				$me->syntaxError("Unexpected token type: [",
						 $token, "]: ", $type);
			}
		}
		elsif ($type == $me->NEWLINE) {
			last LOOP unless ($state == S_BEGIN);
		}
		else {
			$me->syntaxError("Unexpected token type: [", $token,
					 "]: ", $type);
		}
	}

	my @ret;
	if ($state == S_VALUE) {
		$me->parseError("Binding name is undefined.")
			unless ($bname);
		$me->parseError("Value is undefined.")
			unless (defined($value));
		@ret = ($bname, $value);
	}
	elsif ($state != S_BEGIN) {
		my $err = ($me->{EOF}) ? "EOF" : "NEWLINE";
		$me->syntaxError("Unexpected $err");
	}

	return @ret;
}

1;
