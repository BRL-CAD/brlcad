package BP; # BRL-CAD paths and associated info

use strict;
use warnings;

use Readonly;
use File::Find;

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

sub get_brlcad_incdirs {
  # get all include paths and files under the $RDIR/include directory

  find(\&_process_file, ("$RDIR/include"));

  die "debug exit";

} # get_brlcad_incdirs

sub _process_file {
  my $path = $File::Find::name; # full path
  my $fdir = $File::Find::dir;
  my $fnam = $_;

  if (1) {
    print "=== in dir '$fdir'\n";
    print "   path: '$path'\n";
    print "   file: '$fnam'\n";
  }
} # _process_file

# mandatory true return from a Perl module
1;


