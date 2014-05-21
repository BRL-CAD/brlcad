package BP; # BRL-CAD paths and associated info

use strict;
use warnings;

use Readonly;
use File::Find;
use File::Basename;

# The following defs are for use just during the experimental
# phase. They will have to be handled differently during devlopment
# and installation.  For instance, the final .d files may be installed
# as both source (maybe in a sub-dir of the normal include dir) and as
# dmd-compiled versions (say in a 'dmd-obj' installed sub-dir).

# the root dir of the installed BRL-CAD package
Readonly our $RDIR => '/usr/brlcad/rel-7.25.0';

# the source dir of the installed BRL-CAD include files
Readonly our $IDIR => "$RDIR/include/brlcad";

# the source dirs of some other installed BRL-CAD include files
Readonly our $IDIR2 => "$RDIR/include";

# the location for the D interface files
Readonly our $DIDIR => './di';

# used during collecting info and writing modules
our $debug = 0;
my %seen = ();
my %dir  = ();

# need absolute paths

# list of BRL-CAD installed headers
#   key: basename
#   val: path
my $tfil  = '/usr/src/tbrowde/brlcad-svn-d-binding/misc/d-bindings/BH.pm.new';

# list of BRL-CAD external headers called by the installed headers
#   key: basename <X/Y/Z>
#   val: path
my $tfil2 = '/usr/src/tbrowde/brlcad-svn-d-binding/misc/d-bindings/BE.pm.new';

=pod

sub get_brlcad_ext_inc_data {
  # get all external include paths and files called by the BRL-CAD headers
  use BH;
  my $oref  = shift @_;
  my $debug = shift @_;
  $BP::debug = (defined $debug && $debug) ? 1 : 0;

} # get_brlcad_ext_inc_data

=cut

sub get_brlcad_inc_data {
  # get all include paths and files under the $RDIR/include directory
  my $oref  = shift @_;
  my $debug = shift @_;
  $BP::debug = (defined $debug && $debug)? 1 : 0;

  # initialize the module
  open my $fp, '>', "$tfil"
    or die "$tfil: $!";
  print $fp "package BH;\n\n";
  print $fp "# WARNING:  This file is auto-generated: edits will be lost!!\n\n";
  print $fp "# Following is a list of all public and private\n";
  print $fp "# headers found in the installed BRL-CAD package.\n\n";
  print $fp "our %hdr = (\n";
  close $fp;

  find(\&_process_file, ("$RDIR/include"));

  # finalize the module
  open $fp, '>>', $tfil
    or die "$tfil: $!";

  # end the hdr hash
  print $fp ");\n\n";

  # write the path hash
  print $fp "our %hdr = (\n";
  foreach my $d (keys %dir) {
  }

  print $fp "# mandatory true return for a Perl module\n";
  print $fp "1;\n";

  close $fp;

  push @{$oref}, basename($tfil);

  #print "See file '$tfil'\n"; die "debug exit";

} # get_brlcad_inc_data

sub _process_file {
  my $path = $File::Find::name; # full path
  my $fdir = $File::Find::dir;
  my $fnam = $_;

  if (-d $fnam) {
      $dir{$path} = $fnam;
  }
  else {
    if ($BP::debug) {
      print "=== in dir '$fdir'\n";
      print "   path: '$path'\n";
      print "   file: '$fnam'\n";
    }

    # check for dups
    if (!exists $seen{$fnam}) {
      $seen{$fnam} = $path;
    }
    else {
      print "WARNING:  File: '$fnam'\n";
      print "                '$path' is a dup\n";
      print "          Prev: '$seen{$fnam}'\n";
    }

    # save data
    open my $fp, '>>', $tfil
      or die "$tfil: $!";
    print $fp "  '$fnam' => '$path',\n";
    close $fp;
  }

} # _process_file

# mandatory true return from a Perl module
1;


