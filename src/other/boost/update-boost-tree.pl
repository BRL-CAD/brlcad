#!/usr/bin/env perl

use strict;
use warnings;

use Data::Dumper;
use File::Basename;

# the current boost dir
my %curr_files = ();
# the new boost dir
my %new_files  = ();

my $binc = 'boost';
my $blib = 'libs';

my $prog = basename($0);
if (!@ARGV) {
  print<<"HERE";
Usage: $prog <new Boost source dir> [-force]

Replaces all files in the existing Boost directories:

  $binc
  $blib

and replaces them with the new versions in the new
Boost source directory.
HERE
  exit;
}

my $debug = 0;
my $force = 0;
my $Bdir  = 0;
foreach my $arg (@ARGV) {
  if ($arg =~ m{\A -f}xms) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -d}xms) {
    $debug = 1;
  }
  elsif (!$Bdir) {
    $Bdir = $arg;
  }
}

if (! -d $Bdir) {
  die "ERROR:  Non-existent directory '$Bdir'.\n";
}

my @bincfils = qx( find $binc -type f );
my @blibfils = qx( find $blib -type f );
my @Bfils = qx( find $Bdir -type f );

my @t;

@t = ();
foreach my $f (@bincfils) {
  $f =~ s{\A \s*}{}xms;
  $f =~ s{\s* \z}{}xms;
  push @t, $f;
}
@bincfils = @t;

@t = ();
foreach my $f (@blibfils) {
  $f =~ s{\A \s*}{}xms;
  $f =~ s{\s* \z}{}xms;
  push @t, $f;
}
@blibfils = @t;

@t = ();
foreach my $f (@Bfils) {
  $f =~ s{\A \s*}{}xms;
  $f =~ s{\s* \z}{}xms;
  # strip off '$Bdir/'
  $f =~ s{\A $Bdir/}{}xms;
  push @t, $f;
}
@Bfils = @t;

#print Dumper(\@bincfils); die "debug exit";
#print Dumper(\@blibfils); die "debug exit";
#print Dumper(\@Bfils); die "debug exit";

# break all files into hashes
# first the current files
my %ofils = ();
foreach my $f (@bincfils, @blibfils) {
  $ofils{$f} = 1;
}
# then the new ones
my %nfils = ();
foreach my $f (@Bfils) {
  $nfils{$f} = 1;
}

# step through all the old files and replace them with new ones (or
# delete the old ones which do not have a replacement)
my %deletes = ();
foreach my $f (keys %ofils) {
  if (!exists $nfils{$f}) {
    print "WARNING:  File '$f' not found in directory '$Bdir'.\n";
    $deletes{$f} = 1;
    if (-f $f) {
      print "          Deleting old file '$f'.\n";
      unlink $f;
    }
    else {
      print "          Old file '$f' already deleted.\n";
    }
    next;
  }
  my $nf = sprintf "$Bdir/%s", $f;
  my $cmd = "cp $nf $f";
  print "Cmd: $cmd\n";
  qx($cmd);
}
