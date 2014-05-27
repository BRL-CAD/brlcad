#! /usr/local/bin/perl -ws

BEGIN
{
	unless (-f "Calc.pm")
	{
		print "You first need to build Calc.pm.\n\n",
		       "Try: perl -MParse::RecDescent - calc_grammar Calc\n",
		       " or: demo_buildcalc.pl\n\n";
		exit;
	}
}

use Parse::RecDescent;
use Calc;

sub evalop
{
	# my (@list) = @{[@{$_[0]}]};
	my (@list) = @{$_[0]};
	my $val = shift(@list)->();
	while (@list)
	{
		my ($op, $arg2) = splice @list, 0, 2;
		$op->($val,$arg2->());
	}
	return $val;
}

my $parse = Calc->new() or die "bad grammar";

print "> ";
while (<DATA>) {	# FOR DEMO CHANGE TO: while (<DATA>)
  print $parse->main($_), "\n\n> ";
}

__DATA__
$x = 2
$y = 3
+1-1+1-1+1-1+1-1+1
7*7-6*8
121/(121/11)/121*11
1/(10-1/(1/(10-1)))
$x * $y
foreach $i (1..$y) $x = $x * 2 + $i
$x
