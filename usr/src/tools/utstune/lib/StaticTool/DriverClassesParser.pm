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
## driver_classes parser.
##

package StaticTool::DriverClassesParser;

use strict;
use vars qw(@ISA);

use File::Basename;

use StaticTool;
use StaticTool::BindingLexer;

@ISA = qw(StaticTool::BindingLexer);

use constant	S_BEGIN		=> 1;
use constant	S_DRVNAME	=> 2;
use constant	S_CLASS		=> 3;

sub parse
{
	my $me = shift;

	return () if ($me->{EOF});
	my ($drvname, $class) = ('', '');
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
			elsif ($state == S_DRVNAME) {
				$class = $token;
				$state = S_CLASS;
			}
			elsif ($state == S_CLASS) {
				$me->syntaxError("Extra noise after entry.");
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
	if ($state == S_CLASS) {
		$me->parseError("Driver name is undefined.") unless ($drvname);
		$me->parseError("Class is undefined.") unless ($class);
		@ret = ($drvname, $class);
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

	my (%map, @entries);
	my $file = $me->file();
	while (my ($drvname, $class) = $me->parse()) {
		my $num = $majmap->{$drvname};
		die "Undefined driver name in $file: $drvname\n"
			unless (defined($num));
		my $key = $drvname . ',' . $class;
		die "Duplicated entry in $file: $class\n"
			if (defined($map{$key}));
		$map{$key} = $1;
		push(@entries, [$drvname, $class]);
	}

	my $num = scalar(@entries);
	my $fname = basename($file);
	print <<OUT;

/* Embedded $fname */
const int  static_drvclass_count = $num;

const static_strbind_t static_drvclass[] = {
OUT
	foreach my $e (@entries) {
		my ($nm, $cl) = (stringfy($e->[0]), stringfy($e->[1]));
		print <<OUT;
	{ $nm, $cl },
OUT
	}
	print <<OUT;
};
OUT
}

1;
