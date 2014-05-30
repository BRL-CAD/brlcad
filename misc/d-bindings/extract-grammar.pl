#!/usr/bin/env perl

# converts a grammar to a Perl module

use strict;
use warnings;

use File::Basename;
use lib('.');
my $p = basename($0);
my $usage  = "Usage:  $p go [-d][-f][-h]";

my $ifil = 'c-grammar.txt';
my $pm   = 'GS';
my $ofil = "${pm}.pm";

if (!@ARGV) {
  print <<"HERE";
$usage

Use '-h' for extended help.

HERE

  exit;
}

my $debug = 1;
my $force = 1;

foreach my $arg (@ARGV) {
  if ($arg =~ m{\A -d}x) {
    $debug = 1;
  }
  elsif ($arg =~ m{\A -f}x) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -h}x) {
    longhelp();
  }
  else {
    die "FATAL:  Unknown arg '$arg'.\n";
  }
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
  if ($line =~ m{\A [\S]+ \z}x) {
    print "DEBUG:  skipping empty line '$line'.\n"
      if $debug;
    next;
  }

  if ($debug) {
    chomp $line;
    print "DEBUG: non-empty line '$line'\n";
  }

  # add some spaces on the line for more reliable tokenizing
  my $have_colon = $line =~ /:/ ? 1 : 0;
  if ($have_colon) {
    if ($debug) {
      my $s = $line;
      chomp $s;
      print "DEBUG: original line with colon:\n";
      print "  '$s'\n";
    }
    $line =~ s{([\S]*)\:}{$1 \:};
    if ($debug) {
      my $s = $line;
      chomp $s;
      print "       line with colon separated:\n";
      print "  '$s'\n";
    }
  }

  my @d = split(' ', $line);
  my $key  = shift @d;
  my $newprod = ($key =~ s{\: \z}{}x) ? 1 : 0;
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
sub longhelp {
  print <<"HERE";
$usage

  -d  Debug
  -f  Force overwriting existing files
  -h  Extended help

Extracts grammar from file '$ifil' and creates a Perl
data module named '${ofil}'.

HERE

  exit;
} # help

sub strip_comment {
  my $line = shift @_;
  my $idx = index $line, '#';
  if ($idx >= 0) {
    $line = substr $line, 0, $idx;
  }
  return $line;
} # strip_comment
