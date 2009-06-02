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
## Utility to manipulate Kernel image (ARM specific)
##

package XRamConf::Kernel::arm;

use strict;
use FileHandle;
use POSIX;

use XRamConf::Constants;
use XRamConf::Constants::arm;
use XRamConf::Elf;

# Adjust base address of data section so that we can use large pagesize.
# DATA_ALIGN must be a power of 2.
use constant	DATA_ALIGN		=> 0x100000;

use constant	KERNEL_MINSIZE		=> 0x1000000;	# 16M

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $elf = XRamConf::Elf->new($file);
	my $ptrsz = $elf->getPointerSize();
	die "Unexpected pointer size suffix: $ptrsz\n" unless ($ptrsz eq '/w');

	my $me = bless {ELF => $elf}, $class;
	return $me;
}

sub relocate
{
	my $me = shift;

	my $elf = $me->{ELF};
	my $file = $elf->getFile();

	# Nothing to do if no xramfs device is configured.
	my $config = $me->fetchConfig();
	my $userconfig = $me->fetchUserConfig();
	return unless (defined($config) or defined($userconfig));

	# Read symbols from kernel.
	my $etextkey = '$.text';
	my $sbsskey = '^.bss';
	my $results = $elf->get(SYM_KERNELBASE, SYM_KERNELPHYSBASE, $etextkey,
				SYM_CTF_END_ADDRESS, $sbsskey,
				SYM_BACKUP_DRAM_BASE, SYM_BACKUP_DRAM_SIZE,
				SYM_PAGEOFFSET, SYM_PAGEMASK, SYM_PAGESHIFT);
	my $pageoffset = $results->{SYM_PAGEOFFSET()};
	my $pagemask = $results->{SYM_PAGEMASK()};
	my $pageshift = $results->{SYM_PAGESHIFT()};
	my $dram_base = $results->{SYM_BACKUP_DRAM_BASE()};
	my $dram_size = $results->{SYM_BACKUP_DRAM_SIZE()};
	my $dram_end = $dram_base + $dram_size;
	my $kernelphysbase = $results->{SYM_KERNELPHYSBASE()};
	my $textend = $results->{$etextkey};
	my $data_paddr_base = $kernelphysbase + $textend;

	my $data_end = $results->{SYM_CTF_END_ADDRESS()};
	$data_end = ($kernelphysbase + $results->{$sbsskey})
		unless ($data_end);

	# Whole kernel image must be located in backup DRAM.
	die "Can't locate kernel within backup DRAM.\n"
		unless ($kernelphysbase >= $dram_base and
			$data_end <= $dram_end);

	# Reserve memory for device usage.
	# Note that xramfs device pages must be located at the highest
	# address in the reserved area.
	my $xrambase = ($data_end + $pageoffset) & $pagemask;
	my $pfn = $xrambase >> $pageshift;

	# Reserve memory for user backup data.
	my $endpfn = $dram_end >> $pageshift;
	my ($usegcnt, $ufoff, $usegaddr) = $me->userdata();
	my (%sargs, @xmemdev);
	foreach my $ucf (@$userconfig) {
		my $epfn = $pfn + $ucf->{count};
		if ($epfn > $endpfn) {
			my $err = sprintf("userdata[%s] can't be located " .
					  "within backup DRAM: 0x%lx-0x%lx\n",
					  $ucf->{name}, $pfn, $epfn);
			die $err;
		}
		$ucf->{base} = $pfn;
		my $boff = sprintf("0x%lx", $usegaddr + NS_BASE);
		$sargs{$boff} = $pfn;
		$usegaddr += SIZEOF_NVDSEG;
		$pfn = $epfn;
		push(@xmemdev, $ucf);
	}

	my $xrampgbase = $pfn << $pageshift;
	my ($segcnt, $foff, $segaddr) = $me->xramdev();
	foreach my $cf (@$config) {
		my $epfn = $pfn + $cf->{count};
		if ($epfn > $endpfn) {
			my $err = sprintf("xramdev[%s] can't be located " .
					  "within backup DRAM: 0x%lx-0x%lx\n",
					  $cf->{name}, $pfn, $epfn);
			die $err;
		}
		$cf->{base} = $pfn;
		my $boff = sprintf("0x%lx", $segaddr + XS_BASE);
		$sargs{$boff} = $pfn;
		$segaddr += SIZEOF_XDSEG;
		$pfn = $epfn;
		push(@xmemdev, $cf);
	}

	# Determine start address of data section.
	my $xramend = $pfn << $pageshift;
	my $data_paddr = $xramend;
	my $baseoff = $data_paddr_base & (DATA_ALIGN - 1);
	my $doff = $data_paddr & (DATA_ALIGN - 1);
	$data_paddr += DATA_ALIGN unless ($baseoff >= $doff);
	$data_paddr = ($data_paddr & ~(DATA_ALIGN - 1)) | $baseoff;

	# Check whether the kernel data can be located just after xramdev area.
	my (@memlist) = $me->memlist();
	my $reloc;
	my $textsize = $data_paddr_base - $kernelphysbase;
	my $minsize = KERNEL_MINSIZE - $textsize;
	foreach my $mlp (@memlist) {
		my $start = $mlp->{address};
		my $end = $start + $mlp->{size};
		if ($xramend >= $start and $xramend <= $end) {
			my $dsz = $end - $data_paddr;
			$reloc = ($dsz < $minsize);
			last;
		}
	}
	die "Can't detect SDRAM bank where the xramdev device is located.\n"
		unless (defined($reloc));
	if ($reloc) {
		# The kernel data can't locate just after xramdev area.
		undef $data_paddr;
		foreach my $mlp (@memlist) {
			my $start = $mlp->{address};
			my $size = $mlp->{size};
			my $end = $start + $size;

			next if (($xramend >= $start and $xramend <= $end) or
				 $size < $minsize);

			my $dpaddr = $start;
			my $doff = $dpaddr & (DATA_ALIGN - 1);
			$dpaddr += DATA_ALIGN unless ($baseoff >= $doff);
			$dpaddr = ($dpaddr & ~(DATA_ALIGN - 1)) | $baseoff;
			my $dsz = $end - $dpaddr;
			if ($dsz >= $minsize) {
				$data_paddr = $dpaddr;
				last;
			}
		}
		die "The kernel data can't be relocated.\n"
			unless (defined($data_paddr));
	}

	# Create B-tree device index by base PFN.
	# We assume that devices in @xmemdev are already sorted by base PFN
	# in ascending order.
	my $rootaddr = $me->makeIndex(\@xmemdev, \%sargs);
	$sargs{'&' . SYM_XMEMDEV_ROOT()} = $rootaddr;

	# Patch address layout for xramfs devices.
	$sargs{'&' . SYM_DATA_PADDR_BASE()} = $data_paddr_base;
	$sargs{'&' . SYM_DATA_PADDR()} = $data_paddr;
	$sargs{'&' . SYM_XRAMDEV_START_PADDR()} = $xrambase;
	$sargs{'&' . SYM_XRAMDEV_END_PADDR()} = $xramend;
	$sargs{'&' . SYM_XRAMDEV_PAGESTART_PADDR()} = $xrampgbase;

	$elf->set(%sargs);
}

sub makeIndex
{
	my $me = shift;
	my ($list, $sargs) = @_;

	my $cnt = scalar(@$list);
	return 0 unless ($cnt);
	return $list->[0]->{_segaddr} if ($cnt == 1);	# Leaf node

	my $halfidx = $cnt >> 1;
	my $lend = $halfidx - 1;
	my $hstart = $halfidx + 1;
	my $hend = $cnt - 1;
	my $half = $list->[$halfidx];
	my (@lower) = @$list[0 .. $lend];
	my (@higher) = @$list[$hstart .. $hend];
	my $laddr = $me->makeIndex(\@lower, $sargs);
	my $haddr = $me->makeIndex(\@higher, $sargs);

	my $halfaddr = $half->{_segaddr};
	my $loff = sprintf("0x%lx", $halfaddr + XD_LOWER);
	my $hoff = sprintf("0x%lx", $halfaddr + XD_HIGHER);
	$sargs->{$loff} = $laddr;
	$sargs->{$hoff} = $haddr;

	return $halfaddr;
}

sub dumpIndex
{
	my $me = shift;
	my ($out) = @_;

	$out = \*STDOUT unless ($out);
	my $elf = $me->{ELF};
	my $results = $elf->get(SYM_XMEMDEV_ROOT);
	my $rootaddr = $results->{SYM_XMEMDEV_ROOT()};
	return unless ($rootaddr);

	# Fetch all configurations.
	my $config = $me->fetchConfig();
	my $userconfig = $me->fetchUserConfig();
	return unless (defined($config) or defined($userconfig));

	my (%xmemdev) = map {$_->{_segaddr} => $_} (@$config, @$userconfig);
	$me->_dumpIndex($out, $rootaddr, \%xmemdev, 0, 'root');
}

sub _dumpIndex
{
	my $me = shift;
	my ($out, $addr, $xmemdev, $level, $prefix) = @_;

	return unless ($addr);
	my $cf = $xmemdev->{$addr};
	unless ($cf) {
		my $va = sprintf("0x%lx", $addr);
		die "xmemdev at $va is not found\n";
	}
	my $base = $cf->{base};
	my $count = $cf->{count};
	$out->printf("%s%s:0x%x [0x%x, 0x%x)\n", ' ' x ($level << 2),
		     $prefix, $addr, $base, $base + $count);
	my $lower = $cf->{lower};
	my $higher = $cf->{higher};
	my $clevel = $level + 1;
	$me->_dumpIndex($out, $lower, $xmemdev, $clevel, 'lower')
		if ($lower);
	$me->_dumpIndex($out, $higher, $xmemdev, $clevel, 'higher')
		if ($higher);
}

sub xramdev
{
	my $me = shift;

	my ($segcnt, $off, $addr);
	if (defined($me->{XRAMDEV_SEGCNT})) {
		$off = $me->{XRAMDEV_OFF};
		$segcnt = $me->{XRAMDEV_SEGCNT};
		$addr = $me->{XRAMDEV_SEGS};
	}
	else {
		my $elf = $me->{ELF};

		$elf->quiet(1);
		my $results;
		eval {
			$results = $elf->get(SYM_XRAMDEV_SEGCNT);
		};
		$elf->quiet(0);
		return if ($@);

		$segcnt = $results->{SYM_XRAMDEV_SEGCNT()};
		$me->{XRAMDEV_SEGCNT} = $segcnt;
		if ($segcnt > 0) {
			my $offkey = '%' . SYM_XRAMDEV_SEGS;
			my $addrkey = '&' . SYM_XRAMDEV_SEGS;
			$results = $elf->get($offkey, $addrkey);
			$addr = $results->{$addrkey};
			$off = $results->{$offkey};
			$me->{XRAMDEV_OFF} = $off;
			$me->{XRAMDEV_SEGS} = $addr
		}
	}

	return ($segcnt, $off, $addr);
}

sub fetchConfig
{
	my $me = shift;

	my @config;
	my $elf = $me->{ELF};

	# Fetch symbols related to xramdev configuration.
	my ($segcnt, $off, $segaddr) = $me->xramdev();
	return undef unless (defined($segcnt));

	if ($segcnt > 0) {
		my $sz = SIZEOF_XDSEG * $segcnt;
		my $file = $elf->getFile();
		my $fh = FileHandle->new($file) or
			die "open($file) failed: $!\n";
		my $data;

		# Read contents of xramdev_segs array.
		$fh->seek($off, SEEK_SET) or
			die "lseek($file, $off) failed: $!\n";
		$fh->read($data, $sz) or die "read($file) failed: $!\n";

		my (@elem) = unpack("L*", $data);
		my (@nmargs);
		while (@elem) {
			my %cf;

			$cf{_segaddr} = $segaddr;
			$cf{base} = shift(@elem);
			$cf{count} = shift(@elem);
			$cf{attr} = shift(@elem);
			$cf{lower} = shift(@elem);
			$cf{higher} = shift(@elem);
			$cf{flags} = shift(@elem);
			my $nameptr = shift(@elem);
			$cf{vaddr} = shift(@elem);
			push(@config, \%cf);

			push(@nmargs, sprintf("%%0x%lx", $nameptr));
			$segaddr += SIZEOF_XDSEG;
		}

		# Need to dereference string pointer.
		my $nmoff = $elf->get(@nmargs);
		for (my $i = 0; $i < scalar(@nmargs); $i++) {
			my $noff = $nmoff->{$nmargs[$i]};

			my $str = $me->fetchString($fh, $noff);
			$config[$i]->{name} = $str;
		}
	}

	return (wantarray) ? @config : \@config;
}

sub userdata
{
	my $me = shift;

	my ($segcnt, $off, $addr);
	if (defined($me->{NVDRAM_SEGCNT})) {
		$off = $me->{NVDRAM_OFF};
		$segcnt = $me->{NVDRAM_SEGCNT};
		$addr = $me->{NVDRAM_SEGS};
	}
	else {
		my $elf = $me->{ELF};

		$elf->quiet(1);
		my $results;
		eval {
			$results = $elf->get(SYM_NVDRAM_SEGCNT);
		};
		$elf->quiet(0);
		return if ($@);

		$segcnt = $results->{SYM_NVDRAM_SEGCNT()};
		$me->{NVDRAM_SEGCNT} = $segcnt;
		if ($segcnt > 0) {
			my $offkey = '%' . SYM_NVDRAM_SEGS;
			my $addrkey = '&' . SYM_NVDRAM_SEGS;
			$results = $elf->get($offkey, $addrkey);
			$addr = $results->{$addrkey};
			$off = $results->{$offkey};
			$me->{NVDRAM_OFF} = $off;
			$me->{NVDRAM_SEGS} = $addr
		}
	}

	return ($segcnt, $off, $addr);
}

sub fetchUserConfig
{
	my $me = shift;

	my @config;
	my $elf = $me->{ELF};

	# Fetch symbols related to userdata configuration.
	my ($segcnt, $off, $segaddr) = $me->userdata();
	return undef unless (defined($segcnt));

	if ($segcnt > 0) {
		my $sz = SIZEOF_NVDSEG * $segcnt;
		my $file = $elf->getFile();
		my $fh = FileHandle->new($file) or
			die "open($file) failed: $!\n";
		my $data;

		# Read contents of nvdram_segs array.
		$fh->seek($off, SEEK_SET) or
			die "lseek($file, $off) failed: $!\n";
		$fh->read($data, $sz) or die "read($file) failed: $!\n";

		my (@elem) = unpack("L*", $data);
		my $idx = 0;
		my (@strargs, %strkeys);
		my $namefunc = sub {
			my ($i, $key, $ptr) = @_;

			my $k = sprintf("%%0x%lx", $ptr);
			my $map;
			if (defined($strkeys{$i})) {
				$map = $strkeys{$i};
			}
			else {
				$map = {};
				$strkeys{$i} = $map;
			}

			push(@strargs, $k);
			$map->{$key} = $k;
		};
		while (@elem) {
			my %cf;

			$cf{_segaddr} = $segaddr;
			$cf{base} = shift(@elem);
			$cf{count} = shift(@elem);
			$cf{attr} = shift(@elem);
			$cf{lower} = shift(@elem);
			$cf{higher} = shift(@elem);
			$cf{mode} = shift(@elem);
			$cf{flags} = shift(@elem);
			$cf{uid} = shift(@elem);
			$cf{gid} = shift(@elem);
			my $nameptr = shift(@elem);
			my $rprivptr = shift(@elem);
			my $wprivptr = shift(@elem);
			push(@config, \%cf);

			&$namefunc($idx, 'name', $nameptr);
			&$namefunc($idx, 'rpriv', $rprivptr) if ($rprivptr);
			&$namefunc($idx, 'wpriv', $wprivptr) if ($wprivptr);
			$segaddr += SIZEOF_NVDSEG;
		}
		continue {
			$idx++;
		}

		# Need to dereference string pointer.
		my $stroff = $elf->get(@strargs);
		foreach my $i (keys %strkeys) {
			my $map = $strkeys{$i};
			foreach my $key (keys %$map) {
				my $offkey = $map->{$key};
				my $off = $stroff->{$offkey};

				my $str = $me->fetchString($fh, $off);
				$config[$i]->{$key} = $str;
			}
		}
	}

	return (wantarray) ? @config : \@config;
}

sub fetchString
{
	my $me = shift;
	my ($fh, $off) = @_;

	unless ($fh->seek($off, SEEK_SET)) {
		my $file = $me->{ELF}->getFile();
		die "lseek($file, $off) failed: $!\n";
	}

	my $str;

	LOOP:
	while (1) {
		my $data;
		unless ($fh->read($data, 4)) {
			my $file = $me->{ELF}->getFile();
			die "read($file) failed: $!\n";
		}

		my (@elem) = unpack("C*", $data);
		foreach my $c (@elem) {
			last LOOP unless ($c);

			$str .= chr($c);
		}
	}

	return $str;
}

sub getElf
{
	my $me = shift;

	return $me->{ELF};
}

sub memlist
{
	my $me = shift;

	my $elf = $me->{ELF};
	my $file = $elf->getFile();
	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";

	my $results = $elf->get(SYM_ARMPF_BOOT_MEMLIST);
	my $mlp = $results->{SYM_ARMPF_BOOT_MEMLIST()};
	my (@mlist);

	while ($mlp) {
		my $offkey = '%' . $mlp;
		$results = $elf->get($offkey);
		my $off = $results->{$offkey};
		my $data;

		# Read contents of memlist.
		$fh->seek($off, SEEK_SET) or
			die "lseek($file, $off) failed: $!\n";
		$fh->read($data, SIZEOF_MEMLIST) or
			die "read($file) failed: $!\n";
		my (@elem) = unpack("Q2L2", $data);
		my %ml;
		$ml{'address'} = shift(@elem);
		$ml{'size'} = shift(@elem);
		$mlp = $ml{'next'} = shift(@elem);
		$ml{'prev'} = shift(@elem);
		push(@mlist, \%ml);
	}
	die "Failed to derive memlist from the kernel.\n" unless (@mlist);

	return (wantarray) ? @mlist : \@mlist;
}

1;
