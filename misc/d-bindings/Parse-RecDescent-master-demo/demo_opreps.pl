#! /usr/local/bin/perl -w

use Parse::RecDescent;

#$RD_TRACE=1;
#$RD_HINT=1;

my $parser = Parse::RecDescent->new(<<'EOG') or die;

	list1N:	<rightop: term ',' term>(s)

	list12:	<rightop: term ',' term>(..2)

	list2N:	<rightop: term ',' term>(2..)

	list01:	<rightop: term ',' term>(?)

	list:	<rightop: term ',' term>

	list0N:	<rightop: term ',' term>(s?)

	list23:	<rightop: term ',' term>(2..3)

	list03:	<rightop: term ',' term>(0..3)

	term: 't'
EOG

while (<DATA>)
{
	print;
	print "\tlist:\t",   @{$parser->list($_)||['undef']}, "\n";
	print "\tlist01:\t", @{$parser->list01($_)||['undef']}, "\n";
	print "\tlist0N:\t", @{$parser->list0N($_)||['undef']}, "\n";
	print "\tlist1N:\t", @{$parser->list1N($_)||['undef']}, "\n";
	print "\tlist2N:\t", @{$parser->list2N($_)||['undef']}, "\n";
	print "\tlist23:\t", @{$parser->list23($_)||['undef']}, "\n";
	print "\tlist12:\t", @{$parser->list12($_)||['undef']}, "\n";
	print "\tlist03:\t", @{$parser->list03($_)||['undef']}, "\n";
	print "-----\n";
}

__DATA__

t
t,t
t,t,t
t,t,t,t
