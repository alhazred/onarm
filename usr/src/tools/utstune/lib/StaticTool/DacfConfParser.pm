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
## dacf.conf parser
##

package StaticTool::DacfConfParser;

use strict;
use vars qw(@ISA %DEVSPEC %OPID %HASHSYM);

use File::Basename;
use StaticTool;
use StaticTool::BindingLexer;
use StaticTool::StructDumper;

@ISA = qw(StaticTool::BindingLexer);

use constant	S_BEGIN		=> 1;

use constant	S_NT_SPEC	=> 2;
use constant	S_NT_EQUALS	=> 3;
use constant	S_NT_DATA	=> 4;

use constant	S_MN_MODNAME	=> 5;
use constant	S_MN_COLON	=> 6;
use constant	S_MN_OPSET	=> 7;

use constant	S_OP_NAME	=> 8;

use constant	S_OPT_OPTION	=> 9;
use constant	S_OPT_COMMA	=> 10;
use constant	S_OPT_END	=> 11;

use constant	S_OPARG_SPEC	=> 12;
use constant	S_OPARG_EQUALS	=> 13;
use constant	S_OPARG_DATA	=> 14;

%DEVSPEC = ("minor-nodetype"	=> 1,
	    "driver-minorname"	=> 1,
	    "device-path"	=> 1);

%OPID	= ("post-attach"	=> 'DACF_OPID_POSTATTACH',
	   "pre-detach"		=> 'DACF_OPID_PREDETACH');

%HASHSYM = ('minor-nodetype:DACF_OPID_POSTATTACH'
	    => 'posta_mntype',
	    'driver-minorname:DACF_OPID_POSTATTACH'
	    => 'posta_mname',
	    'device-path:DACF_OPID_POSTATTACH'
	    => 'posta_devname',
	    'minor-nodetype:DACF_OPID_PREDETACH'
	    => 'pred_mntype',
	    'driver-minorname:DACF_OPID_PREDETACH'
	    => 'pred_mname',
	    'device-path:DACF_OPID_PREDETACH'
	    => 'pred_devname');

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	my $me = $class->SUPER::new(@_);
	my $dummy = {};
	foreach my $sym (values %HASHSYM) {
		$dummy->{$sym} = 1;
	}
	$me->{DUMMY_ENTRY} = $dummy;

	return $me;
}

sub parse
{
	my $me = shift;

	return $me->dummyEntry() if ($me->{EOF});
	my ($nt_spec, $nt_data, $modname, $opset, $opid, $opts, @args,
	    $arg_spec, %arg_map);
	$opts = 0;
	my $state = S_BEGIN;

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken();
		unless (defined($token)) {
			$me->{EOF} = 1;
			last;
		}

		if ($type == $me->SYMBOL) {
			if ($state == S_BEGIN) {
				$me->parseError("Unknown nt_spec: ", $token)
					unless ($DEVSPEC{$token});
				$nt_spec = $token;
				$state = S_NT_SPEC;
			}
			elsif ($state == S_NT_DATA) {
				$modname = $token;
				$state = S_MN_MODNAME;
			}
			elsif ($state == S_MN_MODNAME) {
				$opid = $me->parseOpid($token);

				# Opset is specified without modname.
				$opset = $modname;
				undef $modname;
				$state = S_OP_NAME;
			}
			elsif ($state == S_MN_COLON) {
				$opset = $token;
				$state = S_MN_OPSET;
			}
			elsif ($state == S_MN_OPSET) {
				$opid = $me->parseOpid($token);
				$state = S_OP_NAME;
			}
			elsif ($state == S_OP_NAME) {
				if ($token eq '-') {
					$state = S_OPT_END;
				}
				else {
					$opts = $me->parseOption($token);
					$state = S_OPT_OPTION;
				}
			}
			elsif ($state == S_OPT_COMMA) {
				$opts = $me->parseOption($token);
				$state = S_OPT_OPTION;
			}
			elsif ($state == S_OPT_END or
			       $state == S_OPT_OPTION or
			       $state == S_OPARG_DATA) {
				$arg_spec = $token;
				$state = S_OPARG_SPEC;
			}
			elsif ($state == S_OPARG_EQUALS) {
				my $val = stringfy($token);
				push(@args,
				     $me->parseArgs($arg_spec, $val,
						    \%arg_map));
				$state = S_OPARG_DATA;
			}
			else {
				$me->syntaxError("Unexpected SYMBOL: ",
						 $token);
			}
		}
		elsif ($type == $me->STRING) {
			if ($state == S_NT_EQUALS) {
				my $s = eval_string($token);
				$me->parseError("Device spec is empty.")
					unless ($s);
				$nt_data = $token;
				$state = S_NT_DATA;
			}
			elsif ($state == S_OPARG_EQUALS) {
				push(@args,
				     $me->parseArgs($arg_spec, $token,
						    \%arg_map));
				$state = S_OPARG_DATA;
			}
			else {
				$me->syntaxError("Unexpected STRING: ",
						 $token);
			}
		}
		elsif ($type == $me->EQUAL) {
			if ($state == S_NT_SPEC) {
				$state = S_NT_EQUALS;
			}
			elsif ($state == S_OPARG_SPEC) {
				$state = S_OPARG_EQUALS;
			}
			else {
				$me->syntaxError("Unexpected EQUAL");
			}
		}
		elsif ($type == $me->COMMA) {
			if ($state == S_OPT_OPTION) {
				$state = S_OPT_COMMA;
			}
			else {
				$me->syntaxError("Unexpected COMMA");
			}
		}
		elsif ($type == $me->COLON) {
			if ($state == S_MN_MODNAME) {
				$state = S_MN_COLON;
			}
			else {
				$me->syntaxError("Unexpected COLON");
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
	if ($state == S_OPT_OPTION or $state == S_OPARG_DATA or
	    $state == S_OPT_END) {
		my $key = $nt_spec . ':' . $opid;
		my $hashsym = $HASHSYM{$key};
		$me->parseError("Failed to detect DACF rule hash: ",
				$nt_spec, $opid) unless ($hashsym);
		@ret = ($hashsym, $nt_data, $modname, $opset, $opid,
			$opts, \@args);
		$me->{DUMPED}->{$hashsym} = 1;
	}
	elsif ($state != S_BEGIN) {
		my $err = ($me->{EOF}) ? "EOF" : "NEWLINE";
		$me->syntaxError("Unexpected $err");
	}

	return (@ret) ? @ret : $me->dummyEntry();
}

sub dummyEntry
{
	my $me = shift;

	my $dummy = $me->{DUMMY_ENTRY};
	return () unless ($dummy and %$dummy);

	my $key = (keys %$dummy)[0];
	delete $dummy->{$key};
	return ($key);
}

sub parseOpid
{
	my $me = shift;
	my ($token) = @_;

	my $opid = $OPID{$token};
	$me->parseError("Unknown option ID: ", $token)
		unless (defined($opid));
	return $opid;
}

sub parseOption
{
	my $me = shift;
	my ($token) = @_;

	# Currently, no option is supported.
	$me->parseError("Invalid option: $token");
}

sub parseArgs
{
	my $me = shift;
	my ($spec, $val, $map) = @_;

	# Do NOT allow dupplicates.
	$me->parseError("Duplicated argument spec: $spec")
		if ($map->{$spec});
	$map->{$spec} = 1;
	return [$spec, $val];
}

sub dumpStruct($)
{
	my $me = shift;

	my %hash;
	while (my ($hashsym, @data) = $me->parse()) {
		my $ent = $hash{$hashsym};
		unless ($ent) {
			$ent = [];
			$hash{$hashsym} = $ent;
		}
		push(@$ent, \@data) if (@data);
	}

	my $file = $me->file();
	my $fname = basename($file);
	print <<OUT;

/* Embedded $fname */
OUT

	my $dumper = StaticTool::StructDumper->new();
	foreach my $hname (sort (keys %hash)) {
		my $data = $hash{$hname};
		my $hn = "static_$hname";
		my $num = scalar(@$data);
		my (@rules, @args);
		my $index = 0;
		foreach my $d (@$data) {
			my $argprefix = sprintf("dacf_arg_%s_%d_", $hname,
						$index);
			$dumper->dumpDacfRule(\@rules, \@args, $argprefix,
					      @$d);
			$index++;
		}
		my $defs = join("", @rules);
		if (@args) {
			my $arg = join("", @args);
			print <<OUT;

/* Arguments for rules in $hn */
$arg
OUT
		}
		print <<OUT;

/* Rules in $hn */
const int  ${hn}_count = $num;

const dacf_rule_t ${hn}[] = {
$defs
};
OUT
	}
}

1;
