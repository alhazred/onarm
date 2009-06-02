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
## Warm boot information header.
##

=for comment

typedef struct wb_info {
	uint8_t		wb_ident[WBI_NIDENT];	/* ident bytes */
	uint16_t	wb_size;		/* size (including wb_info) */
	uint16_t	wb_fentsize;		/* number of files */
	uint16_t	wb_dgentsize;		/* number of digests */
	uint16_t	wb_cmdentsize;		/* number of cmds */
	uint16_t	wb_fileoff;		/* offset of files */
	uint16_t	wb_dgoff;		/* offset of digests */
	uint16_t	wb_cmdoff;		/* offset of cmds */
	uint32_t	wb_entry;		/* start address */
} wb_info_t;

=cut

package XRamConf::Struct::WbInfo;

use strict;
use vars qw(@ISA %TEMPLATES @MEMBERS);

use File::Basename;
use XRamConf::ByteOrder;
use XRamConf::Constants;
use XRamConf::Struct;
use XRamConf::Struct::Member;
use XRamConf::Struct::WbFile;
use XRamConf::Struct::WbDigest;
use XRamConf::Struct::WbCmd;

use constant	WBI_NIDENT	=> 6;

use constant	WBI_MAGIC0	=> 0;
use constant	WBI_MAGIC1	=> 1;
use constant	WBI_MAGIC2	=> 2;
use constant	WBI_MAGIC3	=> 3;
use constant	WBI_VERSION	=> 4;
use constant	WBI_FLAGS	=> 5;

use constant	WBINFO_MAGIC0	=> 0x99;
use constant	WBINFO_MAGIC1	=> ord('W');
use constant	WBINFO_MAGIC2	=> ord('B');
use constant	WBINFO_MAGIC3	=> ord('T');

use constant	WBINFO_CURRENT_VERSION	=> 1;

use constant	WBF_BIG_ENDIAN	=> 0x01;

@ISA = qw(XRamConf::Struct Exporter);
@MEMBERS = qw(wb_ident wb_size wb_fentsize wb_dgentsize wb_cmdentsize
	      wb_fileoff wb_dgoff wb_cmdoff wb_entry);
%TEMPLATES = (wb_ident		=> XRamConf::Struct::Member->
	      new(size => 1, unsigned => 1, elements => WBI_NIDENT),
	      wb_size		=> $XRamConf::Struct::Member::UINT16,
	      wb_fentsize	=> $XRamConf::Struct::Member::UINT16,
	      wb_dgentsize	=> $XRamConf::Struct::Member::UINT16,
	      wb_cmdentsize	=> $XRamConf::Struct::Member::UINT16,
	      wb_fileoff	=> $XRamConf::Struct::Member::UINT16,
	      wb_dgoff		=> $XRamConf::Struct::Member::UINT16,
	      wb_cmdoff		=> $XRamConf::Struct::Member::UINT16,
	      wb_entry		=> $XRamConf::Struct::Member::UINT32);

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	my $me = $class->SUPER::new(NAME => 'wb_info',
				    TEMPLATES => \%TEMPLATES,
				    MEMBERS => \@MEMBERS);
	$me->{_FILES} = [];
	$me->{_DIGESTS} = [];
	$me->{_COMMANDS} = [];

	return bless $me, $class;
}

sub addFile
{
	my $me = shift;
	my ($name, $paddr, $size) = @_;

	$name = basename($name);
	my $file = XRamConf::Struct::WbFile->new(wbf_paddr => $paddr,
						 wbf_size => $size);

	# Check length of filename.
	die "Too long filename: $name\n"
		if (length($name) >= XRAMDEV_MAX_NAMELEN);
	$file->set('wbf_name', $name);

	push(@{$me->{_FILES}}, $file);
}

sub addDigest
{
	my $me = shift;
	my ($type, $flags, $paddr, $size, $digest) = @_;

	my $digest = XRamConf::Struct::WbDigest->new(wbd_type => $type,
						     wbd_flags => $flags,
						     wbd_value => $digest,
						     wbd_paddr => $paddr,
						     wbd_size => $size);
	push(@{$me->{_DIGESTS}}, $digest);
}

sub addCommand
{
	my $me = shift;
	my ($cmd) = @_;

	push(@{$me->{_COMMANDS}}, $cmd);
}

#
# Setup ident bytes.
#
sub setIdent
{
	my $me = shift;
	my ($version, $order) = @_;

	my $ident = $me->get('wb_ident');
	$ident->[WBI_MAGIC0()] = WBINFO_MAGIC0;
	$ident->[WBI_MAGIC1()] = WBINFO_MAGIC1;
	$ident->[WBI_MAGIC2()] = WBINFO_MAGIC2;
	$ident->[WBI_MAGIC3()] = WBINFO_MAGIC3;
	$ident->[WBI_VERSION()] = $version;
	$ident->[WBI_FLAGS()] = WBF_BIG_ENDIAN
		if ($order == BIG_ENDIAN);

	$me->set('wb_ident', $ident);
}

sub relocateSection
{
	my $me = shift;
	my ($list, $startoff, $entkey, $offkey) = @_;

	my ($offset, $size);
	my $cnt = scalar(@$list);
	if ($cnt) {
		my $elem = $list->[0];
		my $align = $elem->getAlignment();
		$offset = ROUNDUP($startoff, $align);
		$size = $elem->getSize() * $cnt;
	}
	else {
		$offset = $startoff;
		$size = 0;
	}

	$me->set($entkey, $cnt) if ($entkey);
	$me->set($offkey, $offset) if ($offkey);

	return ($offset, $size);
}

#
# Encode struct wb_info and subsequent sections.
#
sub encode
{
	my $me = shift;
	my ($order) = @_;

	# Setup default ident bytes.
	my $ident = $me->get('wb_ident', 1);
	$me->setIdent(WBINFO_CURRENT_VERSION, $order)
		unless (defined($ident));

	# Determine section layout.
	my $files = $me->{_FILES};
	my $digests = $me->{_DIGESTS};
	my $commands = $me->{_COMMANDS};
	my $filecnt = scalar(@$files);
	my $dgcnt = scalar(@$digests);
	my $cmdcnt = scalar(@$commands);

	# Determine offset for each section.
	my $offset = $me->getSize();
	my ($fileoff, $fsize) =
		$me->relocateSection($files, $offset, 'wb_fentsize',
				     'wb_fileoff');

	$offset = $fileoff + $fsize;
	my ($dgoff, $dgsize) =
		$me->relocateSection($digests, $offset, 'wb_dgentsize',
				     'wb_dgoff');

	$offset = $dgoff + $dgsize;
	my ($cmdoff, $cmdsize) =
		$me->relocateSection($commands, $offset, 'wb_cmdentsize',
				     'wb_cmdoff');

	# Set whole size of warm boot information.
	$me->set('wb_size', $cmdoff + $cmdsize);

	# Encode header.
	my $raw = $me->SUPER::encode($order);

	# Encode file section.
	$raw .= $me->encodeSection($order, $files, $fileoff) if ($filecnt);

	# Encode digest section.
	$raw .= $me->encodeSection($order, $digests, $dgoff) if ($dgcnt);

	# Encode command section.
	$raw .= $me->encodeSection($order, $commands, $cmdoff) if ($cmdcnt);

	return $raw;
}

sub encodeSection
{
	my $me = shift;
	my ($order, $list, $off) = @_;

	my $raw;
	my $al = $list->[0]->getAlignment();
	my $mod = $off & ($al - 1);
	if ($mod != 0) {
		my $pad = $al - $mod;
		$raw = pack('C' . $pad, 0);
	}

	foreach my $sct (@$list) {
		$raw .= $sct->encode($order);
	}

	return $raw;
}

#
# Decode struct wb_info and subsequent sections.
#
sub decode
{
	my $me = shift;
	my ($raw) = @_;

	$me->{_FILES} = [];
	$me->{_DIGESTS} = [];
	$me->{_COMMANDS} = [];

	# Check ident bytes.
	my (@ident) = unpack('C' . WBI_NIDENT, $raw);
	my (@magic) = (WBINFO_MAGIC0, WBINFO_MAGIC1,
		       WBINFO_MAGIC2, WBINFO_MAGIC3);
	for (my $i = 0; $i < scalar(@magic); $i++) {
		unless ($ident[$i] == $magic[$i]) {
			my $msg = sprintf("Bad magic[%d]: 0x%02x, 0x%02x\n",
					  $i, $ident[$i], $magic[$i]);
			die $msg;
		}
	}

	my $order = ($ident[WBI_FLAGS()] & WBF_BIG_ENDIAN)
		? BIG_ENDIAN : LITTLE_ENDIAN;

	$me->SUPER::decode($order, $raw);

	# Decode file section.
	my $nfiles = $me->get('wb_fentsize');
	my $fileoff = $me->get('wb_fileoff');
	$me->decodeSection($order, XRamConf::Struct::WbFile->new(), $fileoff,
			   $nfiles, '_FILES', $raw);

	# Decode digest section.
	my $ndgs = $me->get('wb_dgentsize');
	my $dgoff = $me->get('wb_dgoff');
	$me->decodeSection($order, XRamConf::Struct::WbDigest->new(), $dgoff,
			   $ndgs, '_DIGESTS', $raw);

	# Decode command section.
	my $ncmds = $me->get('wb_cmdentsize');
	my $cmdoff = $me->get('wb_cmdoff');
	$me->decodeSection($order, XRamConf::Struct::WbCmd->new(), $cmdoff,
			   $ncmds, '_COMMANDS', $raw);
}

sub decodeSection
{
	my $me = shift;
	my ($order, $obj, $off, $nent, $key, $data) = @_;

	my $size = $obj->getSize();
	for (my $i = 0; $i < $nent; $i++, $off += $size) {
		my $raw = substr($data, $off, $size);
		my $sct = $obj->new();
		$sct->decode($order, $raw);
		push(@{$me->{$key}}, $sct);
	}
}

sub printValue
{
	my $me = shift;
	my ($fh, $label, $value) = @_;

	$fh->printf("  %-24s:  %s\n", $label, $value);
}

sub print
{
	my $me = shift;
	my ($fh) = @_;

	my $ident = $me->get('wb_ident');
	my $version = $ident->[WBI_VERSION()];
	my $flags = $ident->[WBI_FLAGS()];

	$fh->print("=== Warm Boot Information Header\n");
	$me->printValue($fh, 'Version', $version);
	my @flg;
	my $sflags = sprintf("0x%x", $flags);
	push(@flg, 'big endian') if ($flags & WBF_BIG_ENDIAN);
	$sflags .= sprintf(" (%s)", join(', ', @flg)) if (@flg);
	$me->printValue($fh, 'Flags', $sflags);

	$me->printValue($fh, 'Infomation size', $me->get('wb_size'));
	$me->printValue($fh, 'Number of files', $me->get('wb_fentsize'));
	$me->printValue($fh, 'Number of digests', $me->get('wb_dgentsize'));
	$me->printValue($fh, 'Number of commands', $me->get('wb_cmdentsize'));
	$me->printValue($fh, 'File section offset',
		  sprintf("0x%x", $me->get('wb_fileoff')));
	$me->printValue($fh, 'Digest section offset',
		  sprintf("0x%x", $me->get('wb_dgoff')));
	$me->printValue($fh, 'Command section offset',
		  sprintf("0x%x", $me->get('wb_cmdoff')));
	$me->printValue($fh, 'Start address',
			sprintf("0x%x", $me->get('wb_entry')));

	# Print file section.
	$me->printSection($fh, '_FILES', 'File Section');

	# Print digest section.
	$me->printSection($fh, '_DIGESTS', 'Digest Section');

	# Print command section.
	$me->printSection($fh, '_COMMANDS', 'Command Section');
}

sub printSection
{
	my $me = shift;
	my ($fh, $key, $label) = @_;

	my $list = $me->{$key};
	if (@$list) {
		$fh->print("\n=== $label");
		my $index = 0;
		foreach my $elem (@$list) {
			$fh->printf("\n  --- Index: $index\n");
			$elem->print($fh);
		}
		continue {
			$index++;
		}
	}

}

1;
