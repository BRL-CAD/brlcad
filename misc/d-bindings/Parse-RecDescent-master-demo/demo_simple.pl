use v5.10;
use warnings;


# WHO IS NEXT TO WHOM?

use Parse::RecDescent;

$grammar =
q{
    <nocheck>

	inputs   :	input(s)

	input	 :	who_question "\n" {1}
	     	 |	is_question  "\n" {1}
	     	 |	statement    "\n" {1}
	     	 |	/bye|quit|exit/ { exit }
		 |	<reject:!$text> <error>     # ERROR IF NOT END OF TEXT
		 |	{ print STDERR "resyncing\n" }

			{ _error(@$_) foreach @{$thisparser->{errors}}; }
			<resync>

	statement:	namelist are <commit> 'next' 'to' namelist
				{ ::nextto $item[1], $item[6], $thisline; 1 }
		 |	<error?> <reject>

	who_question:
			'who' <commit> are 'next' 'to' name '?'
				{ ::whonextto $item[6] ; 1 }
		 |	<error?> <reject>

	is_question:
			'is' <commit> name 'next' 'to' name '?'
				{ ::isnextto($item[3], $item[6]); 1 }
		 |	<error?> <reject>

	namelist :	name(s) 'and' <commit> namelist
				{ [ @{$item[1]}, @{$item[3]} ] }
		 |	name(s)

	name	 :	...!'who' ...!'and' ...!are /[A-Za-z]+/

	are	 :	'is' | 'are'
};

$parse = new Parse::RecDescent ($grammar);
$parse->{tokensep} = '[ \t]*';

$input = '';

print "> ";
while (<>)
{

	if (/^\.$/) { $parse->inputs($input) || print "huh?\n"; $input = '' }
	else	    { $input .= $_ }
	print "> ";
}

sub nextto($$$)
{
	foreach $A ( @{$_[0]} ) {
	    foreach $B ( @{$_[1]} ) {
		nexttoAB($A,$B,$_[2]);
	    }
	}
	print "okay\n";
}

sub nexttoAB($$$)
{
	$nextto{$_[0]} or $nextto{$_[0]} = [];
	$nextto{$_[1]} or $nextto{$_[1]} = [];
	push @{$nextto{$_[0]}}, $_[1];
	push @{$nextto{$_[1]}}, $_[0];
	print "Learnt something from line $_[2]\n";
}

sub whonextto($)
{
	if (defined $nextto{$_[0]})
		{ print join(" and ", @{$nextto{$_[0]}}) . ".\n"; }
	else
		{ print "sorry, I've never heard of $_[0].\n"; }
}

sub isnextto($$)
{
	if (!$nextto{$_[0]})
		{ print "sorry, I've never heard of $_[0].\n"; }
	elsif (!$nextto{$_[1]})
		{ print "sorry, I've never heard of $_[1].\n"; }
	else
	{
		foreach $name (@{$nextto{$_[0]}})
		{
			if ($name eq $_[1]) { print "yes\n"; return }
		}
		print "no\n";
	}
}
