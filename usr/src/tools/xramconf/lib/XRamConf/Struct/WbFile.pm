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
## Warm boot file section.
##

=for comment

/* Max length of filename (including terminator) */
#define	WBFILE_MAXNAME		32

typedef struct wb_file {
	uint32_t	wbf_paddr;	/* physical address to be loaded */
	uint32_t	wbf_size;	/* size of load area */
	char		wbf_name[WBFILE_MAXNAME];	/* filename */
} wb_file_t;

=cut

package XRamConf::Struct::WbFile;

use strict;
use vars qw(@ISA %TEMPLATES @MEMBERS);

use XRamConf::Constants;
use XRamConf::Struct;
use XRamConf::Struct::Member;
use XRamConf::Struct::StringMember;

@ISA = qw(XRamConf::Struct);
@MEMBERS = qw(wbf_paddr wbf_size wbf_name);
%TEMPLATES = (wbf_paddr	=> $XRamConf::Struct::Member::UINT32,
	      wbf_size	=> $XRamConf::Struct::Member::UINT32,
	      wbf_name	=> XRamConf::Struct::StringMember->
	      new(XRAMDEV_MAX_FILE_NAMELEN));

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%values) = @_;

	my (%arg) = (NAME => 'wb_file', TEMPLATES => \%TEMPLATES,
		     MEMBERS => \@MEMBERS, VALUES => \%values);
	my $me = $class->SUPER::new(%arg);

	return bless $me, $class;
}

sub printValue
{
	my $me = shift;
	my ($fh, $label, $value) = @_;

	$fh->printf("    %-22s:  %s\n", $label, $value);
}

sub print
{
	my $me = shift;
	my ($fh) = @_;

	$me->printValue($fh, 'paddr', sprintf("0x%x", $me->get('wbf_paddr')));
	$me->printValue($fh, 'size', sprintf("0x%x", $me->get('wbf_size')));
	$me->printValue($fh, 'filename', $me->get('wbf_name'));
}

1;
