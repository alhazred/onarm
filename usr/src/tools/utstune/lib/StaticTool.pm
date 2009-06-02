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
## Global definitions for statictool.
##

package StaticTool;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;

@ISA = qw(Exporter);

# Default modinfo filename.
use constant	DEFAULT_MODINFO		=> 'modinfo';

# NULL definition for C language.
use constant	C_NULL			=> 'NULL';

# Make string.
sub stringfy(@)
{
	my ($str, $noquote) = @_;

	my $ret;
	if (defined($str)) {
		$ret = ($noquote) ? $str : "\"$str\"";
	}
	else {
		$ret = C_NULL;
	}
	return $ret;
}

# Evaluate quoted string.
sub eval_string($)
{
	my ($str) = @_;

	my $s = eval "return $str";
	if ($@) {
		my $err = "$@";
		chomp($err);
		die "Invalid string token: $str\n";
	}

	return $s;
}

# Escape string for symbol name.
sub escape_symbol($)
{
	my ($str) = @_;

	$str = 'X' . $str unless ($str =~ /^[a-zA-Z]/);
	$str =~ s/\./_/g;

	return $str;
}

@EXPORT = qw(DEFAULT_MODINFO C_NULL stringfy eval_string escape_symbol);

1;
