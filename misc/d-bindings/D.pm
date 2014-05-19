package D;

use strict;
use warnings;

use Storable;
use Digest::MD5::File qw(file_md5_hex);
use Readonly;
use Data::Dumper;
use File::Copy;
use File::Basename;

use lib('.');
use CParse;

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
    if (!defined $meth || $meth !~ m{\A [1-3]{1} \z}x);

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
      #flatten_c_header($ifil, $tfil0, $stem, \%syshdr);

      # use gcc; need a C input file
      my $cfil = "./di/$stem.h.c";
      if (-f $tfil0) {
	copy $tfil0, $cfil;
      }
      else {
	copy $ifil, $cfil;
      }

      push @tmpfils, $cfil;
      push @{$ofils_ref}, $cfil
	if $debug;

      # use g++ -E
      convert_with_gcc_E($cfil, $tfil1);

      # dress up the file and convert it to "final" form (eventually)
      convert1final($ofil, $tfil1, \%syshdr, $stem);
    }
    #==== method 2 ====
    elsif ($meth == 2) {

      # use dstep
      convert_with_dstep($tfil0, $tfil1);

      # dress up the file and convert it to "final" form (eventually)
      convert1final($ofil, $tfil1, \%syshdr, $stem);
    }
    #==== method 3 ====
    elsif ($meth == 3) {

      # use gcc; need a C input file
      my $cfil = "./di/$stem.h.c";
      copy $ifil, $cfil;
      push @tmpfils, $cfil;

      # the output file of interest will be named:
      my $tufil = "$cfil.001t.tu";
      push @tmpfils, $tufil;
      push @{$ofils_ref}, $tufil
	if $debug;

      convert_with_gcc_fdump_tu($cfil);

      # extract data into another file
      # the output file of interest will be named:
      my $pfil = "$tufil.dat";
      push @tmpfils, $pfil;
      push @{$ofils_ref}, $pfil
	if $debug;

      process_tu_file($tufil, $pfil);

      # dress up the file and convert it to "final" form (eventually)
      #convert3final($ofil, $pfil, \%syshdr, $stem);
    }

    # eliminate unneeded intermediate files;
    unlink @tmpfils
      if !$debug;

    # update hash for the new, final output file
    $curr_hash = calc_md5hash($ofil);

    if (!-f $ofil) {
      print "D::convert(): Output file '$ofil' does not exist.\n"
	if $verbose;
    }
    elsif (!$prev_hash || $prev_hash ne $curr_hash) {
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

=pod

sub process_tu_file {
  die "GCC::TranslationUnit doesn't work; author queried";

  # processes tu output for method 3
  my $tufil = shift @_;
  my $ofil  = shift @_;

  use GCC::TranslationUnit;

  die "ERROR:  tu file '$tufil' not found."
    if ! -f $tufil;

  # echo '#include <stdio.h>' > stdio.c
  # gcc -fdump-translation-unit -c stdio.c
  my $node = GCC::TranslationUnit::Parser->parsefile($tufil);
  die "ERROR:  Undefined reference \$tu"
    if (!defined $node or !$node);

  open my $fp, '>', $ofil
    or die "$ofil: $!";

  # list every function/variable name
  while ($node) {

    #print Dumper($node);

    if ($node->isa('GCC::Node::function_decl') ||
       $node->isa('GCC::Node::var_decl')) {
      printf $fp "%s declared in %s\n",
        $node->name->identifier, $node->source;
    }
    else {
      printf $fp "%s declared in %s\n",
        $node->name->identifier, $node->source;
    }

    $node = $node->chain;
  }

} # convert_tu_file

=cut

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

  # avoid multiple blank lines
  my $prev_line_was_space = 1;

  foreach my $h (sort keys %sysmod) {
    my $mod = $sysmod{$h};
    next if !$mod;
    print $fpo "import $mod;\n";
  }
  print $fpo "\n";

  open my $fpi, '<', $ifil
    or die "$ifil: $!";

  my @lines = <$fpi>;
  my $nl = @lines;


 LINE:

  for (my $i = 0; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $lines[$i];

    my @d = split(' ', $line);

    # ignore or collapse blank lines?
    if (!defined $d[0]) {
      if (!$prev_line_was_space) {
	print $fpo "\n";
	$prev_line_was_space = 1;
      }
      next LINE;
    }

    my $key = $d[0];

    if ($key =~ m{\A \s* \#}x) {
      if (1) {
	# replace cpp comment lines with a space unless prev line was a space
	if (!$prev_line_was_space) {
	  print $fpo "\n";
	  $prev_line_was_space = 1;
	}
	next LINE;
      }
      elsif (0) {
	# ignore cpp comment lines
	next LINE;
      }
    }

    if (!exists $CParse::key{$key}) {
      die "unknown key '$key' at line $lnum, file '$ifil'...";
    }

    # capture second token
    my $key2 = (1 < @d) ? $d[1] : '';
    if (!exists $CParse::key2{$key2}) {
      warn "unknown key2 '$key2' at line $lnum, file '$ifil'...";
    }

    $i = CParse::extract_unknown(\@lines, $i, $fpo);

    #print $fpo $line;
    $prev_line_was_space = 0;
    #next LINE;

=pod

    if ($key eq 'typedef') {
      if ($key2 eq 'struct') {
	#$i = CParse::extract_struct(\@lines, $i, $fpo);
	$i = CParse::extract_struct(\@lines, $i, $fpo);
	next LINE;
      }
      elsif ($key2 eq 'enum') {
#	 $i = CParse::extract_enum(\@lines, $i, $fpo);
	$i = CParse::extract_struct(\@lines, $i, $fpo);
	next LINE;
      }
      else {
	# $i = CParse::extract_typedef(\@lines, $i, $fpo);
	$i = CParse::extract_struct(\@lines, $i, $fpo);
	next LINE;
      }
    }
    elsif ($key eq 'struct') {
      $i = CParse::extract_struct(\@lines, $i, $fpo);
	next LINE;
    }
    elsif ($key eq 'enum') {
#      $i = CParse::extract_enum(\@lines, $i, $fpo);
	$i = CParse::extract_struct(\@lines, $i, $fpo);
	next LINE;
    }
    else {
      $i = CParse::extract_unknown(\@lines, $i, $fpo);
      #die "unhandled key '$key' at line $lnum, file '$ifil'...";
	next LINE;
    }

=cut

  }

  # ender
  print $fpo "\n"
    if !$prev_line_was_space;

  print $fpo "} // extern (C) {\n";

} # convert1final

sub convert_with_gcc_E {
  my $ifil = shift @_; # tfil0
  my $ofil = shift @_; # tfil1

  my $incdirs  = "-I${D::IDIR} -I${D::IDIR2}";

  my $opts = '';
  #$opts .= ' -CC'; # keep C and C++ comments
  #$opts .= ' -H'; # list includes
  $opts .= ' -v'; # report include paths
  #$opts .= ' -P'; # omit line markers

  my $cmd = "gcc -E $opts $incdirs $ifil > $ofil";
  print "debug-cmd: '$cmd'\n"
    if $debug;

  my $msg = qx($cmd);

  if ($msg) {
    chomp $msg;
    print "WARNING: msg: '$msg'\n";
  }

} # convert_with_gcc_E

sub convert_with_gcc_fdump_tu {
  my $cfil = shift @_;

  my $incdirs  = "-I${D::IDIR} -I${D::IDIR2}";

  my $ofil = "$cfil.o";

  my $cmd = "gcc -fdump-translation-unit -c $incdirs -o $ofil $cfil";
  print "debug-cmd: '$cmd'\n"
    if $debug;

  my $msg = qx($cmd);

  if ($msg) {
    chomp $msg;
    print "WARNING: msg: '$msg'\n";
  }

} # convert_with_fdump_tu

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
