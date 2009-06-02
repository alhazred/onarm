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
## driver_aliases parser.
##

package StaticTool::DriverAliasesParser;

use strict;
use vars qw(@ISA);

use File::Basename;

use StaticTool;
use StaticTool::BindingLexer;

@ISA = qw(StaticTool::BindingLexer);

use constant	S_BEGIN		=> 1;
use constant	S_DRVNAME	=> 2;
use constant	S_DRVNAME_COMMA	=> 3;
use constant	S_ALIAS		=> 4;
use constant	S_ALIAS_COMMA	=> 5;

sub parse
{
	my $me = shift;

	return () if ($me->{EOF});
	my ($drvname, $alias) = ('', '');
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
				$drvname = $token;
				$state = S_DRVNAME;
			}
			elsif ($state == S_DRVNAME_COMMA) {
				$drvname .= $token;
				$state = S_DRVNAME;
			}
			elsif ($state == S_ALIAS_COMMA) {
				$alias .= $token;
				$state = S_ALIAS;
			}
			elsif ($state == S_DRVNAME) {
				$alias = $token;
				$state = S_ALIAS;
			}
			elsif ($state == S_ALIAS) {
				$me->{UNPARSED} = [$token, $type];
				last LOOP;
			}
		}
		elsif ($type == $me->COMMA) {
			if ($state == S_DRVNAME) {
				$drvname .= $token;
				$state = S_DRVNAME_COMMA;
			}
			elsif ($state == S_ALIAS) {
				$alias .= $token;
				$state = S_ALIAS_COMMA;
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
	if ($state == S_ALIAS) {
		$me->parseError("Driver name is undefined.")
			unless ($drvname);
		$me->parseError("Alias name is undefined.")
			unless ($alias);
		@ret = ($drvname, $alias);
	}
	elsif ($state != S_BEGIN) {
		my $err = ($me->{EOF}) ? "EOF" : "NEWLINE";
		$me->syntaxError("Unexpected $err");
	}

	return @ret;
}

sub dumpStruct
{
	my $me = shift;
	my ($majmap) = @_;

	my (%almap, @keys);
	my $file = $me->file();
	while (my ($drvname, $alias) = $me->parse()) {
		my $num = $majmap->{$drvname};
		die "Undefined driver name in $file: $drvname\n"
			unless (defined($num));
		die "Duplicated alias in $file: $alias\n"
			if (defined($almap{$alias}));
		$almap{$alias} = $num;
		push(@keys, $alias);
	}

	my $num = scalar(@keys);
	my $fname = basename($file);
	print <<OUT;

/* Embedded $fname */
const int  static_drvalias_count = $num;

const static_bind_t static_drvalias[] = {
OUT
	foreach my $n (@keys) {
		my $v = $almap{$n};
		my $nm = stringfy($n);
		print <<OUT;
	{ $nm, $v },
OUT
	}
	print <<OUT;
};
OUT
}

1;
