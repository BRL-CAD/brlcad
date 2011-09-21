#!/usr/bin/env perl

use strict;
use warnings;

use File::Basename;
use File::Find;

my $vfil = 'db-file-list.txt';
my @dirs
  = (
     'articles',
     'books',
     'lessons',
     'presentations',
     'specifications',
     'system',
    );

my $prog = basename($0);
if (!@ARGV) {
  print <<"HERE";
Usage: $prog --go [--force]

Finds all DocBook files in known sub-directories and writes results
  to file '$vfil'.
No existing file is overwritten unless the '--force' option is used.
HERE
  exit;
}

my $force = 0;
foreach my $arg (@ARGV) {
  if ($arg =~ /^[-]{1,2}f/i) {
    $force = 1;
  }
  elsif ($arg =~ /^[-]{0-2}g/i) {
    ; # go
  }
  else {
    die "ERROR:  Option '$arg' is unknown.\n";
  }
}

if (-f && !$force) {
  die "ERROR:  File '$vfil' exists...molve it or use the '--force' option.\n";
}

open my $fp, '>', $vfil
  or die "$vfil: $!\n";

my @fils = ();
find({wanted => \&findsub}, @dirs);

@fils = sort @fils;
print $fp "$_\n" for @fils;

print "Normal end.  See DB file list '$vfil'.\n";

#### SUBROUTINES ####

sub findsub {
  my $f = $File::Find::name;
  if ($f =~ m{\.xml \z}xmsi) {
    my $ff = basename($f);
    return if $ff =~ m{\A \.}xmsi;
    push @fils, $f;
  }
}

