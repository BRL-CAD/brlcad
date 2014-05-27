#! /usr/local/bin/perl -ws

# use strict;

use Parse::RecDescent;

print 1 if Parse::RecDescent->new(<<'EOGRAMMAR')->file(join "", <DATA>);

	<autorule: /\w+/ { print "Found $item[-1]\n" } >

	file: 	defn(s)

	defn:	'def' ident ':' block

	block:	'{' item(s) '}'

EOGRAMMAR

__DATA__

def ident : { item item item }
