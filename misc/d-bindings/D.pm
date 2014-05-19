package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);
use Readonly;
use Data::Dumper;
use File::Copy;
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
# the source dir of some other installed BRL-CAD include files
Readonly our $IDIR2 => "$RDIR/include";
# the location for the D interface files
Readonly our $DIDIR => './di';

# file type suffixes
Readonly our $Hsuf => '.h';
Readonly our $Dsuf => '.d';


# local vars
my $storefile = '.md5tablestore';
# key md5 file hases by file name in this table;
my %md5table;

# ignored top-level original .h files
my @ignore
  = (
     'conf.h',
     'dvec.h',
     'redblack.h',
    );
my %ignore = ();
@ignore{@ignore} = ();

# temp ignored .h files called inside top-level files (may be
# permanent, they are ostensably for CMake)
my @tignore
  = (
     'config_win.h',
     'brlcad_config.h',
    );
my %tignore = ();
@tignore{@tignore} = ();

# "mapped" files (header files not in the main BRL-CAD header dir)
my %is_mapped
  = (
     'tcl.h'          => "$IDIR2/tcl.h",
     'tclDecls.h'     => "$IDIR2/tclDecls.h",
     'tclPlatDecls.h' => "$IDIR2/tclPlatDecls.h",
    );

# sys headers and their equivalen Phobos module names form import
# statements; empty is unknown or don't use
my %sysmod
  = (
          '<tchar.h>'     => '',,
          '<wchar.h>'     => '',,
          '<sys/types.h>' => '',,
          '<dlfcn.h>'     => '',,
          '<signal.h>'    => '',,
          '<setjmp.h>'    => '',,
          '<stdint.h>'    => '',,
          '<limits.h>'    => '',,
          '<string.h>'    => '',,
          '<stdlib.h>'    => '',,
          '<stdarg.h>'    => '',,
          '<stddef.h>'    => '',,
          '<stdio.h>'     => 'std.stdio',
    );

#==========================================================================
# vars for export
our $force   = 0;
our $verbose = 0;
our $debug   = 0;
our $clean   = 0;
our $devel   = 1;

# global vars
Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

#### subroutines ####
sub remove_md5hash_store {
  unlink $storefile;
  %md5table = ();
} # remove_md5hash_store

sub convert {
  my $ifils_ref = shift @_; # @ifils
  my $ofils_ref = shift @_; # @ofils
  my $fref      = shift @_; # %f
  my $sref      = shift @_; # %stats
  my $meth      = shift @_; # meth: 1 or 2

  die "ERROR:  Unknown conversion method $meth."
    if (!defined $meth || !($meth == 1 || $meth == 2));

  foreach my $ifil (@{$ifils_ref}) {
    my $stem = basename $ifil;
    $stem =~ s{$Hsuf \z}{}x;

    my ($process, $stat) = (0, 0);
    my ($curr_hash, $prev_hash, $fpo);

    # check to see current status of input file
    $curr_hash = file_md5_hex($ifil);
    $prev_hash = retrieve_md5hash($ifil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      $process = 1;
      store_md5hash($ifil, $curr_hash);
    }
    $process = 1
      if ($force);

    # final output file
    my $ofil = "${D::DIDIR}/${stem}${Dsuf}";
    $prev_hash = retrieve_md5hash($ofil);
    if (!$prev_hash) {
      $process = 1;
    }

    # don't overwrite
    if (-e $ofil && !$force) {
      $process = 0;
    }

    if (!$process) {
      print "D::convert(): Skipping input file '$ifil'...\n"
	if $verbose;
      next;
    }

    print "D::convert(): Processing file '$ifil' => '$ofil'...\n"
      if $verbose;

    # container for tmp files
    my @tmpfils = ();

    # get rid of system headers (but record their use)
    my %syshdr = ();

    # first intermediate file
    my $tfil0 = "${D::DIDIR}/${stem}.inter0";
    push @tmpfils, $tfil0;
    push @{$ofils_ref}, $tfil0
	if $debug;

    # second intermediate file
    my $tfil1 = "${D::DIDIR}/${stem}.inter1";
    push @tmpfils, $tfil1;
    push @{$ofils_ref}, $tfil1
	if $debug;

    #==== method 1 ====
    if ($meth == 1) {

      # insert unique included files into the single input file
      flatten_c_header($ifil, $tfil0, $stem, \%syshdr);

      # use g++ -E
      convert_with_gcc($tfil0, $tfil1);

      # dress up the file and convert it to "final" form (eventually)
      convert1final($ofil, $tfil1, \%syshdr, $stem);
    }
    #==== method 1 ====
    elsif ($meth == 2) {

      # use dstep
      convert_with_dstep($tfil0, $tfil1);

      # dress up the file and convert it to "final" form (eventually)
      convert1final($ofil, $tfil1, \%syshdr, $stem);
    }

    # eliminate unneeded intermediate files;
    unlink @tmpfils
      if !$debug;

    # update hash for the new, final output file
    $curr_hash = calc_md5hash($ofil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      if ($verbose) {
	print "D::convert(): Output file '$ofil' hashes:\n";
	print "  prev '$prev_hash'\n";
	print "  curr '$curr_hash'\n";
      }
      store_md5hash($ofil, $curr_hash);
      push @{$ofils_ref}, $ofil;
    }
    else {
      print "D::convert(): Output file '$ofil' has not changed.\n"
	if $verbose;
    }

  } # iterate over input files

} # convert

sub calc_md5hash {
  my $f = shift @_;
  return file_md5_hex($f);
} # calc_md5hash

sub store_md5hash {
  # store an input hash (or calculates a new one) and stores it
  my $f       = shift @_;
  my $md5hash = shift @_;

  $md5hash = calc_md5hash($f)
    if !defined $md5hash;

  my $tabref = retrieve($storefile)
    if -e $storefile;
  $tabref = \%md5table
    if !defined $tabref;
  $tabref->{$f} = $md5hash;
  store $tabref, $storefile;

  if (0 && $debug) {
    print Dumper($tabref); die "debug exit";
  }
} # store_md5hash

sub retrieve_md5hash {
  # return stored file hash or 0 if not found in hash
  my $f = shift @_;

  my $tabref = retrieve($storefile)
    if -e $storefile;
  $tabref = \%md5table
    if !defined $tabref;

  my $md5hash = 0;
  $md5hash = $tabref->{$f}
    if (exists $tabref->{$f});

  return $md5hash;

} # retrieve_md5hash

sub collect_files {
  # deletes generated files if $D::clean
  my $href = shift @_; # @h
  my $dref = shift @_; # @d

  my @d = glob("${D::DIDIR}/*${Dsuf}");
  if ($clean) {
    unlink @d;
    remove_md5hash_store();
  }
  else {
    push @{$dref}, @d;
  }

  my @h  = glob("${D::IDIR}/*${Hsuf}");
  foreach my $f (@h) {
    next if exists $ignore{$f};
    push @{$href}, $f;
  }

} # collect_files

sub is_syshdr {
  my $f = shift @_;
  return 1 if ($f =~ m{\A \s* \< [\S]+ \> \s* \z}x);
  return 0;
} # is_syshdr

sub convert1final {
  my $ofil = shift @_; # $ofil
  my $ifil = shift @_; # $tfil1
  my $sref = shift @_; # \%syshdr
  my $stem = shift @_; # stem of .h file name (e.g., stem of 'bu.h' is 'bu'

  open my $fpo, '>', $ofil
    or die "$ofil: $!";

  print $fpo "module $stem;\n";
  print $fpo "\n";

  print $fpo "extern (C) {\n";
  print $fpo "\n";

  foreach my $h (sort keys %sysmod) {
    my $mod = $sysmod{$h};
    next if !$mod;
    print $fpo "import $mod;\n";
  }
  print $fpo "\n";

  open my $fpi, '<', $ifil
    or die "$ifil: $!";
  while (defined(my $line = <$fpi>)) {

=pod

    # ignore cpp comment lines
    next if ($line =~ m{\A \s* \#}x);

=cut

=pod

    # ignore blanklines
    my @d = split(' ', $line);
    next if !defined $d[0];

=cut

    print $fpo $line;
  }

  # ender
  print $fpo "\n";
  print $fpo "} // extern (C) {\n";

} # convert1final

sub convert_with_gcc {
  my $ifil = shift @_; # tfil0
  my $ofil = shift @_; # tfil1

  my $incdirs  = "-I${D::IDIR} -I${D::IDIR2}";

  my $opts = '';
  #$opts .= ' -CC'; # keep C++ comments
  #$opts .= ' -H'; # list includes
  $opts .= ' -v'; # report include paths
  $opts .= ' -P'; # omit line markers

  my $msg = qx(gcc -E -x c $opts $incdirs -o $ofil $ifil);

  if ($msg) {
    chomp $msg;
    print "WARNING: msg: '$msg'\n";
  }

} # convert_with_gcc

sub convert_with_dstep {
  my $ifil = shift @_; # tfil0
  my $ofil = shift @_; # tfil1

  # usage: dstep Foo.h -o Foo.d

  my $msg = qx(dstep $ifil -o $ofil);

  if ($msg) {
    chomp $msg;
    print "WARNING: msg: '$msg'\n";
  }

} # convert_with_dstep

sub flatten_c_header {
  # insert unique included files into the single input file
  my $ifil      = shift @_; # $ifil
  my $ofil      = shift @_; # $tfil0
  my $stem      = shift @_; # stem of .h file name (e.g., stem of 'bu.h' is 'bu'
  my $sysref    = shift @_; # \%syshdr

  open my $fpo, '>', $ofil
    or die "$ofil: $!";

  my $incregex = qr/\A \s* \# \s* include \s+
		    (
		      (\<[a-zA-Z0-9\._\-\/]{3,}\>)
		    |
		      (\"[a-zA-Z0-9\._\-\/]{3,}\")
		    ) \s*/x;

  my (@fpi, @parfils) = ();

  # the starting file
  my $level = 0;
  open $fpi[$level], '<', $ifil
    or die "$ifil: $!";
  push @parfils, $ifil;

  if (0 && $debug) {
    # use g++ -E
    my $tfil = 'temp-bu-cpp-file.txt';
    convert_with_gcc($ifil, $tfil);
    die "debug exit: see tmp file '$tfil'";
  }

  # record its source
  print $fpo "//===========================================================================\n";
  print $fpo "// beginning input lines (level $level) from '$ifil'\n";
  print $fpo "//===========================================================================\n";

  # don't repeat included files
  my %seen = ();
  $seen{$ifil} = 1;

 LINE:
  while (defined(my $line = readline $fpi[$level])) {
    if (0 && $debug) {
      my $s = $line;
      chomp $s;
      print "debug: line = '$s'\n";
    }
    if ($line =~ m{$incregex}x) {
      my $s = $1;
      chomp $s;
      $s =~ s{\A \s*}{}x;
      $s =~ s{\s* \z}{}x;
      $s =~ s{\A \"}{}x;
      $s =~ s{\" \z}{}x;
      print "debug: \$1 (\$s) = '$s'\n"
	if (0 && $debug);

      # get a new file
      my $f = $s;

      # ignore <> files
      if (is_syshdr($f)) {
	print "WARNING: ignoring sys include file '$f' for now...\n"
	  if (0 && $debug);
	$sysref->{$s} = 1;
	# don't print
	next LINE;
      }

      # some files are ignored for now
      if (exists $tignore{$f}) {
	print "WARNING: ignoring include file '$f' for now...\n"
	  if (0 && $debug);
	next LINE;
      }

      # may be a "mapped" file
      if (exists $is_mapped{$f}) {
	my $ff = $is_mapped{$f};
	print "WARNING: using mapped file '$f' => '$ff' for now...\n"
	  if (0 && $debug);
	$f = $ff;
      }

      # one more shot for things in the subdir of $IDIR like './bu/bu.h'
      if (!-f $f) {
	# the following two formats are the same but confuse the 'seen' hash:
	#   'bu/bu_vlist.h'
	#   './bu/bu_vlist.h'
	my $s = $f;
	$s =~ s{\A \./}{}x;
	my $ff = "${D::IDIR}/${s}";
	print "NOTE: using file '$f' => '$ff'...\n"
	  if (0 && $debug);
	$f = $ff;
      }

      # but don't include it if we've seen it before
      next LINE if (exists $seen{$f});
      # record it as seen
      $seen{$f} = 1;

      # we try to follow this include file
      die "ERROR:  Include file '$f' (in '$parfils[$level]') not found."
	if !-f $f;
      print "DEBUG:  opening include file '$f' (in '$parfils[$level]')\n"
	if $debug;
      open $fpi[++$level], '<', $f
	or die "$f: $!";
      $parfils[$level] = $f;

      # record its source
      print $fpo "//===========================================================================\n";
      print $fpo "// included input lines (level $level) from '$f'\n";
      print $fpo "//===========================================================================\n";

      next LINE;
    }

    print $fpo $line;
  }

  # work back up the file tree as we find EOF
  if ($level > 0) {
    --$level;
    if (!defined $fpi[$level]) {
      print "ERROR:  At level $level, file pointer is not defined!\n";
    }
    goto LINE;
  }

  #print Dumper(\@parfils); die "debug exit";

  #print Dumper(\%syshdr); die "debug exit";
} # flatten_c_header

sub get_c_statement {
} # get_c_statement

# mandatory true return from a Perl module
1;
