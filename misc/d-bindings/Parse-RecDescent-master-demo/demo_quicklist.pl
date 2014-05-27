#! /usr/local/bin/perl -w

use Parse::RecDescent;

#$RD_TRACE=1;
#$RD_HINT=1;

my $parser = Parse::RecDescent->new(<<'EOG') or die;

	list1N_c:	term(s /,/)
	list1N_s:	term(s /\/+/)

	list0N_c:	term(s? /,/)
	list0N_s:	term(s? /\/+/)

	list01_c:	term(? /,/)
	list01_s:	term(? /\/+/)

	list2_c:	term(2 /,/)
	list2_s:	term(2 /\/+/)

	list02_c:	term(0..2 /,/)
	list02_s:	term(0..2 /\/+/)

	list2N_c:	term(2.. /,/)
	list2N_s:	term(2.. /\/+/)

	list13_c:	term(..3 /,/)
	list13_s:	term(..3 /\/+/)

	term: 't'
EOG

while (<DATA>)
{
	print;
	print "\tlist1N_c:\t", @{$parser->list1N_c($_)||['undef']}, "\n";
	print "\tlist1N_s:\t", @{$parser->list1N_s($_)||['undef']}, "\n";

	print "\tlist0N_c:\t", @{$parser->list0N_c($_)||['undef']}, "\n";
	print "\tlist0N_s:\t", @{$parser->list0N_s($_)||['undef']}, "\n";

	print "\tlist01_c:\t", @{$parser->list01_c($_)||['undef']}, "\n";
	print "\tlist01_s:\t", @{$parser->list01_s($_)||['undef']}, "\n";

	print "\tlist2_c:\t", @{$parser->list2_c($_)||['undef']}, "\n";
	print "\tlist2_s:\t", @{$parser->list2_s($_)||['undef']}, "\n";

	print "\tlist02_c:\t", @{$parser->list02_c($_)||['undef']}, "\n";
	print "\tlist02_s:\t", @{$parser->list02_s($_)||['undef']}, "\n";

	print "\tlist2N_c:\t", @{$parser->list2N_c($_)||['undef']}, "\n";
	print "\tlist2N_s:\t", @{$parser->list2N_s($_)||['undef']}, "\n";

	print "\tlist13_c:\t", @{$parser->list13_c($_)||['undef']}, "\n";
	print "\tlist13_s:\t", @{$parser->list13_s($_)||['undef']}, "\n";

	print "-----\n";
}

__DATA__

t
t,t
t,t,t
t,t,t,t
t
t/t
t/t//t
t/t///t/t
