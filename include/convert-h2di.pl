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
my $usage = "Usage: $p mode [options...]";

if (!@ARGV) {
  print <<"HERE";
$usage

Use option '-h' for details.

Converts C .h files in this directory to rudimentary .d files
  for linking C to the D language.
HERE

  exit;
}

my $force = 0;
my $debug = 0;

# modes
my $report  = 0;
my $convert = 0;

sub zero_modes {
  $report   = 0;
  $convert1 = 0;
} # zero_modes

foreach my $arg (@ARGV) {
  my $val = undef;
  my $idx = index $val, '=';
  if ($idx >= 0) {
    print "DEBUG: input arg is '$arg'\n"
      if $debug;

    $val = substr $arg, $idx+1;
    $arg = substr $arg, 0, $idx;
    if ($debug) {
      print "arg is now '$arg', val is '$val'\n";
      die "debug exit";x
    }
  }

  if ($arg =~ m{\A -f}xms) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -d}xms) {
    $debug = 1;
  }
  elsif ($arg =~ m{\A -h}xms) {
    help();
  }

  # modes
  elsif ($arg =~ m{\A -r}xms) {
    zero_modes();
    $report = 1;
  }
  elsif ($arg =~ m{\A -c1}xms) {
    zero_modes();
    $c1 = 1;
  }

  # error
  else {
    die "ERROR:  Unknown arg '$arg'.\n";
  }
}

# global vars
my $NEW  = -1;
my $SAME =  0;
my $DIFF =  1;

# collect all .h files and get the prefixes
my @h = glob("*.h");
my $nh = @h;
# ditto for .d files
my @d = glob("*.d");
my $nd = @d;
my @fils = (@h, @d);
my $nf = @fils;
my %f;
@f{@fils} = ();
my %stats = ();
get_status(\%stats, \%f);

# get status of .h files
print "There are $nh C header files in this directory:\n";
print "  new:       $stats{h}{new}\n";
print "  unchanged: $stats{h}{sam}\n";
print "  changed:   $stats{h}{dif}\n";
print "There are $nd D header files in this directory:\n";
print "  new:       $stats{d}{new}\n";
print "  unchanged: $stats{d}{sam}\n";
print "  changed:   $stats{d}{dif}\n";

#### subroutines ####
sub get_status {
  my $sref = shift @_; # \%stats
  my $fref = shift @_; # \%f

  $sref->{h}{new} = 0;
  $sref->{h}{sam} = 0;
  $sref->{h}{dif} = 0;

  $sref->{d}{new} = 0;
  $sref->{d}{sam} = 0;
  $sref->{d}{dif} = 0;

  foreach my $f (keys %{$fref}) {
    my $s = file_status($f);
    my $typ = $f =~ m{\.h \z}xms ? 'h' : 'd';
    die "ERROR:  Uknown file type for file '$f'!"
      if ($typ eq 'd' && $f !~ m{\d \z}xms);

    $fref->{$f}{status} = $s;
    $fref->{$f}{typ}    = $typ;

    if ($s == $NEW) {
      ++$sref->{$typ}{new};
    }
    elsif ($s == $SAME) {
      ++$sref->{$typ}{sam};
    }
    elsif ($s == $DIFF) {
      ++$sref->{$typ}{dif};
    }
    else {
      die "ERROR:  Unknown status for '$f'!" if ! defined $s;
    }
  }
} # get_status

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

sub help {
  print <<"HERE";
$usage

modes:

  -r    report status of .h and .d file in the directory
  -c1   use method 1 to convert .h files to .d files

  -h=X  restrict conversion attemp to just file X

options:

  -f    force overwriting files
  -d    debug

notes:

  1.  The convert options will normall convert all known .h files to
      .d files only if they have no existing .d file.

  2.  Excptions to note 1:

      + Use of the '-h=X' option restricts the set of .h files to be
        considered to the single file X.

      + Use of the '-f' option will allow overwriting any existing .d
        file.


HERE

  exit;
} # help
