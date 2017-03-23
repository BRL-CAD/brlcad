#! /usr/local/bin/perl -sw

# DEMONSTRATE USING DERIVED CLASS METHODS IN A PARSER

package MyParser;

	use Parse::RecDescent;

	@ISA = qw { Parse::RecDescent };

	# CALL THIS METHOD THROUGH AN OBJECT...

	sub print_cat ($$)
	{
		print "CAT: $_[1]\n";
	}

	# CALL THESE METHODS DIRECTLY (MUST QUALIFY)...

	sub print_dog ($)
	{
		print "DOG: $_[0]\n";
	}

	sub print_rat ($)
	{
		print "RAT: $_[0]\n";
	}


package main;

	$grammar =
	q{
		inputs   :	input(s) | <reject>

		input	 :	"cat"
					{ $thisparser->print_cat($item[1]); }
			 |	"dog"
					{ MyParser::print_dog($item[1]); }
			 |	"rat"
					# ERROR (UNQUALIFIED METHOD CALL)
					{ print_rat($item[1]); }

	};

	$parse = new MyParser ($grammar);

	print "> ";
	while (<>)
	{
		$parse->inputs($_) || print "huh?\n";
		print "> ";
	}

