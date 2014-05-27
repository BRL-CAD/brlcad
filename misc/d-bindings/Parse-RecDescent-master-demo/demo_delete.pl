#! /usr/local/bin/perl -sw

# DELETABLE PRODUCTIONS

use Parse::RecDescent;

$grammar =
q{
	commands :	command(s)

	command  :	directive
		 |	evaluation

	directive:	'only prefix'
			    { $thisparser->{deleted} = {infix=>1,postfix=>1} }
		 |	'not prefix'
			    { $thisparser->{deleted}{prefix} = 1 }

	evaluation:	<reject: $thisparser->{deleted}{prefix}> op var var
				{ print "prefix $item[3] $item[2] $item[4]\n" }

	          |	<reject: $thisparser->{deleted}{infix}> var op var
				{ print "infix $item[2] $item[3] $item[4]\n" }

	          |	<reject: $thisparser->{deleted}{postfix}> var var op
				{ print "postfix $item[2] $item[4] $item[3]\n" }

	op        :	'+' | '-'

	var	  :	/(?!\d)\w+/
};

$parse = new Parse::RecDescent ($grammar);

while (<DATA>)
{
	$parse->commands($_);
}

__DATA__
a + b
+ a b
a b +
not prefix
a + b
+ a b
a b +
only prefix
a + b
+ a b
a b +
not prefix
a + b
+ a b
a b +
