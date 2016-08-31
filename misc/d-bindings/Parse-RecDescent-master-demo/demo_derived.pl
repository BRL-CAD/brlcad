#! /usr/local/bin/perl -sw

# THE OL' "EMPTY SUBCLASS IN THE DEMO" TRICK.

use Parse::RecDescent;

sub Parse::RecDescent::f
{
	print "Parse::RecDescent::f\n";
}

@DerParser::ISA = qw { Parse::RecDescent };

$grammar =
q{
	typedef : /type/ ident /has/ <commit> field(s) 'end type'
			{ $return = $item[2]; }
		| /type/ ident ( /is/ | /are/ ) ident
			{ $return = $item[2]; }
		| <error>

	field   : /field/ ident /is/ ident

	ident   : /[A-Za-z]\w*/
			{ f(); $return = $item[1]; }
 };

$parse = new DerParser ( $grammar ) || die "\n";

$str = "
type student has
	field name is text
	field age
end type
";

print "> ", $parse->typedef($str) || "<failed (as expected)>", "\n";


$str = " type student has end type ";

print "> ", $parse->typedef($str) || "<failed (as expected)>", "\n";


$str = " type studentRec is student ";

print "> ", $parse->typedef($str) || "<failed (unexpectedly!)>", "\n";
