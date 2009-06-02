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
## Warm boot digest section.
##

=for comment

typedef struct wb_digest {
	uint16_t	wbd_type;	/* digtest type */
	uint16_t	wbd_flags;	/* flags */
	uint32_t	wbd_value;	/* digest value */
	uint32_t	wbd_paddr;	/* physical address to be verified */
	uint32_t	wbd_size;	/* size of area to be verified */
} wb_digest_t;

=cut

package XRamConf::Struct::WbDigest;

use strict;
use vars qw(@ISA @EXPORT %TEMPLATES @MEMBERS %TYPESTR);

use Exporter;
use XRamConf::Struct;
use XRamConf::Struct::Member;

use constant	WBDT_CRC32	=> 1;
use constant	WBDT_CHECKSUM	=> 2;

@ISA = qw(XRamConf::Struct Exporter);
@MEMBERS = qw(wbd_type wbd_flags wbd_value wbd_paddr wbd_size);
%TEMPLATES = (wbd_type	=> $XRamConf::Struct::Member::UINT16,
	      wbd_flags	=> $XRamConf::Struct::Member::UINT16,
	      wbd_value	=> $XRamConf::Struct::Member::UINT32,
	      wbd_paddr	=> $XRamConf::Struct::Member::UINT32,
	      wbd_size	=> $XRamConf::Struct::Member::UINT32);
%TYPESTR = (WBDT_CRC32()	=> 'crc32',
	    WBDT_CHECKSUM()	=> 'cksum');

@EXPORT = qw(WBDT_CRC32 WBDT_CHECKSUM);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my (%values) = @_;

	my (%arg) = (NAME => 'wb_digest', TEMPLATES => \%TEMPLATES,
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

	my $type = $me->get('wbd_type');
	my $typestr = $TYPESTR{$type} || 'unknown';
	$me->printValue($fh, 'type', sprintf("%d (%s)", $type, $typestr));
	$me->printValue($fh, 'flags', sprintf("0x%x", $me->get('wbd_flags')));
	$me->printValue($fh, 'value', sprintf("0x%x", $me->get('wbd_value')));
	$me->printValue($fh, 'paddr', sprintf("0x%x", $me->get('wbd_paddr')));
	$me->printValue($fh, 'size', sprintf("0x%x", $me->get('wbd_size')));
}

1;
