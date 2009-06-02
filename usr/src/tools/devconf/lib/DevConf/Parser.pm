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
## Parser for built-in device configuration file.
##

package DevConf::Parser;

use strict;
use vars qw(@ISA %SPECIAL_TOKEN_TYPE %SPECIAL_TOKENS);

use FileHandle;
use File::stat;
use File::Basename;

use UtsTune::Parser;
use UtsTune::TuneDefParser;

use DevConf::Constants;
use DevConf::Block::DefObj;
use DevConf::Block::Device;
use DevConf::Block::Register;
use DevConf::Type::CppByte;
use DevConf::Type::CppInt;
use DevConf::Type::CppInt64;
use DevConf::Type::CppString;

@ISA = qw(UtsTune::TuneDefParser);	# Piggyback utstune parser.

# Parser state
use constant	S_BEGIN		=> 1;
use constant	S_BLOCK		=> 2;
use constant	S_BLOCK_END	=> 3;
use constant	S_PARAM		=> 4;
use constant	S_PARAM_COLON	=> 5;
use constant	S_PARAM_COMMA	=> 6;
use constant	S_PARAM_VALUE	=> 7;
use constant	S_PARAM_END	=> 8;
use constant	S_PROP		=> 9;
use constant	S_PROP_COLON	=> 10;
use constant	S_PROP_COMMA	=> 11;
use constant	S_PROP_VALUE	=> 12;
use constant	S_PROP_END	=> 13;

use constant	BUILTIN_DEV_FMT	=> "\t\t%-36s   /* %s */\n";

# Define block name at the top level context.
use constant	BLOCK_DEFS => {
	device	=> 'parseDevice',
	defobj	=> 'parseDefObj',
};

# Define block name that defines properties except for 'boolean'.
use constant	PROPERTY_DEFS => {
	int	=> {CLASS => 'DevConf::Type::CppInt',
		    METHOD => 'addIntProperty'},
	int64	=> {CLASS => 'DevConf::Type::CppInt64',
		    METHOD => 'addInt64Property'},
	string	=> {CLASS => 'DevConf::Type::CppString',
		    METHOD => 'addStringProperty'},
	byte	=> {CLASS => 'DevConf::Type::CppByte',
		    METHOD => 'addByteProperty'},
};

# Token prefix and its token type.
%SPECIAL_TOKEN_TYPE = (cpp => CPP, obj => OBJ);

# Construct %SPECIAL_TOKENS map.
{
	foreach my $prefix (keys %SPECIAL_TOKEN_TYPE) {
		my $c = substr($prefix, 0, 1);
		$SPECIAL_TOKENS{$c} = 1;
	}
}

sub new
{
	my $this = shift;
	my $class = ref($this) || $this;
	my ($file) = @_;

	my $fh = FileHandle->new($file) or die "open($file) failed: $!\n";
	$fh->input_line_number(1);

	my $sb = stat($fh) or die "stat($file) failed: $!\n";

	my $me = bless {FILE => $file, HANDLE => $fh, CHAR => [],
			DEVICES => [], UNPARSED => [], EXPORT => [],
			OBJMAP => {}}, $class;

	return $me;
}

sub nextToken
{
	my $me = shift;
	my ($needed) = @_;

	my $unparsed = pop(@{$me->{UNPARSED}});
	return @$unparsed if (defined($unparsed));

	my ($skip, $c, $eol);
	while (defined($c = $me->getChar())) {
		# Skip newline.
		if ($c eq "\n") {
			undef $skip;
			$eol = 1;
			next;
		}
		if ($eol and $c eq "\\") {
			# This line should be exported to the output file.
			my $line = '';
			while (defined($c = $me->getChar())) {
				last if ($c eq "\n");
				$line .= $c;
			}
			push(@{$me->{EXPORT}}, $line);
			next;
		}
		undef $eol;

		# Skip whitespace.
		next if ($c =~ /^\s$/);

		# Skip comment.
		if ($c eq '#') {
			$skip = 1;
			next;
		}
		next if ($skip);
		last;
	}

	unless (defined($c)) {
		$me->syntaxError("Unexpected EOF.") if ($needed);
		return undef;
	}

	return $me->tokenize($c);
}


sub parseError
{
	my $me = shift;

	my $line = $me->lineNumber();
	my $file = $me->{FILE};
	my $err = join("", @_);
	die "$file: $line: $err\n";
}

sub tokenize
{
	my $me = shift;
	my ($c) = @_;

	if ($SPECIAL_TOKENS{$c}) {
		# Check whether this token is has a valid prefix.
		my (@ns, $prefix);
		while (1) {
			my $c1 = $me->getChar();
			last unless (defined($c1));
			if ($c1 eq ':') {
				$prefix = $c . join('', @ns);
				push(@ns, $c1);
				last;
			}
			push(@ns, $c1);
		}
		if ($prefix and my $type = $SPECIAL_TOKEN_TYPE{$prefix}) {
			my $token = '';
			$c = $me->getChar();
			while (defined($c)) {
				if ($c eq ';' or $c eq ',' or $c =~ /^\s$/) {
					$me->ungetChar($c);
					last;
				}
				$token .= $c;
				$c = $me->getChar();
			}
			$me->syntaxError("Empty token with \"$prefix\" prefix.")
				unless ($token);
			return ($token, $type);
		}
		else {
			while (my $c1 = pop(@ns)) {
				$me->ungetChar($c1);
			}
		}
	}

	return $me->SUPER::tokenize($c);
}

sub parse
{
	my $me = shift;

	return if ($me->{_PARSED});
	$me->{_PARSED} = 1;

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(0);
		last unless (defined($type));

		if (my $method = BLOCK_DEFS->{$token}) {
			# Found a block at the top level context.
			$me->$method();
			next LOOP;
		}
		$me->syntaxError("Unknown block name: $token");
	}
}

sub parseDevice
{
	my $me = shift;

	my $list = $me->{DEVICES};
	my $devidx = scalar(@$list);
	my $state = S_BLOCK;
	my $dev;

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_BLOCK) {
			if ($type == CPP or $type == $me->SYMBOL) {
				$me->parseError("Node name must be a quoted ",
						"string");
			}
			elsif ($type != $me->STRING) {
				$me->syntaxError("\"device\" requires node ",
						 "name: ", $token);
			}
			my $str = $me->evalString($token);
			$me->checkNodeName($str, 'node name');
			my $name = DevConf::Type::CppString->
				new($token, $type);
			$dev = DevConf::Block::Device->new($name, $devidx);
			$state = S_BLOCK_END;
			next LOOP;
		}
		if ($state == S_BLOCK_END) {
			$me->syntaxError("'{' is required.")
				unless ($type == $me->LBRACE);
			$me->parseParameter($dev);
			$state = S_BLOCK;
			last LOOP;
		}
	}

	# Check whether parameters are valid.
	if ($dev) {
		$dev->fixup($me);
		push(@$list, $dev);
	}

	return;
}

sub parseDefObj
{
	my $me = shift;

	my $state = S_BLOCK;
	my ($obj, $name);

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);

		if ($state == S_BLOCK) {
			if ($type == CPP) {
				$me->parseError("Object name must not be a ",
						"CPP macro name.");
			}
			elsif ($type == $me->STRING) {
				$me->parseError("Object name must not be a ",
						"quoted string.");
			}
			elsif ($type != $me->SYMBOL) {
				$me->syntaxError("\"defobj\" requires name: ",
						 $token);
			}
			$name = $token;
			$me->checkNodeName($name, 'object name',
					   '[^a-zA-Z\d_]');
			$me->parseError("Object \"$name\" is already defined.")
				if ($me->getDefinedObject($name));
			$obj = DevConf::Block::DefObj->new($name);
			$state = S_BLOCK_END;
			next LOOP;
		}
		if ($state == S_BLOCK_END) {
			$me->syntaxError("'{' is required.")
				unless ($type == $me->LBRACE);
			$me->parseParameter($obj);
			$state = S_BLOCK;
			last LOOP;
		}
	}

	# Check whether parameters are valid.
	if ($obj) {
		$obj->fixup($me);
		$me->{OBJMAP}->{$name} = $obj;
	}

	return;
}

sub checkNodeName
{
	my $me = shift;
	my ($name, $label, $invchars) = @_;

	$me->parseError("Too long $label: $name")
		if (length($name) > MAX_NODENAMELEN);

	my $ll = ucfirst($label);
	$me->parseError("$ll must not be an empty string")
		if (isEmptyString($name));

	$me->syntaxError("$ll must start with alphabet: $name")
		unless ($name =~ /^[a-zA-Z]/o);

	$invchars = '[^a-zA-Z\d,_]' unless ($invchars);
	$me->syntaxError("Illegal character in $label: $name")
		if ($name =~ /$invchars/);
}

sub parseParameter
{
	my $me = shift;
	my ($dev) = @_;

	my $state = S_PARAM;
	my ($pname, @param);

	LOOP:
	while (1) {
		my ($token, $type) = $me->nextToken(1);
		if ($state == S_PARAM) {
			# Check end of block.
			last LOOP if ($type == $me->RBRACE);

			$me->syntaxError("Parameter name is required.")
				unless ($type == $me->SYMBOL);
			$pname = $token;
			if ($dev->isa('DevConf::Block::Device')) {
				if ($pname eq 'registers') {
					$me->parseRegisters($dev);
					next LOOP;
				}
				if ($pname eq 'properties') {
					$me->parseProperties($dev);
					next LOOP;
				}
			}
			$state = S_PARAM_COLON;
			next LOOP;
		}
		if ($state == S_PARAM_COLON) {
			$me->syntaxError("':' is required.")
				unless ($type == $me->COLON);
			$state = S_PARAM_VALUE;
			next LOOP;
		}
		if ($state == S_PARAM_VALUE || $state == S_PARAM_COMMA) {
			$me->syntaxError("Value must be defined")
				unless ($type & $me->TOKEN_VALUE);
			$me->parseError("Object reference can not be written ",
					"here: $token") if ($type == OBJ);
			push(@param, [$token, $type]);
			$state = S_PARAM_END;
			next LOOP;
		}
		if ($state == S_PARAM_END) {
			if ($type == $me->COMMA) {
				$state = S_PARAM_COMMA;
			}
			else {
				$me->syntaxError("Semicolon is required")
					unless ($type == $me->SEMI);

				$dev->addParameter($me, $pname, \@param);
				$state = S_PARAM;
				undef $pname;
				undef @param;
			}
			next LOOP;
		}
	}
}

sub parseRegisters
{
	my $me = shift;
	my ($dev) = @_;

	$me->parseError("Only one \"registers\" block can be specified in ",
			" one \"device\" block")
		unless (@{$dev->getRegisters()} == 0);
	my (@regs);
	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);

	while (1) {
		($token, $type) = $me->nextToken(1);

		last if ($type == $me->RBRACE);
		$me->syntaxError("Unexpected token in \"registers\": $token")
			unless ($token eq 'reg');
		push(@regs, $me->parseRegister($dev));
	}
	$me->parseError("No \"reg\" block in \"registers\" block")
		unless (@regs);

	$dev->setRegisters($me, \@regs);
}

sub parseRegister
{
	my $me = shift;
	my ($dev) = @_;

	my $reg = DevConf::Block::Register->new();
	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);
	$me->parseParameter($reg);

	# Check whether parameters are valid.
	$reg->fixup($me);

	return $reg;
}

sub parseProperties
{
	my $me = shift;
	my ($dev) = @_;

	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);

	while (1) {
		($token, $type) = $me->nextToken(1);
		last if ($type == $me->RBRACE);

		if ($token eq 'boolean') {
			$me->parseBoolProp($dev);
		}
		else {
			my $spec = PROPERTY_DEFS->{$token};

			$me->syntaxError("Unexpected token in ",
					 "\"properties\": ", $token)
				unless ($spec);

			my $class = $spec->{CLASS};
			my $ptype = $class->new();
			my $method = $spec->{METHOD};
			$me->parseProp($dev, $ptype, $method);
		}
	}
}

sub parseProp
{
	my $me = shift;
	my ($dev, $ptype, $method) = @_;

	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);

	my $state = S_PROP;
	my ($qname, @props, $objname);

	LOOP:
	while (1) {
		($token, $type) = $me->nextToken(1);
		last LOOP if ($type == $me->RBRACE);

		if ($state == S_PROP) {
			$me->syntaxError("Property name must be a quoted ",
					 "string: $token")
				unless ($type == $me->STRING);
			$qname = $token;
			$me->checkPropName($qname);
			$state = S_PROP_COLON;
			next LOOP;
		}
		if ($state == S_PROP_COLON) {
			$me->syntaxError("':' is required.")
				unless ($type == $me->COLON);
			$state = S_PROP_VALUE;
			next LOOP;
		}
		if ($state == S_PROP_VALUE || $state == S_PROP_COMMA) {
			$me->syntaxError("Property value must be defined")
				unless ($type & $me->TOKEN_VALUE);
			my $err = $ptype->checkValue($token, $type, {});
			$me->parseError("$qname: $err") if ($err);

			if ($type == OBJ) {
				# Check whether referred object is defined.
				$me->parseError("Undefined object: $token")
					unless ($me->getDefinedObject($token));
				$objname = $token;
			}

			my $t = $ptype->new($token, $type);
			push(@props, $t);
			$state = S_PROP_END;
			next LOOP;
		}
		if ($state == S_PROP_END) {
			if ($type == $me->COMMA) {
				$state = S_PROP_COMMA;
			}
			else {
				$me->syntaxError("Semicolon is required")
					unless ($type == $me->SEMI);

				if ($objname) {
					# Check whether object reference is
					# only value for the property.
					$me->parseError("Array value with ",
							"object reference is ",
							"not allowed.")
						if (@props > 1);
					$dev->addObjectReference($objname);
				}

				$dev->$method($me, $qname, \@props);
				$state = S_PROP;
				undef $qname;
				undef @props;
				undef $objname;
			}
			next LOOP;
		}
	}
}

sub checkPropName
{
	my $me = shift;
	my ($qname) = @_;

	my $name = $me->evalString($qname);
	$me->parseError("Register must be defined in \"registers\" block")
		if ($name eq PROP_REG);
	$me->parseError("Interrupt resources must be defined by ",
			"\"interrupt\" parameter")
		if ($name eq PROP_INTERRUPTS);
}

sub parseBoolProp
{
	my $me = shift;
	my ($dev) = @_;

	my ($token, $type) = $me->nextToken(1);
	$me->syntaxError("'{' is required.") unless ($type == $me->LBRACE);

	my $state = S_PROP;

	LOOP:
	while (1) {
		($token, $type) = $me->nextToken(1);
		last LOOP if ($type == $me->RBRACE);

		if ($state == S_PROP) {
			$me->syntaxError("Property name must be a quoted ",
					 "string: $token")
				unless ($type == $me->STRING);
			$me->checkPropName($token);
			$dev->addBoolProperty($me, $token);
			$state = S_PROP_END;
			next LOOP;
		}
		if ($state == S_PROP_END) {
			$me->syntaxError("Semicolon is required")
				unless ($type == $me->SEMI);
			$state = S_PROP;
			next LOOP;
		}
	}
}

sub output
{
	my $me = shift;
	my ($out) = @_;

	$me->parse();

	my $exp = join("\n", @{$me->{EXPORT}});

	$out->print(<<OUT);
/*
 * WARNING: Auto-generated file. NEVER EDIT THIS FILE!!!
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/builtin.h>
$exp

OUT

	# Dump C objects defined by "defobj" block.
	my (%objs);
	my $devices = $me->{DEVICES};
	foreach my $dev (@$devices) {
		foreach my $name (@{$dev->getAllObjectReferences()}) {
			my $cond = $dev->cppCondition();
			my $obj = $me->getDefinedObject($name);

			$obj->addCppCondition($cond);
			$objs{$name} = $obj;
		}
	}

	if (%objs) {
		$out->print("\n/* Objects defined by \"defobj\" block. */\n");
		while (my ($name, $obj) = each(%objs)) {
			$obj->output($out);
		}
	}

	$out->print("\n/* Definitions of property values. */\n");
	my (@props, @uart);
	foreach my $dev (@$devices) {
		my $prmap = $dev->dumpForward($out, $me);
		push(@props, $prmap);
		push(@uart, $dev) if ($dev->isUart());
	}

	my $sym = SYM_BUILTIN_DEV;
	my $symNdevs = SYM_BUILTIN_NDEVS;
	my $typeDev = TYPE_BUILTIN_DEV;

	$out->print(<<OUT);

/* Definitions of device nodes. */
const $typeDev	$sym\[\] = {
OUT
	foreach my $dev (@$devices) {
		my $name = $dev->getName();
		my $prmap = shift(@props);
		my $prop_int = $prmap->{bd_prop_int};
		my $prop_int64 = $prmap->{bd_prop_int64};
		my $prop_str = $prmap->{bd_prop_string};
		my $prop_byte = $prmap->{bd_prop_byte};
		my $prop_bool = $prmap->{bd_prop_boolean};
		my $nprops_int = $prmap->{bd_nprops_int};
		my $nprops_int64 = $prmap->{bd_nprops_int64};
		my $nprops_str = $prmap->{bd_nprops_string};
		my $nprops_byte = $prmap->{bd_nprops_byte};
		my $nprops_bool = $prmap->{bd_nprops_boolean};

		my $cond = $dev->cppCondition();
		$out->print("#if\t$cond\n") if ($cond);
		$out->print("\t{\n");

		$out->printf(BUILTIN_DEV_FMT, $name . ',', 'bd_name');

		foreach my $key (qw(bd_prop_int bd_prop_int64 bd_prop_string
				    bd_prop_byte bd_prop_boolean
				    bd_nprops_int bd_nprops_int64
				    bd_nprops_string bd_nprops_byte
				    bd_nprops_boolean)) {
			my $value = $prmap->{$key};
			$value .= ',' unless ($key eq 'bd_nprops_boolean');
			$out->printf(BUILTIN_DEV_FMT, $value, $key);
		}
		$out->print(<<OUT);
	},
OUT
		$out->print("#endif\t/* $cond */\n") if ($cond);
	}
	$out->print(<<OUT);
};

const uint_t	$symNdevs = sizeof($sym) / sizeof($typeDev);
OUT

	my $symUart = SYM_BUILTIN_UART_PORT;
	my $symNuart = SYM_BUILTIN_UART_NPORTS;

	$out->print(<<OUT);

const int	$symUart\[\] = {
OUT
	foreach my $dev (@uart) {
		my $cond = $dev->cppCondition();
		$out->print("#if\t$cond\n") if ($cond);
		foreach my $port (@{$dev->getUartPorts()}) {
			$out->print("\t$port,\n");
		}
		$out->print("#endif\t/* $cond */\n") if ($cond);
	}

	$out->print(<<OUT);
};

const uint_t	$symNuart = sizeof($symUart) / sizeof(int);
OUT
}

sub getDefinedObject
{
	my $me = shift;
	my ($name) = @_;

	return $me->{OBJMAP}->{$name};
}

# Unused TuneDefParser methods.
sub debug;
sub print;
sub isDefinition;
sub getPath;
sub parseUniqueBlock;
sub parseEnhanced;
sub getEfile;
sub updateEfile;
sub merge;
sub header;
sub getDefPath;
sub hasValue;
sub getOption;
sub getStatic;
sub getMakeOpt;
sub updateCheck;

1;
