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
## C language struct dumper.
##

package StaticTool::StructDumper;

use strict;

use FileHandle;
use File::Basename;

use StaticTool;

use constant	TAB_LENGTH	=> 8;
use constant	MEMBER_COLUMN	=> 45;

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;

	return bless {}, $class;
}

sub handle
{
	my $me = shift;

	return $me->{FILE};
}

# Dump struct member.
sub dumpMember
{
	my $me = shift;
	my ($value, $member, $indent, $dst) = @_;

	$indent = "\t" unless (defined($indent));
	$value .= ',';
	my $len = MEMBER_COLUMN - length($indent) * TAB_LENGTH;
	my $str = sprintf("%s%-${len}s  /* %s */\n", $indent, $value, $member);
	if (ref($dst) eq 'ARRAY') {
		push(@$dst, $str);
	}
	else {
		print $str;
	}
}

# Format properties defined in driver.conf file.
sub formatProperty
{
	my $me = shift;
	my ($type, $values) = @_;

	my ($ctype, $size, $propflag, $val);

	require StaticTool::BindingLexer;
	import StaticTool::BindingLexer;

	if ($type == StaticTool::BindingLexer->INT) {
		($ctype, $size, $propflag) =
			("int", 4, 'DDI_PROP_TYPE_INT');
		$val = join(",\n", map { "\t" . $_ } @$values);
		$size *= scalar(@$values);
	}
	elsif ($type == StaticTool::BindingLexer->STRING) {
		$propflag = 'DDI_PROP_TYPE_STRING';
		# Encode string array.
		$ctype = 'char';
		($val, $size) = $me->encodeString($values);
	}
	else {
		die "Unknown type: $type\n";
	}

	return ($ctype, $size, $propflag, $val);
}

sub encodeString
{
	my $me = shift;
	my ($values) = @_;

	my $size = 0;
	my $newline = '';
	my @ret;
	foreach my $str (@$values) {
		my @val;
		my $s = eval_string($str);
		$s .= "\0";	# Append terminator.
		my $len = length($s);
		$size += $len;
		push(@ret, "$newline\t/* $str */");
		$newline = "\n";
		my (@line);
		for (my $i = 0; $i < $len; $i++) {
			my $c = substr($s, $i, 1);
			my $ch = ord($c);
			die "Invalid character in string token: $str\n"
				if ($ch > 127);
			push(@line, sprintf("0x%02x", $ch));
			if (@line == 16) {
				push(@ret, sprintf("\t%s,",
						   join(', ', @line)));
				undef @line;
			}
		}
		push(@ret, sprintf("\t%s", join(', ', @line)))
	}
	if ($size == 0) {
		(@ret) = (sprintf("\t0x00"));
		$size = 1;
	}

	return (join("\n", @ret), $size);
}

# Dump ddi_prop_t.
# Results are stored into @$ddiprop.
sub dumpDdiProp
{
	my $me = shift;
	my ($ddiprop, $pname, $hindex, $index, $next, $prop) = @_;

	my $key = $prop->{KEYS}->[$index];
	my $type = $prop->{TYPES}->{$key};
	my $values = $prop->{VALUES}->{$key};
	my $skey = stringfy($key);

	my ($ctype, $size, $propflag, $val) =
		$me->formatProperty($type, $values);
	my $vname = $pname . '_val';
	push(@$ddiprop, <<OUT);
static const $ctype ${vname}[] = {
$val
};
OUT

	push(@$ddiprop, "static ddi_prop_t $pname = {\n");
	$me->dumpMember($next, "prop_next", undef, $ddiprop);
	$me->dumpMember("NODEV", "prop_dev", undef, $ddiprop);
	$me->dumpMember($skey, "prop_name", undef, $ddiprop);
	$me->dumpMember($propflag, "prop_flags", undef, $ddiprop);
	$me->dumpMember($size, "prop_len", undef, $ddiprop);
	$me->dumpMember("(caddr_t)$vname", "prop_val", undef, $ddiprop);
	push(@$ddiprop, "};\n");
}

# Dump member of struct hwc_spec array, and related definitions.
# Results are stored into @$hwc.
sub dumpHwcSpec
{
	my $me = shift;
	my ($hwc, $ddiprop, $index, $prop) = @_;

	my $keys = $prop->{KEYS};
	my $ddip = C_NULL;
	if (@{$keys}) {
		my $nprop = scalar(@{$keys});
		my @pnames;
		for (my $i = 0; $i < $nprop; $i++) {
			my $pname = "drvconf_ddi_prop_${index}_${i}";
			push(@pnames, $pname);
			push(@$ddiprop, <<OUT);
static ddi_prop_t $pname;
OUT
		}
		for (my $i = 0; $i < $nprop; $i++) {
			my $next = ($i == $nprop - 1) ? C_NULL
				: '&' . $pnames[$i + 1];
			$me->dumpDdiProp($ddiprop, $pnames[$i], $index, $i,
					 $next, $prop);
		}
		$ddip = ($pnames[0]) ? '&' . $pnames[0] : C_NULL;
	}

	my $name = stringfy($prop->{name}, 1);
	my $class = stringfy($prop->{class}, 1);
	my $parent = stringfy($prop->{parent}, 1);
	push(@$hwc, "\t{\n");
	my $indent = "\t\t";
	$me->dumpMember(C_NULL, "hwc_next", $indent, $hwc);
	$me->dumpMember($parent, "hwc_parent_name", $indent, $hwc);
	$me->dumpMember($class, "hwc_class_name" , $indent, $hwc);
	$me->dumpMember($name, "hwc_devi_name" , $indent, $hwc);
	$me->dumpMember($ddip, "hwc_devi_sys_prop_ptr", $indent, $hwc);
	$me->dumpMember(C_NULL, "hwc_hash_next", $indent, $hwc);
	$me->dumpMember("0", "hwc_major", $indent, $hwc);
	push(@$hwc, "\t},\n");
}

# Dump binding file.
sub dumpBindingFile($$)
{
	my $me = shift;
	my ($file, $prefix) = @_;

	require StaticTool::IntBindParser;
	import StaticTool::IntBindParser;

	my $parser = StaticTool::IntBindParser->new($file);
	my (%names, %values, @keys);
	while (my ($n, $v) = $parser->parse()) {
		if (defined($names{$n})) {
			my $val = $names{$n};
			die "$file: Binding name conflict: $n: $v, $val\n";
		}
		if (defined($values{$v})) {
			my $nm = $values{$v};
			die "$file: Duplicated binding value: $v: $n, $nm\n";
		}
		push(@keys, $n);
		$names{$n} = $v;
		$values{$v} = $n;
	}

	my $num = scalar(@keys);
	my $fname = basename($file);
	print <<OUT;

/* Embedded $fname */
const int  ${prefix}_count = $num;

const static_bind_t ${prefix}[] = {
OUT
	foreach my $n (@keys) {
		my $v = $names{$n};
		my $nm = stringfy($n);
		print <<OUT;
	{ $nm, $v },
OUT
	}
	print <<OUT;
};
OUT

	return \%names;
}

# Dump dacf_arg_t.
sub dumpDacfArg
{
	my $me = shift;
	my ($dst, $args, $symname, $next) = @_;

	$next = C_NULL unless (defined($next));
	push(@$dst, "static const dacf_arg_t $symname = {\n");
	$me->dumpMember(stringfy($args->[0]), "arg_name", undef, $dst);
	$me->dumpMember($args->[1], "arg_val", undef, $dst);
	$me->dumpMember($next, "arg_next", undef, $dst);
	push(@$dst, "};\n");
}

# Dump member of dacf_rule_t array.
sub dumpDacfRule
{
	my $me = shift;
	my ($rule, $args, $argprefix, $nt_data, $modname, $opset, $opid, $opts,
	    $optarg) = @_;

	my @argnames;
	my $index = 0;
	foreach my $a (@$optarg) {
		my $argname = $argprefix . $index;
		push(@argnames, $argname);
		push(@$args, "static const dacf_arg_t  $argname;\n");
		$index++;
	}
	for (my $i = 0; $i < @$optarg; $i++) {
		my $next = $argnames[$i + 1];
		$next = '&' . $next if ($next);
		$me->dumpDacfArg($args, $optarg->[$i], $argnames[$i], $next);
	}

	my $indent = "\t\t";
	my $argptr = (@argnames) ? '&' . $argnames[0] : C_NULL;
	push(@$rule, "\t{\n");
	$me->dumpMember($nt_data, "r_devspec_data", $indent, $rule);
	$me->dumpMember(stringfy($modname), "r_module", $indent, $rule);
	$me->dumpMember(stringfy($opset), "r_opset", $indent, $rule);
	$me->dumpMember($opid, "r_opid", $indent, $rule);
	$me->dumpMember($opts, "r_opts", $indent, $rule);
	$me->dumpMember(0, "r_refs", $indent, $rule);
	$me->dumpMember("(dacf_arg_t *)$argptr", "r_args", $indent, $rule);
	push(@$rule, "\t},\n");
}

1;
