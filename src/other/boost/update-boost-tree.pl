#!/usr/bin/env perl

use strict;
use warnings;

use Data::Dumper;
use File::Basename;
use File::Copy;

#=== "collect_files" ===
# files/dirs in the current boost dir (trunk)
my %original_files = ();
my %original_dirs  = ();
# files/dirs in the new boost dir
my %boost_files = ();
my %boost_dirs  = ();

#=== updated files ===
# track deleted, new, and copied files
my %deleted_files = ();
# new files copied to BRL-CAD directory (only determined with '-make' option)
my %new_files = ();
# 'same' files
my %same_files = ();


# BRL-CAD's breakout of Boost into dirs
my $binc = 'boost';
my $blib = 'libs';

# absolute path to a working build dir
my $builddir = '/disk3/extsrc/brlcad-build-debug';

my $prog   = basename($0);
my $log    = 'update-boost-tree.log';

# the reused file for update history:
my $flist  = 'update-boost-tree-file-list.txt';

# a file generated every time collect files is run
my $nlist = 'update-boost-tree-original-file-list.txt';

if (!@ARGV) {
  print<<"HERE";
Usage: $prog <mode: -copy | -make> <new Boost source dir> [-force][-new]

norm:

Replaces all files in the existing Boost directories:

  $binc
  $blib

and replaces them with the new versions in the new
Boost source directory.

All actions are written to the log file '$log'.

File '$flist' is an updated record of all files.  Use the '-new' option
to start clean.

HERE
  exit;
}

my $debug = 0;
my $force = 0;
my $Bdir  = 0;
my $new   = 0;
# modes
my $copy  = 0;
my $make  = 0;

sub zero_args {
  $copy  = 0;
  $make  = 0;
}
foreach my $arg (@ARGV) {
  if ($arg =~ m{\A -f}xms) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -d}xms) {
    $debug = 1;
  }
  elsif ($arg =~ m{\A -n}xms) {
    $new = 1;
  }
  elsif ($arg =~ m{\A -c}xms) {
    zero_args();
    $copy = 1;
  }
  elsif ($arg =~ m{\A -m}xms) {
    zero_args();
    $make = 1;
  }
  elsif (!$Bdir) {
    $Bdir = $arg;
  }
}

if (!$copy && !$make) {
  die "You must choose a mode: '-copy' or '-make'.\n";
}
if (!$Bdir) {
  die "You must enter a new Boost source directory.\n";
}
elsif (! -d $Bdir) {
  die "ERROR:  Non-existent directory '$Bdir'.\n";
}

# name the script to be generated
my $script = "update-boost-to-$Bdir.sh";

my @of = ($log, $script);
foreach my $f (@of) {
  if (-e $f) {
    if (!$force) {
      die "ERROR:  Output file '$f' exists.  Move it or use the '-force' option.\n";
    }
    print "WARNING:  Output file '$f' is being overwritten.\n";
  }
}
open my $fp, '>', $log
  or die "$log: $!";

if (-e $flist && $new) {
  if (!$force) {
    die "ERROR:  History file '$flist' exists.  Move it or use the '-force' option.\n";
  }
  print "WARNING:  History file '$flist' is being overwritten.\n";
}

# Always collect files if $flist doesn't exist.
if (! -f $flist) {
  collect_files_and_dirs();
}
else {
  read_flist()
}

goto MAKE_ONLY if ($make);

# First we compare the current directory and files with the new: copy
# new files with the same path and delete old files not found in the
# new tree.
print $fp "Updating BRL-CAD Boost directory from new source directory:\n";
print $fp "\n  $Bdir\n\n";

# step through all the old files and replace them with new ones (or
# delete the old ones which do not have a replacement)
foreach my $f (keys %original_files) {
  if (!exists $boost_files{$f}) {
    print $fp "WARNING:  File '$f' not found in directory '$Bdir'.\n";
    $deleted_files{$f} = 1;
    if (-f $f) {
      print $fp "          Deleting old file '$f'.\n";
      unlink $f;
    }
    else {
      print $fp "          Old file '$f' already deleted.\n";
    }
    next;
  }
  else {
    $same_files{$f} = 1;
  }
  my $nf = sprintf "$Bdir/%s", $f;
  my $cmd = "cp $nf $f";
  print $fp "Cmd: $cmd\n";
  qx($cmd);
}

goto SUMMARY;

MAKE_ONLY:

# Next grossly try to build known targets and get 'fatal error' for
# missing files.  Collect the file names and copy them (also collect
# to write a script to ease a repeat).
my $tfil  = "$builddir/build_boost_tmp.log";
my $tfil2 = "$builddir/boost_tmp.txt";
RUN:
for (;;) {

  my $msg = qx(cd $builddir ; make > $tfil 2>&1 );
  $msg = qx(grep 'fatal error' $tfil > $tfil2);
  open my $fp2, '<', $tfil2
    or die "$tfil2: $!";
  my $r1 = 'fatal error: ';
  my $len1 = length $r1;
  while (defined(my $line = <$fp2>)) {
    print "DEBUG:  line: '$line'\n"
      if $debug;
    my $idx = rindex $line, $r1;
    if ($idx >= 0) {
      $idx += $len1; # should point to first character of missing file name
      my $idx2 = index $line, ':', $idx;
      if ($idx2 >= 0) {
	my $f = substr $line, $idx, $idx2 - $idx;
	print "DEBUG:  missing file: '$f'\n"
	  if $debug;
	die "?? file '$f' not found"
	  if (!-f "$Bdir/$f");

	# add the file and press on to remake
	my $dir = dirname($f);
	qx(mkdir -p $dir) if (! -d $dir);
	my $nf = sprintf "$Bdir/%s", $f;
	copy($nf, $dir);
	die "unexpected dup new file '$f'"
	  if (exists $new_files{$f});
	$new_files{$f} = 1;
	next RUN;
      }
    }
  }
  close $fp2;
  last RUN;
}
#print "DEBUG: see file '$tfil'\n"; die "debug exit";

unlink $tfil if !$debug;
unlink $tfil2 if !$debug;

# generate script


#=== END MAKE_ONLY

SUMMARY:

write_file_list($fp, $log);
close $fp;
write_file_list($fp, $flist);

print "\nNormal end.  See log file '$log'.\n";
print "Also see file lists:\n";
print "  $nlist (the original files)\n";
print "  $flist (update status)\n";

#### subroutines ####
sub collect_files_and_dirs {
  my $ohash = shift @_; # original files
  my $nhash = shift @_; # new files

  my @bincfils = qx( find $binc -type f );
  my @blibfils = qx( find $blib -type f );
  my @Bfils    = qx( find $Bdir -type f );

  foreach my $f (@bincfils, @blibfils) {
    $f = trim_string($f);
    my $dir = dirname($f);
    $original_dirs{$dir} = 1;
    $original_files{$f} = 1;
  }
  foreach my $f (@Bfils) {
    $f = trim_string($f);
    # strip off '$Bdir/'
    $f =~ s{\A $Bdir/}{}xms;
    my $dir = dirname($f);
    $boost_dirs{$dir} = 1;
    $boost_files{$f} = 1;
  }

  # output
  open my $fp, '>', $nlist
    or die "$nlist $!";
  write_file_list($fp, $nlist);

} # collect_files_and_dirs

sub read_flist {
  open my $fp, '<', $flist
    or die "$flist: $!";
  my $set = '';
  while (defined(my $line = <$fp>)) {
    $line = normalize_line($line);
    next if !$line;
    my $idx = rindex $line, ':';
    if ($idx >= 0) {
      $set = get_set($line) ;
      next;
    }
    insert_file($set, $line);
  }

} # read_flist

sub trim_string {
  my $s = shift @_;
  $s =~ s{\A \s*}{}xms;
  $s =~ s{\s* \z}{}xms;
  return $s;
} # trim_string

sub normalize_line {
  my $line = shift @_;
  my $idx = index $line, '#';
  if ($idx >= 0) {
    $line = substr $line, 0, $idx;
  }
  $line = trim_string($line);
  return $line;
} # strip_comment

sub write_file_list {
  # used for three different purposes
  my $fp    = shift @_;
  my $fname = shift @_;

  if ($fname eq $flist) {
    if (-e $flist) {
      # make a copy first
      my $bu = "$flist.backup";
      copy($flist, $bu)
	or die "Copy failed: $!";
    }
    open $fp, '>', $flist
      or die "$flist: $!";
  }

  goto LOG if ($fname eq $log);

  print $fp "# files from BRL-CAD and '$Bdir'\n";

  my @of = (sort keys %original_files);
  my $nof = @of;
  my $sof = $nof > 1 ? 's' : '';
  print $fp "# $nof file$sof from BRL-CAD\n";
  print $fp "original_files:\n";
  print $fp "  $_\n" for @of;

  my @bf = (sort keys %boost_files);
  my $nbf = @bf;
  my $sbf = $nbf > 1 ? 's' : '';
  print $fp "# $nbf file$sbf from '$Bdir'\n";
  print $fp "boost_files:\n";
  print $fp "  $_\n" for @bf;

  # end of nlist
  return if ($fname eq $nlist);

 LOG:

  my @df = (sort keys %deleted_files);
  my $ndf = @df;
  my $sdf = $ndf == 1 ? '' : 's';
  if ($fname eq $log) {
    print $fp "\nDeleted $ndf file$sdf:\n";
  }
  else {
    print $fp "# $ndf file$sdf deleted from BRL-CAD\n";
    print $fp "deleted_files:\n";
  }
  print $fp "  $_\n" for @df;

  my @nf = (sort keys %new_files);
  my $nnf = @nf;
  my $snf = $nnf == 1 ? '' : 's';
  if ($fname eq $log) {
    print $fp "\n$nnf new file$snf copied into BRL-CAD:\n";
  }
  else {
    print $fp "# $nnf new file$snf copied into BRL-CAD\n";
    print $fp "new_files:\n";
  }
  print $fp "  $_\n" for @nf;

  my @sf = (sort keys %same_files);
  my $nsf = @sf;
  my $ssf = $nsf == 1 ? '' : 's';
  if ($fname eq $log) {
    print $fp "\nUpdated $nsf file$ssf:\n";
  }
  else {
    print $fp "# $nsf updated file$ssf copied into BRL-CAD\n";
    print $fp "same_files:\n";
  }
  print $fp "  $_\n" for @sf;

  if ($fname eq $log) {
    my $ncf = $nsf + $nnf;
    my $scf = $ncf == 1 ? '' : 's';
    print "\nSummary:\n";
    print "  Copied $ncf new or updated file$scf.\n";
    print "  Deleted $ndf file$sdf.\n";

    print $fp "\nSummary:\n";
    print $fp "  Copied $ncf new or updated file$scf.\n";
    print $fp "  Deleted $ndf file$sdf.\n";
  }

} # write_file_list

sub get_set {
  my $s = shift @_;
  my $fs = '';

  # nlist AND flist ============
  if ($s eq 'original_files:') {
    $fs = 'o';
  }
  elsif ($s eq 'boost_files:') {
    $fs = 'b';
  }

  # flist only =================
  elsif ($s eq 'deleted_files:') {
    $fs = 'd';
  }
  elsif ($s eq 'new_files:') {
    $fs = 'n';
  }
  elsif ($s eq 'same_files:') {
    $fs = 's';
  }
  else {
    die "Unknown file set '$s'";
  }

  return $fs;
} # get_set

sub insert_file {
  my $s = shift @_; # file set code
  my $f = shift @_; # file name

  # nlist AND flist ============
  if ($s eq 'o') {
    my $dir = dirname($f);
    $original_dirs{$dir} = 1;
    $original_files{$f} = 1;
  }
  elsif ($s eq 'b') {
    my $dir = dirname($f);
    $boost_dirs{$dir} = 1;
    $boost_files{$f} = 1;
  }

  # flist only =================
  elsif ($s eq 'd') {
    $deleted_files{$f} = 1;
  }
  elsif ($s eq 'n') {
    $new_files{$f} = 1;
  }
  elsif ($s eq 's') {
    $same_files{$f} = 1;
  }
  else {
    die "Unknown file set '$s'";
  }
} # insert_file
