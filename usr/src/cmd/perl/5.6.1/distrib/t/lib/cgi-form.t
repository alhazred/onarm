#!./perl -w

use lib qw(t/lib ./lib ../blib/lib);

# Due to a bug in older versions of MakeMaker & Test::Harness, we must
# ensure the blib's are in @INC, else we might use the core CGI.pm
use lib qw(blib/lib blib/arch);

use Test;
our $loaded = 1;
BEGIN { 
	plan(tests => 17);
}
END {
	ok($loaded, 1, "Loaded");
}

use CGI (':standard','-no_debug');

my $CRLF = "\015\012";
if ($^O eq 'VMS') {
    $CRLF = "\n";  # via web server carriage is inserted automatically
}
if (ord("\t") != 9) { # EBCDIC?
    $CRLF = "\r\n";
}


# Set up a CGI environment
$ENV{REQUEST_METHOD}  = 'GET';
$ENV{QUERY_STRING}    = 'game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}       = '/somewhere/else';
$ENV{PATH_TRANSLATED} = '/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}     ='/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT}     = 8080;
$ENV{SERVER_NAME}     = 'the.good.ship.lollypop.com';

ok(start_form(-action=>'foobar',-method=>'get'),
   qq(<form method="get" action="foobar" enctype="application/x-www-form-urlencoded">\n),
   "start_form()");

ok(submit(),
   qq(<input type="submit" name=".submit" />),
   "submit()");

ok(submit(-name  => 'foo',
	  -value => 'bar'),
   qq(<input type="submit" name="foo" value="bar" />),
   "submit(-name,-value)");

ok(submit({-name  => 'foo',
	   -value => 'bar'}),
   qq(<input type="submit" name="foo" value="bar" />),
   "submit({-name,-value})");

ok(textfield(-name => 'weather'),
   qq(<input type="text" name="weather" value="dull" />),
   "textfield({-name})");

ok(textfield(-name  => 'weather',
	     -value => 'nice'),
   qq(<input type="text" name="weather" value="dull" />),
   "textfield({-name,-value})");

ok(textfield(-name     => 'weather',
	     -value    => 'nice',
	     -override => 1),
   qq(<input type="text" name="weather" value="nice" />),
   "textfield({-name,-value,-override})");

ok(checkbox(-name  => 'weather',
	    -value => 'nice'),
   qq(<input type="checkbox" name="weather" value="nice" />weather),
   "checkbox()");

ok(checkbox(-name  => 'weather',
	    -value => 'nice',
	    -label => 'forecast'),
   qq(<input type="checkbox" name="weather" value="nice" />forecast),
   "checkbox()");

ok(checkbox(-name     => 'weather',
	    -value    => 'nice',
	    -label    => 'forecast',
	    -checked  => 1,
	    -override => 1),
   qq(<input type="checkbox" name="weather" value="nice" checked="checked" />forecast),
   "checkbox()");

ok(checkbox(-name  => 'weather',
	    -value => 'dull',
	    -label => 'forecast'),
   qq(<input type="checkbox" name="weather" value="dull" checked="checked" />forecast),
   "checkbox()");

ok(radio_group(-name => 'game'),
   qq(<input type="radio" name="game" value="chess" checked="checked" />chess ).
   qq(<input type="radio" name="game" value="checkers" />checkers),
   'radio_group()');

ok(radio_group(-name   => 'game',
	       -labels => {'chess' => 'ping pong'}),
   qq(<input type="radio" name="game" value="chess" checked="checked" />ping pong ).
   qq(<input type="radio" name="game" value="checkers" />checkers),
   'radio_group()');

ok(checkbox_group(-name   => 'game',
		  -Values => [qw/checkers chess cribbage/]),
   qq(<input type="checkbox" name="game" value="checkers" checked="checked" />checkers ).
   qq(<input type="checkbox" name="game" value="chess" checked="checked" />chess ).
   qq(<input type="checkbox" name="game" value="cribbage" />cribbage),
   'checkbox_group()');

ok(checkbox_group(-name       => 'game',
		  '-values'   => [qw/checkers chess cribbage/],
		  '-defaults' => ['cribbage'],-override=>1),
   qq(<input type="checkbox" name="game" value="checkers" />checkers ).
   qq(<input type="checkbox" name="game" value="chess" />chess ).
   qq(<input type="checkbox" name="game" value="cribbage" checked="checked" />cribbage),
   'checkbox_group()');

ok(popup_menu(-name     => 'game',
	      '-values' => [qw/checkers chess cribbage/],
	      -default  => 'cribbage',
	      -override => 1)."\n",
   <<END, 'checkbox_group()');
<select name="game">
<option value="checkers">checkers</option>
<option value="chess">chess</option>
<option selected="selected" value="cribbage">cribbage</option>
</select>
END

