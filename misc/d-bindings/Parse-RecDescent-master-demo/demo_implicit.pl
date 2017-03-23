#! /usr/local/bin/perl -sw

# IMPLICIT SUB RULES, OKAY!

use strict;
use Parse::RecDescent;

my $grammar =
q{
	{ my $state = "happy"; }

	testimony	: declaration(s)
				{ print "Ended up $state\n"; }

	declaration	: 'I' ('like'|'loathe')(s) ('ice-cream'|'lawyers')
				{ print "$item[3]\n";
				  $state = 'happy/sad'; }
			| 'I' ("can't"|"can") ('fly'|'swim')
				{ print "$item[3]\n";
				  $state = 'able/unable'; }
			| <error>

};

my $parse = new Parse::RecDescent ($grammar);
my $input;

while (defined ($input = <>))
{
	$parse->testimony($input) or print "huh?\n";
}
