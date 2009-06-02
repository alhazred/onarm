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
# Copyright (c) 2008-2009 NEC Corporation
# All rights reserved.
#

##
## Common constants and simple functions.
##

package DevConf::Constants;

use strict;
use vars qw(@ISA @EXPORT);

use Exporter;
use UtsTune::Parser;

@ISA = qw(Exporter);

##
## Parser constants
##

# Token type for cpp macros
use constant	CPP		=> (110 | UtsTune::Parser::TOKEN_VALUE);
use constant	OBJ		=> (111 | UtsTune::Parser::TOKEN_VALUE);

# Maximum value of integer
use constant	INT_MIN		=> -2147483648;
use constant	UINT_MAX	=> 4294967295;
use constant	CHAR_MIN	=> -128;
use constant	UCHAR_MAX	=> 255;

# NULL in C language.
use constant	C_NULL		=> 'NULL';

# Maximum number of properties per type.
use constant	MAX_NPROPS	=> 255;

# Maximum length of node name.
use constant	MAX_NODENAMELEN	=> 31;

# Special property name
use constant	PROP_REG	=> 'reg';
use constant	PROP_INTERRUPTS	=> 'interrupts';

# Prefix for internal symbols.
use constant	SYM_INTERNAL_PREFIX	=> '__builtin_device__';
use constant	SYM_OBJECT_PREFIX	=> '__builtin_device__obj_';

# Global symbol name for device definition.
use constant	SYM_BUILTIN_DEV		=> 'builtin_dev';
use constant	SYM_BUILTIN_NDEVS	=> 'builtin_ndevs';
use constant	SYM_BUILTIN_UART_PORT	=> 'builtin_uart_port';
use constant	SYM_BUILTIN_UART_NPORTS	=> 'builtin_uart_nports';

# Type of built-in device information.
use constant	TYPE_BUILTIN_DEV	=> 'builtin_dev_t';

sub isEmptyString($)
{
	my ($str) = @_;

	return 1 unless ($str);
	return if ($str =~ /^\s+$/o);

	my $c = substr($str, 0, 1);
	$c = ord($c);
	return ($c == 0) ? 1 : undef;
}

@EXPORT = qw(CPP OBJ INT_MIN UINT_MAX CHAR_MIN UCHAR_MAX C_NULL MAX_NPROPS
	     MAX_NODENAMELEN PROP_REG PROP_INTERRUPTS
	     SYM_INTERNAL_PREFIX SYM_OBJECT_PREFIX SYM_BUILTIN_DEV
	     SYM_BUILTIN_NDEVS SYM_BUILTIN_UART_PORT SYM_BUILTIN_UART_NPORTS
	     TYPE_BUILTIN_DEV isEmptyString);

1;
