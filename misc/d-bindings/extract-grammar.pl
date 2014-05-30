#!/usr/bin/env perl

# converts a grammar to a Perl module

use strict;
use warnings;

use File::Basename;
use lib('.');
my $p = basename($0);
my $usage  = "Usage:  $p go\n\n";

my $ifil = 'c-grammar.txt';
my $pm   = 'GS';
my $ofil = "${pm}.pm";

my $debug = 1;

if (!@ARGV) {
  print <<"HERE";
$usage

Extracts grammar from file '$ifil' and creates a Perl
data module named '${ofil}'.

HERE

  exit;
}

die "FATAL:  Unable to find grammar file '$ifil'.\n"
  if ! -f $ifil;

open my $fp, '<', $ifil
  or die "$ifil: $!";

my %prods        = ();
my $inprod       = 0;
my $currprod     = '';
my @currchildren = ();
while (defined(my $line = <$fp>)) {
  $line = strip_comment($line);
  my @d = split(' ', $line);
  next if !defined $d[0];

  if ($debug) {
    chomp $line;
    print "DEBUG: line '$line'\n";
  }

  my $key  = shift @d;
  my $newprod = ($key =~ s{\: \z}{}x} : 1 : 0;
  my $key2 = $newprod ? '' : shift @d;
  if ($newprod || (defined $key2 && $key2 eq ':')) {
    # beginning of a production rule with non-empty @d as subrules
    if ($inprod) {

      # if we are inprod, we need to close the current one
    }
    $inprod = 1;
  }
}

#### subroutines ####
sub strip_comment {
  my $line = shift @_;
  my $idx = index $line, '#';
  if ($idx >= 0) {
    $line = substr $line, 0, $idx;
  }
  return $line;
} # strip_comment
