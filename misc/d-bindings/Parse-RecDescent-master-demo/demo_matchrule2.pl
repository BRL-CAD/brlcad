#! /usr/local/bin/perl -sw

use vars qw($animal);
use Parse::RecDescent;

$RD_AUTOACTION = q{ $last = $item[1] };

$grammar = q {

	sequence: <rulevar: local $last>
	sequence: base <matchrule: after_$last >(3..)
			{ [ $item[1], @{$item[2]} ] }
		| base sequence
			{ $item[2] }


	base: /[ACGT]/

	after_A: /[C]/
	after_C: /[AG]/
	after_G: /[CT]/
	after_T: /[G]/
};

$parser = new Parse::RecDescent( $grammar ) or
    die "bad grammar; bailing";

local $/;
use AutoDump;

show $parser->sequence(<DATA>);

__DATA__

AAACTTTAAAACGTGCGCACGTGTAAAAAA
