#! /usr/local/bin/perl -ws

use Parse::RecDescent;

my $street_type = join '|', qw
{
        Street		St\.?
        Road		Rd
        Avenue		Ave\.?
	Lane
	Way
	Highway		Hwy
};

sub Parse::RecDescent::street_name
{
	print join('|', @_), "\n";
        $_[1] =~ s/\A\s*(([A-Z]+\s+)+($street_type))//io;
        return $1;
}

my $parser = Parse::RecDescent->new(<<'EOGRAMMAR');

	addr: /\d+[A-Z]?/i street_name
		{ print "Number $item[1] in $item{street_name}\n" }

EOGRAMMAR

while (<>)
{
	$parser->addr($_);
}


