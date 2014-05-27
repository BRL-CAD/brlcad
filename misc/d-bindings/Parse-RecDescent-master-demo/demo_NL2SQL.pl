#! /usr/local/bin/perl -ws

use Parse::RecDescent;


my $grammar = <<'EOGRAMMAR';

translate:	select
	 |	sum
	 |	identify
	 |			{ "Could you rephase that?\n" }

select:		ask_select qualifier(?) field /of/ qualifier(?) table
				{ "SELECT DISTINCT $item[3]\nFROM $item[6]\n" }
	      |	ask_select qualifier(?) table /'?/ qualifier(?) field
				{ "SELECT DISTINCT $item[6]\nFROM $item[3]\n" }
	      |	ask_select qualifier(?) table
				{ "SELECT *\nFROM $item[3]\n" }

sum:		ask_count table prep qualifier field prep(?) value
				{ "SELECT COUNT(*)\nFROM $item[2]\n" .
				  "WHERE $item[5] = $item[7]\n" }
   |		ask_count table
				{ "SELECT COUNT(*)\nFROM $item[2]\n" }

identify:	ask_select(?) /who supplies/ qualifier value
				{ "SELECT supplier\nFROM merchandise\n" .
				  "WHERE name = $item[4]\n" }

	|	whats qualifier field /of/ qualifier /suppliers? of/ value
				{ "SELECT suppliers.$item[3]\n" .
				  "FROM suppliers, products\n" .
				  "WHERE products.name = $item[-1]\n" .
				  "AND suppliers.name = products.supplier \n" }




field:		/(name)s?/			{ $1 }
     |		/(product)s?/			{ $1 }
     |		/(id)(entit(y|ies))?/		{ $1 }
     |		/(quantit(y|ies))/		{ $1 }
     |		/(received)s?/			{ $1 }
     |		/(supplier)s?/			{ $1 }
     |		/(cost)s?/			{ $1 }
     |		/(address)(es)?/		{ $1 }

table:		/suppliers?/			{ 'suppliers' }
     |		'merchandise'
     |		/orders?/			{ 'orders' }

qualifier:	/the|every|all( the)?|any|our/

ask_select:	reply to_me

ask_count:	ask_select(?) /how (many|much)/

reply:		/tell|show|list/

value:		/\w+/				{ qq{'$item[1]'} }

to_me:		/((to )?(me|us))?/

whats:		/what's|what (is|are)/

prep:		/for|of|with|by/

EOGRAMMAR

my $parser = Parse::RecDescent->new($grammar)
	or die "Bad grammar";

$| = 1;
while (<DATA>)
{
	print "> ";
	sleep 1;
	print; <>;
	my $SQL = $parser->translate($_);
	print $SQL, "\n";
}

__DATA__
how many orders for the product spam are there?
tell me how many suppliers by the name of Jones we have
what are the names of our suppliers of trinitrotoluene?
tell me our suppliers' names
list our merchandise
show us the suppliers of the mechanise
list all supplier names
how many orders are there?
how much merchandise do we carry?
who supplies our nitrocelluose?
tell me who supplies our ethylacetate?
what are the addresses of our suppliers of trinitrotoluene?
