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
## group file parser
##

package XRamConf::Parser::Group;

use strict;

use FileHandle;

# Default group file in the source tree. ($SRC relative)
use constant	DEFAULT_GROUP_FILE	=> 'cmd/Adm/group';

use constant	GROUP_NELEMS		=> 4;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $me = bless {GROUP => {}}, $class;
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
	$file = $src . '/' . DEFAULT_GROUP_FILE if ($src);

	return $file;
}

sub parse
{
	my $me = shift;
	my ($file) = @_;

	my $fh = FileHandle->new($file) or return;
	my $group = $me->{GROUP};

	while (<$fh>) {
		chomp;
		my (@line) = split(/:/, $_, GROUP_NELEMS);
		my $name = $line[0];
		my $gid = $line[2];
		next unless (defined($name) and defined($gid));

		$group->{$name} = $gid;
	}
}

sub getId
{
	my $me = shift;
	my ($name) = @_;

	return $me->{GROUP}->{$name};
}

1;
