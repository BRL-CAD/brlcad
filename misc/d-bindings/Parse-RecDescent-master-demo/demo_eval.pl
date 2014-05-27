#! /usr/local/bin/perl -w

use Parse::RecDescent;

$text = "1 + 2 * 3 ** 4";

$grammar = q{
        <autotree>

        expr	:	<leftop: conj /\|\|/ conj>

        conj	:	<leftop: addn /&&/ addn>

        addn	:	<leftop: mult /[+-]/ mult >

        mult	:	<leftop: expo /[*\/\%]/ expo >

        expo	:	<leftop: value /\^|\*\*/ value >

        value	:	/\d+(\.\d+)?/
};

my $extract_tree = Parse::RecDescent->new($grammar);

my $tree = $extract_tree->expr($text);

use Data::Dumper 'Dumper';
print Dumper $tree;

print $tree->eval();

package expr;
use List::Util  'reduce';
sub eval { reduce {$_[0] || $_[1]} map $_->eval(), @{$_[0]{__DIRECTIVE1__}} }

package conj ;
use List::Util  'reduce';
sub eval { reduce {$_[0] && $_[1]} map $_->eval(), @{$_[0]{__DIRECTIVE1__}} }

package addn ;
use List::Util  'reduce';
sub eval { reduce {$_[0] + $_[1]} map $_->eval(), @{$_[0]{__DIRECTIVE1__}} }

package mult ;
use List::Util  'reduce';
sub eval { reduce {$_[0] * $_[1]} map $_->eval(), @{$_[0]{__DIRECTIVE1__}} }

package expo ;
use List::Util  'reduce';
sub eval { reduce {$_[0] ** $_[1]} map $_->eval(), @{$_[0]{__DIRECTIVE1__}} }

package value ;
sub eval { return $_[0]{__VALUE__} }

