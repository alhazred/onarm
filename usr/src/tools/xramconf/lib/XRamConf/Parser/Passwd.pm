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
## password file parser
##

package XRamConf::Parser::Passwd;

use strict;

use FileHandle;

# Default password file in the source tree. ($SRC relative)
use constant	DEFAULT_PASSWD_FILE	=> 'cmd/Adm/sun/passwd';

use constant	PASSWD_NELEMS		=> 7;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $me = bless {PASSWD => {}}, $class;
	$file = $me->defaultFile() unless ($file);
	$me->{FILE} = $file;
	$me->parse($file);

	return $me;
}

sub file
{
	my $me = shift;

	return $me->{FILE};
}

sub defaultFile
{
	my $me = shift;

	my $file;
	my $src = $ENV{SRC};
	$file = $src . '/' . DEFAULT_PASSWD_FILE if ($src);

	return $file;
}

sub parse
{
	my $me = shift;
	my ($file) = @_;

	my $fh = FileHandle->new($file) or return;
	my $passwd = $me->{PASSWD};

	while (<$fh>) {
		chomp;
		my (@line) = split(/:/, $_, PASSWD_NELEMS);
		my $name = $line[0];
		my $uid = $line[2];
		next unless (defined($name) and defined($uid));

		$passwd->{$name} = $uid;
	}
}

sub getId
{
	my $me = shift;
	my ($name) = @_;

	return $me->{PASSWD}->{$name};
}

1;
