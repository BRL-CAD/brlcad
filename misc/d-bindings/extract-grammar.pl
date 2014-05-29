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

if (!@ARGV) {
  print <<"HERE";
$usage

Extracts garammar from file '' and creates a Perl
data module named '${ofil}'.

HERE

  exit;
}

die "FATAL:  Unable to find grammar file '$ifil'.\n"
  if ! -f $ifil;

open my $fp, '<', $ifil
  or die "$ifil: $!";

while (defined my $line = <$fp>)) {
  $line = strip_comment($line);
  my @d = split(' ', $line);
  next if !defined $d[0];

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
