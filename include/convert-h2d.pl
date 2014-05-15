#!/usr/bin/env perl

# converts a C .h file to a rudimentary .d file for linking C to D

use strict;
use warnings;

use File::Basename;
use Storable;
use Digest::MD5::File qw(file_md5_hex);

# use the reference D compiler
my $DMD = '/usr/bin/dmd';
die "ERROR:  Reference compiler 'dmd' not found.\n"
  if (! -f $DMD);

my $p = basename($0);
if (!@ARGV) {
  print <<"HERE";
Usage: $p -go [-force][-debug]

Converts C .h files in this directory to rudimentary .d files
  for linking C to the D language.
HERE

  exit;
}

my $force = 0;
my $debug = 0;
foreach my $arg (@ARGV) {
  my $val = undef;
  if ($arg =~ m{\A -f}xms) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -d}xms) {
    $debug = 1;
  }
}

# global vars
my $NEW  = -1;
my $SAME =  0;
my $DIFF =  1;

# collect all .h files and get the prefixes
my @h = glob("*.h");
my %h;
@h{@h} = ();
my $nh = @h;


# get status of .h files
print "There are $nh C header files in this directory:\n";
my $nnew = 0;
my $nsam = 0;
my $ndif = 0;
foreach my $f (@h) {
  my $s = file_status($f);
  $h{$f}{status} = $s;
  if ($s == $NEW) {
    ++$nnew;
  }
  elsif ($s == $SAME) {
    ++$nsam;
  }
  elsif ($s == $DIFF) {
    ++$ndif;
  }
  else {
    die "ERROR:  Unknown status for '$f'!" if ! defined $s;
  }
}
print "  new:       $nnew\n";
print "  unchanged: $nsam\n";
print "  changed:   $ndif\n";

# report status
foreach my $f (@h) {
  my $s = $h{$f}{status};
  $h{$f}{status} = file_status($f);
}

#### subroutines ####
sub file_status {
  # uses an md5 hash to determine if a file has changed (or is new)
  my $f = shift @_;

  my $hfil = ".${f}.md5hash"; # keep the last hash here
  # current hash:
  my $new_hash = file_md5_hex($f);
  my $old_hash_ref = retrieve($hfil) if -e $hfil;
  $old_hash_ref = '' if !defined $old_hash_ref;
  my $old_hash = $old_hash_ref ? $$old_hash_ref : '';

  my $status = undef;
  if (!$old_hash_ref) {
    $status = $NEW;
    #print "Note that '$f' is new or unprocessed.\n";
  }
  elsif ($old_hash eq $new_hash) {
    $status = $SAME;
    #print "Note that '$f' has not changed.\n";
  }
  else {
    store \$new_hash, $hfil;
    $status = $DIFF;
    #print "Note that '$f' has changed.\n";
  }

  die "ERROR:  Unknown status for '$f'!" if ! defined $status;

  return $status;

} # file_status
