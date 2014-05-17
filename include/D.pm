package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);
use Readonly;
use Data::Dumper;
use File::Copy;

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

# temp ignored .h files called inside top-level files
my @tignore
  = (
     'config_win.h',
     'brlcad_config.h',
    );
my %tignore = ();
@tignore{@tignore} = ();

# this is a kludge
Readonly our $IDIR => '/usr/src/tbrowde/brlcad-svn-d-binding/include';

# "mapped" files
my %is_mapped
  = (
     'tcl.h'          => "$IDIR/../src/other/tcl/generic/tcl.h",
     'tclDecls.h'     => "$IDIR/../src/other/tcl/generic/tclDecls.h",
     'tclPlatDecls.h' => "$IDIR/../src/other/tcl/generic/tclPlatDecls.h",
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

# global vars
Readonly our $NEW  => -1;
Readonly our $SAME =>  0;
Readonly our $DIFF =>  1;

#### subroutines ####
sub remove_md5hash_store {
  unlink $storefile;
  %md5table = ();
} # remove_md5hash_store

sub convert1 {
  my $ifils_ref = shift @_; # @ifils
  my $ofils_ref = shift @_; # @ofils
  my $fref      = shift @_; # %f
  my $sref      = shift @_; # %stats

  my $incdirs  = '-I. -I../src/other/tcl/generic';

  foreach my $ifil (@{$ifils_ref}) {
    my $stem = $ifil;
    $stem =~ s{\.h \z}{}x;

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
    my $ofil = $stem . '.d';
    $prev_hash = retrieve_md5hash($ofil);
    if (!$prev_hash) {
      $process = 1;
    }

    # don't overwrite
    if (-e $ofil && !$force) {
      $process = 0;
    }

    if (!$process) {
      print "D::convert1(): Skipping input file '$ifil'...\n"
	if $verbose;
      next;
    }

    print "D::convert1(): Processing file '$ifil' => '$ofil'...\n"
      if $verbose;

    # get rid of system headers (but record their use)
    my %syshdr = ();

    # container for tmp files
    my @tmpfils = ();

    # first intermediate file
    my $tfil0 = $stem . '.inter0';
    push @tmpfils, $tfil0;
    open $fpo, '>', $tfil0
      or die "$tfil0: $!";

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

  LINE:
    while (defined(my $line = <$fpi[$level]>)) {
      if ($line =~ m{$incregex}x) {
	my $s = $1;
	chomp $s;
	$s =~ s{\A \s*}{}x;
	$s =~ s{\s* \z}{}x;
	$s =~ s{\A \"}{}x;
	$s =~ s{\" \z}{}x;
	#print "debug: \$1 (\$s) = '$s'\n";

	# get a new file
	my $f = $s;

        # ignore <> files
	if (is_syshdr($f)) {
	  print "WARNING: ignoring sys include file '$f' for now...\n";
	  $syshdr{$s} = 1;
	  # don't print
	  next LINE;
	}

	# some files are ignored for now
        if (exists $tignore{$f}) {
	  print "WARNING: ignoring include file '$f' for now...\n";
	  next LINE;
	}

        # may be a "mapped" file
	if (exists $is_mapped{$f}) {
	  my $ff = $is_mapped{$f};
	  print "WARNING: using mapped file '$f' => '$ff' for now...\n";
	  $f = $ff;
	}

	# we try to follow this include file
	die "ERROR:  Include file '$f' (in '$parfils[$level]') not found."
	  if !-f $f;
	print "DEBUG:  opening include file '$f' (in '$parfils[$level]')\n";
	open $fpi[++$level], '<', $f
	  or die "$f: $!";
        $parfils[$level] = $f;
	next LINE;
      }

      print $fpo $line;
    }
    # work back up the file tree as we find EOF
    if ($level > 0) {
      --$level;
      goto LINE;
    }

    push @{$ofils_ref}, $tfil0
      if $debug;

    #print Dumper(\%syshdr); die "debug exit";

    # second intermediate file
    my $tfil1 = $stem . '.inter1';
    push @tmpfils, $tfil1;
    push @{$ofils_ref}, $tfil1
      if $debug;

    my $msg = qx(gcc -E -x c $incdirs -o $tfil1 $tfil0);

    # dress up the file and convert it to "final" form (eventually)
    convert1final($ofil, $tfil1, \%syshdr, $stem);

    # eliminate unneeded intermediate files;
    unlink @tmpfils
      if !$debug;

    # update hash for the new, final output file
    $curr_hash = calc_md5hash($ofil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      if ($verbose) {
	print "D::convert1(): Output file '$ofil' hashes:\n";
	print "  prev '$prev_hash'\n";
	print "  curr '$curr_hash'\n";
      }
      store_md5hash($ofil, $curr_hash);
      push @{$ofils_ref}, $ofil;
    }
    else {
      print "D::convert1(): Output file '$ofil' has not changed.\n"
	if $verbose;
    }
  }

} # convert1

sub calc_md5hash {
  my $f = shift @_;
  return file_md5_hex($f);
} # calc_md5hash

sub store_md5hash {
  # store an input hash (or calculates a new one) and stores it
  my $f       = shift @_;
  my $md5hash = shift @_;

  $md5hash = calc_md5hash($f) if !defined $md5hash;

  my $tabref = retrieve($storefile) if -e $storefile;
  $tabref = \%md5table if !defined $tabref;
  $tabref->{$f} = $md5hash;
  store $tabref, $storefile;

  if ($debug) {
    print Dumper($tabref); die "debug exit";
  }
} # store_md5hash

sub retrieve_md5hash {
  # return stored file hash or 0 if not found in hash
  my $f = shift @_;

  my $tabref = retrieve($storefile) if -e $storefile;
  $tabref = \%md5table if !defined $tabref;

  my $md5hash = 0;
  if (exists $tabref->{$f}) {
    $md5hash = $tabref->{$f};
  }

  return $md5hash;

} # retrieve_md5hash

sub collect_files {
  # deletes generated files if $D::clean
  my $href  = shift @_; # @h
  my $diref = shift @_; # @di

  my @di = glob("*.di");
  if ($clean) {
    unlink @di;
    remove_md5hash_store();
  }
  else {
    push @{$diref}, @di;
  }


  my @h  = glob("*.h");
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

  foreach my $h (sort keys %sysmod) {
    my $mod = $sysmod{$h};
    next if !$mod;
    print $fpo "import $mod;\n";
  }
  print $fpo "\n";

  open my $fpi, '<', $ifil
    or die "$ifil: $!";
  while (defined(my $line = <$fpi>)) {
    # ignore cpp comment lines
    next if ($line =~ m{\A \s* \#}x);

    # ignore blanklines
    my @d = split(' ', $line);
    next if !defined $d[0];

    print $fpo $line;
  }

} # convert1final

# mandatory true return from a Perl module
1;
