#! /usr/local/bin/perl -sw

# PARSE LOGICAL EXPRESSIONS TO A "list of lists" PARSE TREE

sub printtree
{
	print "    " x $_[0];
	print "$_[1]:\n";
	foreach ( @_[2..$#_] )
	{
		if (ref($_)) { printtree($_[0]+1,@$_); }
		else	     { print "    " x $_[0], "$_\n" }
	}
	print "\n";

}

use Parse::RecDescent;

$RD_AUTOACTION = q{ [@item] };

$grammar =
q{
	expr	:	disj

	disj	:	conj 'or' disj | conj

	conj	:	unary 'and' conj | unary

	unary	:	'not' atom
		|	'(' expr ')'
		|	atom

	atom	:	/[a-z]+/i

};

$parse = new Parse::RecDescent ($grammar);

while (<DATA>)
{
	my $tree = $parse->expr($_);
	printtree(0,@$tree) if $tree;
}

__DATA__
a and b and not c
(c or d) and f
