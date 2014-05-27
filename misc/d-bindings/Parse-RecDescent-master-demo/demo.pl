#! /usr/bin/perl -w

package Foo;
use base Parse::RecDescent;

sub print_cat {
	print "Found ", @_[1..$#_], "\n";
}

package main;


$parse = new Parse::RecDescent(<<'EOG');
	input	 :	"cat"
				{ $thisparser->print_cat($item[1]); }
		 |	"dog"
				{ $thisparser->print_cat($item[1]); }
EOG

$parse = bless $parse, "Foo";

$parse->input("cat");
$parse->input("doggone");


