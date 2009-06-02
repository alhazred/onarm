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
## Base class for warm boot command section.
##

=for comment

#define	WBCMD_SIZE		32
#define	WBCMD_PADSIZE		(WBCMD_SIZE - 4)

typedef struct wb_cmd {
	uint16_t	wbc_type;		/* command type */
	uint16_t	wbc_flags;		/* flags for this command */
	union {
		/* Copy memory (WBCT_COPY) */
		struct {
			uint32_t	wbcc_from;	/* source address */
			uint32_t	wbcc_to;	/* dest address */
			uint32_t	wbcc_size;	/* size */
		} wbu_copy;
		uint8_t	wbu_pad[WBCMD_PADSIZE];
	} wbc_un;
} wb_cmd_t;

=cut

package XRamConf::Struct::WbCmd;

use strict;
use vars qw(@ISA @EXPORT %TEMPLATES %MEMBERS @MEMBERS_COMMON
	     %TEMPLATES_COMMON);

use XRamConf::Struct;
use XRamConf::Struct::Member;

use constant	WBCT_COPY	=> 1;
use constant	WBCMD_SIZE	=> 32;

use constant	WBCF_POST_CBOOT	=> 0x0001;
use constant	WBCF_PRE_ENTRY	=> 0x0002;

@ISA = qw(XRamConf::Struct Exporter);

@MEMBERS_COMMON = qw(wbc_type wbc_flags);
%MEMBERS = (WBCT_COPY() => [@MEMBERS_COMMON, 'wbcc_from', 'wbcc_to',
			    'wbcc_size']);

%TEMPLATES_COMMON = (wbc_type	=> $XRamConf::Struct::Member::UINT16,
		     wbc_flags	=> $XRamConf::Struct::Member::UINT16);
%TEMPLATES = (WBCT_COPY() =>
	      {%TEMPLATES_COMMON,
	       wbcc_from	=> $XRamConf::Struct::Member::UINT32,
	       wbcc_to		=> $XRamConf::Struct::Member::UINT32,
	       wbcc_size	=> $XRamConf::Struct::Member::UINT32});

@EXPORT = qw(WBCT_COPY WBCF_POST_CBOOT WBCF_PRE_ENTRY);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($type) = @_;

	my $templates = \%TEMPLATES_COMMON;
	my $members = \@MEMBERS_COMMON;
	my $me = $class->SUPER::new(NAME => 'wb_cmd', TEMPLATES => $templates,
				    MEMBERS => $members);
	$me = bless $me, $class;
	$me->set('wbc_type', $type) if (defined($type));

	return $me;
}

sub set
{
	my $me = shift;
	my ($name, $value) = @_;

	$me->SUPER::set($name, $value);

	if ($name eq 'wbc_type') {
		my $templates = $TEMPLATES{$value};
		my $members = $MEMBERS{$value};
		die "Unknown command type: $value\n"
			unless ($templates and $members);
		$me->{TEMPLATES} = $templates;
		$me->{MEMBERS} = $members;
	}
}

sub getSize
{
	return WBCMD_SIZE;
}

sub getAlignment
{
	return $XRamConf::Struct::Member::UINT32->getAlignment();
}

sub decode
{
	my $me = shift;
	my ($order, $raw) = @_;

	# At first, determine command type.
	my $tmpl = $TEMPLATES_COMMON{'wbc_type'};
	my ($type) = $tmpl->decode($order, $raw);
	$me->set('wbc_type', $type);

	$me->SUPER::decode($order, $raw);
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

	my $type = $me->get('wbc_type');
	my $typestr = ($type == WBCT_COPY) ? 'COPY' : 'unknown';
	$me->printValue($fh, 'type', sprintf("%d (%s)", $type, $typestr));

	my @flg;
	my $flags = $me->get('wbc_flags');
	push(@flg, 'Post Cold Boot') if ($flags & WBCF_POST_CBOOT);
	push(@flg, 'Pre Entry') if ($flags & WBCF_PRE_ENTRY);
	my $flagstr = sprintf("0x%x", $flags);
	$flagstr .= ' (' . join(', ', @flg) . ')' if (@flg);
	$me->printValue($fh, 'flags', $flagstr);

	if ($type == WBCT_COPY) {
		$me->printValue($fh, 'from',
				sprintf("0x%x", $me->get('wbcc_from')));
		$me->printValue($fh, 'to',
				sprintf("0x%x", $me->get('wbcc_to')));
		$me->printValue($fh, 'size',
				sprintf("0x%x", $me->get('wbcc_size')));
	}
}

1;
