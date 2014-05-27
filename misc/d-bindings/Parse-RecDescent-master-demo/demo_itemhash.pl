#! /usr/local/bin/perl -ws

use Parse::RecDescent;

sub evalop
{
	my (@list) = @{[@{$_[0]}]};
	my $val = shift(@list)->();
	while (@list)
	{
		my ($op, $arg2) = splice @list, 0, 2;
		$op->($val,$arg2->());
	}
	return $val;
}

my $parse = Parse::RecDescent->new(<<'EndGrammar');

	main: expr /\s*\Z/ { $item{expr}->() }
	    | <error: expected expression in $item{__RULE__}>

	expr: /for(each)?/ lvar range expr
			{ my ($vname,$expr) = @item{"lvar","expr"};
			  my ($from, $to) = @{$item{range}};
			  sub { my $val;
				no strict "refs";
				for $$vname ($from->()..$to->())
					{ $val = $expr->() }
				return $val;
			      }
			}
	    | lvar '=' addition
			{ my ($vname, $expr) = @item{"lvar","addition"};
			  sub { no strict 'refs'; $$vname = $expr->() }
			}
	    | addition

	range: "(" expr ".." expr ")"
			{ [ @item[2,4] ] }

	addition: <leftop:multiplication add_op multiplication>
			{ my $add = $item[1]; sub { ::evalop $add } }

	add_op: '+'	{ sub { $_[0] += $_[1] } }
	      | '-'	{ sub { $_[0] -= $_[1] } }

	multiplication: <leftop:factor mult_op factor>
			{ my $mult = $item[1]; sub { ::evalop $mult } }

	mult_op: '*'	{ sub { $_[0] *= $_[1] } }
	       | '/'	{ sub { $_[0] /= $_[1] } }

	factor: number
	      | rvar
	      | '(' expr ')' { $item{expr} }

	number: /[-+]?\d+(\.\d+)?/	{ sub { $item[1] } }

	lvar:	/\$([a-z]\w*)/		{ $1 }

	rvar:	lvar			{ sub { no strict 'refs'; ${$item{lvar}} } }

EndGrammar

print "> ";
while (<>) {	# FOR DEMO CHANGE TO: while (<DATA>)
  print eval {$parse->main($_)}||"", "\n\n> ";
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
