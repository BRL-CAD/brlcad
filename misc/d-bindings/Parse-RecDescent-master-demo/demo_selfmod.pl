#! /usr/local/bin/perl -sw

# A SELF-MODIFYING**2 PARSER

use Parse::RecDescent;

$grammar =
q{
	type_defn : 'only' <commit> "$::keyword" is type_name ';'
		       { $thisparser->Replace("type_name: '$item[5]'");
			  print "\"$item[5]\" is now the only valid type name\n";
		       }

		  | "$::keyword" <commit> identifier(s) is type_name /('?s)?/ ';'
		       { my $newnames = join " | ", map { "'$_'" } @{$item[3]};
			  $thisparser->Extend("type_name: $newnames");
			  print "added $newnames as type name(s)\n";
		       }

		  | /change\s+$::keyword\s+to/ identifier
		       { $::keyword = $item[2];
			  print "changed $item[0] keyword to: $item[2]\n";
		       }

		  | <error>

	is        : /is|are/

	type_name : 'int' | 'float'

	identifier: ...!is ...!"$::keyword" ...!type_name /[A-Za-z]\w*/

};

use vars qw { $keyword };

$keyword = "type";

$parse = new Parse::RecDescent ($grammar);

while (<>)
{
	$parse->type_defn($_) or print "huh?\n";
}
