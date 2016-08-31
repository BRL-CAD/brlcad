#! /usr/local/bin/perl -w

use Parse::RecDescent;

my $PCSV_parser = Parse::RecDescent->new(<<'EOGRAMMAR');

	file:	<skip: '[ \t]*'> line(s?)

	line:	field(s? /,|=>/) "\n" 	{ $item[1] }

	field:	<perl_quotelike>	{ join "", @{$item[1]} }
	     |	/((?!,|=>).)*/

EOGRAMMAR

use Data::Dumper;

print  Data::Dumper->Dump($PCSV_parser->file(join "", <DATA>));

__DATA__
comma,separated=>values
with,q{a,perl,twist},/to them/
