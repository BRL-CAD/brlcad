package D;

use v5.14;

use strict;
use warnings;

use Data::Dumper;
use File::Copy;
use File::Basename;

use lib('.');

use G;
use CExtract;
use BP;
use DS;
use ParsePPCHeader;
use HObj;

# ignored top-level original .h files
my @ignore
  = (
     # don't use these at the moment
     #'conf.h',
     'dvec.h',
     'redblack.h',
    );
my %ignore = ();
@ignore{@ignore} = ();

# temp ignored .h files called inside top-level files (may be
# permanent, they are ostensably for CMake)
my @tignore
  = (
     # don't use these at the moment
     #'config_win.h',
     #'brlcad_config.h',
    );
my %tignore = ();
@tignore{@tignore} = ();

# "mapped" files (header files not in the main BRL-CAD header dir)
my %is_mapped
  = (
     'tcl.h'          => "$BP::IDIR2/tcl.h",
     'tclDecls.h'     => "$BP::IDIR2/tclDecls.h",
     'tclPlatDecls.h' => "$BP::IDIR2/tclPlatDecls.h",
    );

# sys headers and their equivalent Phobos module names form import
# statements; empty is unknown or don't use
my %sysmod
  = (
     # don't use these at the moment
     #'<tchar.h>'     => '',,
     #'<wchar.h>'     => '',,
     #'<sys/types.h>' => '',,
     #'<dlfcn.h>'     => '',,
     #'<signal.h>'    => '',,
     #'<setjmp.h>'    => '',,
     #'<stdint.h>'    => '',,
     #'<limits.h>'    => '',,
     #'<string.h>'    => '',,
     #'<stdlib.h>'    => '',,
     #'<stdarg.h>'    => '',,
     #'<stddef.h>'    => '',,
     #'<stdio.h>'     => 'std.stdio',
    );

#### subroutines ####

sub convert {
  my $ifils_ref = shift @_; # @ifils
  my $ofils_ref = shift @_; # @ofils
  my $fref      = shift @_; # %f
  my $sref      = shift @_; # %stats
  my $meth      = shift @_; # meth: 1 or 2

  die "ERROR:  Unknown conversion method $meth."
    if (!defined $meth || $meth !~ m{\A [1-3]{1} \z}x);

  # the input files array should contain C header files to be
  # processed (typically with a .h suffix)

  foreach my $ifil (@{$ifils_ref}) {

    my $stem = basename $ifil;
    $stem =~ s{$G::Hsuf \z}{}x;

    my ($process, $stat) = (0, 0);
    my ($curr_hash, $prev_hash, $fpo);

    # check to see current status of input file
    $curr_hash = DS::file_md5_hex($ifil);
    $prev_hash = DS::retrieve_md5hash($ifil);

    if (!$prev_hash || $prev_hash ne $curr_hash) {
      $process = 1;
      DS::store_md5hash($ifil, $curr_hash);
    }
    $process = 1
      if ($G::force);

    # final output file
    my $ofil = "${BP::DIDIR}/${stem}${G::Dsuf}";
    $prev_hash = DS::retrieve_md5hash($ofil);
    if (!$prev_hash) {
      $process = 1;
    }

    # don't overwrite
    if (-e $ofil && !$G::force) {
      $process = 0;
    }

    if (!$process) {
      print "D::convert(): Skipping input file '$ifil'...\n"
	if $G::verbose;
      next;
    }

    print "D::convert(): Processing file '$ifil' => '$ofil'...\n"
      if $G::verbose;

    # container for tmp files
    my @tmpfils = ();

    # get rid of system headers (but record their use)
    my %syshdr = ();

    # intermediate file after gcc preprocessing
    my $ppfil = "${BP::DIDIR}/${stem}.i";
    push @tmpfils, $ppfil;
    push @{$ofils_ref}, $ppfil
      if $G::debug;

    #==== method 1 ====
    if ($meth == 1) {

=pod

      # insert unique included files into the single input file
      flatten_c_header($ifil, $tfil0, $stem, \%syshdr);

=cut

      # use gcc; need a C input file
      my $cinfil = "./di/$stem.h.c";
      push @tmpfils, $cinfil;
      push @{$ofils_ref}, $cinfil
	if $G::debug;

      # note: handling of GCC constructs '__atribute__',
      # -__restrict', '__extension__', et al.:
      # Compatibility with non-GNU compilers
      # ====================================
      # Fortunately, the __attribute__ mechanism was cleverly designed in a
      # way to make it easy to quietly eliminate them if used on platforms
      # other than GNU C. Superficially, __attribute__ appears to have
      # multiple parameters (which would typically rule out using a macro),
      # but the two sets of parentheses effectively make it a single
      # parameter, and in practice this works very nicely.
      #
      # /* If we're not using GNU C, elide __attribute__ */
      # #ifndef __GNUC__
      # #  define  __attribute__(x)  /*NOTHING*/
      # #endif

      open my $fp, '>', $cinfil
        or die "$cinfil: $!";

      # some prelims:
      print $fp "/* HEADER ADDITION FOR D LANGUAGE INTERFACE:\n";
      print $fp "\n";
      print $fp "   Since we're not using GNU C output, elide unused\n";
      print $fp "   funcstions (or macros) '__attribute__', '__extension__',\n";
      print $fp "   and '__restrict' and rename '__const'.\n";
      print $fp "\n";
      print $fp "*/\n";
      print $fp "#define  __attribute__(x)  /* NOTHING  */\n";
      print $fp "#define  __extension__     /* NOTHING */\n";
      print $fp "#define  __restrict        /* NOTHING */\n";
      print $fp "#define  __const           const\n";

      # if developing, have possibility to add a function to check its
      # parsing (but MUCH later after all else is well)
      if (0 && $G::addfunc) {
	print $fp "void\n";
	print $fp "foo(int i)\n";
	print $fp "{\n";
	print $fp "  printf(\"value of i is '%d'\\n\", i);\n";
	print $fp "}\n";
      }

      # now add the lines from the original C header
      open my $fpi, '<', $ifil
	or die "$ifil: $!";
      my @tflines = <$fpi>;
      print $fp $_ for @tflines;
      close $fp;

      # use gcc -E to preprocess the file
      print "DEBUG:  preprocessing '$cinfil' to '$ppfil' using gcc -E...\n"
	if $G::debug;

      convert_with_gcc_E($cinfil, $ppfil);

      # convert it to "final" form (eventually)
      print "DEBUG:  converting '$ppfil' to D module '$ofil'...\n"
	if $G::debug;

      convert1final($ofil, $ppfil, \%syshdr, $stem, $ofils_ref, \@tmpfils);

    }
    #==== method 2 ====
    elsif ($meth == 2) {

=pod

      # use dstep
      convert_with_dstep($tfil0, $tfil1);

      # dress up the file and convert it to "final" form (eventually)
      convert1final($ofil, $tfil1, \%syshdr, $stem);

=cut

    }
    #==== method 3 ====
    elsif ($meth == 3) {

=pod

      # use gcc; need a C input file
      my $cfil = "./di/$stem.h.c";
      copy $ifil, $cfil;
      push @tmpfils, $cfil;

      # the output file of interest will be named:
      my $tufil = "$cfil.001t.tu";
      push @tmpfils, $tufil;
      push @{$ofils_ref}, $tufil
	if $G::debug;

      convert_with_gcc_fdump_tu($cfil);

      # extract data into another file
      # the output file of interest will be named:
      my $pfil = "$tufil.dat";
      push @tmpfils, $pfil;
      push @{$ofils_ref}, $pfil
	if $G::debug;

      process_tu_file($tufil, $pfil);

      # dress up the file and convert it to "final" form (eventually)
      #convert3final($ofil, $pfil, \%syshdr, $stem);

=cut

    }

    # eliminate unneeded intermediate files;
    unlink @tmpfils
      if !$G::debug;

    # update hash for the new, final output file
    $curr_hash = DS::calc_md5hash($ofil);

    if (!-f $ofil) {
      print "D::convert(): Output file '$ofil' does not exist.\n"
	if $G::verbose;
    }
    elsif (!$prev_hash || $prev_hash ne $curr_hash) {
      if ($G::verbose) {
	print "D::convert(): Output file '$ofil' hashes:\n";
	print "  prev '$prev_hash'\n";
	print "  curr '$curr_hash'\n";
      }
      DS::store_md5hash($ofil, $curr_hash);
      push @{$ofils_ref}, $ofil;
    }
    else {
      print "D::convert(): Output file '$ofil' has not changed.\n"
	if $G::verbose;
    }

  } # iterate over input files

} # convert

sub collect_files {

  # deletes generated files if $G::clean
  my $href = shift @_; # @h
  my $dref = shift @_; # @d

  my @d = glob("${BP::DIDIR}/*${G::Dsuf}");
  if ($G::clean) {
    unlink @d;
    DS::remove_md5hash_store();
  }
  else {
    push @{$dref}, @d;
  }

  # main API headers
  my @h  = glob("${BP::IDIR}/*${G::Hsuf}");
  foreach my $f (@h) {
    next if exists $ignore{ basename $f };
    push @{$href}, $f;
  }

  # other headers
  #my @oh  = glob("${BP::IDIR}/*${G::Hsuf}");

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
  my $ofil       = shift @_; # $ofil
  my $ifil       = shift @_; # $ppfil
  my $sref       = shift @_; # \%syshdr
  my $stem       = shift @_; # stem of .h file name (e.g., stem of 'bu.h' is 'bu'

  my $ofils_aref = shift @_; # \@ofils
  my $tfils_aref = shift @_; # \@tmpfils

  # before we open the final output file we need more intermediate
  # processing: the $ppfil has to be parsed and some fancy extraction
  # and class processing done

  # process the input file ($ppfil)
  open my $fpi, '<', $ifil
    or die "$ifil: $!";

  my @ilines = <$fpi>;
  close $fpi;
  my $nl = @ilines;

  # save processed lines for later
  my @olines = ();

  # chunks go into an array
  my @objs = ();

  # use for limiting chunk (object) processing
  my $nchunks = 0;

 LINE:

  for (my $i = 0; $i < $nl; ++$i) {
    my $lnum = $i + 1;
    my $line = $ilines[$i];

    # ignore or collapse blank lines
    if ($line !~ /\S+/) {
      printf "DEBUG: skipping blank line at line %d (%s)...\n",
	__LINE__, __FILE__
	  if (0 && $G::debug);
      next LINE;
    }

    my @d = split(' ', $line);

    my $key = $d[0];

    if ($key =~ m{\A \s* \#}x) {
      # ignore cpp comment lines
      next LINE;
    }

    # always getting chunks
    my $obj = HObj::get_new_hobj();
    $obj->num($nchunks);

    $i = CExtract::extract_object({
				   lines_aref      => \@ilines,
				   curr_index      => $i,
				   obj             => $obj,
				 });

    push @olines, $obj->orig_line();
    push @objs, $obj;

    # update number of chunks being processed
    ++$nchunks;

    if ($G::maxchunks && $G::maxchunks == $nchunks) {
      my $s = $nchunks > 1 ? 's' : '';
      print "DEBUG:  last line after $nchunks chunk${s}.\n"
	if $G::debug;
      last LINE;
    }

  }

  # report number of chunks found
  print "DEBUG:  Processed $nchunks objects...\n"
    if $G::debug;

=pod

  # don't go this route for now

  # parse the tree
  my $ptree = ParsePPCHeader::parse_cfile
    ({
      ityp => 'aref',
      ival => \@olines,
     });

  if (defined $ptree) {
    my $fp;
    # dump it
    my $efil = "${ifil}.dump.txt";
    open $fp, '>', $efil
      or die "$efil: $!";
    push @{$tfils_aref}, $efil;
    push @{$ofils_aref}, $efil
      if $G::debug;
    ParsePPCHeader::dump_parse_tree($fp, $ptree);
    close $fp;

    # inspect it (print it)
    my $pfil = "${ifil}.myprint.txt";
    open $fp, '>', $pfil
      or die "$pfil: $!";
    push @{$tfils_aref}, $pfil;
    push @{$ofils_aref}, $pfil
      if $G::debug;
    ParsePPCHeader::print_parse_tree($fp, $ptree);
  }

=cut

  # pretty print the objects
  if ($G::pretty_print) {
    my ($f, $f2, $fp, $fp2);

    print "DEVEL: pretty printing C and D objects...\n";
    $f  = "${ifil}.c_pretty_print.txt";
    $f2 = "${ifil}.d_pretty_print.txt";

    open $fp, '>', $f
      or die "$f: $!";
    push @{$tfils_aref}, $f;
    push @{$ofils_aref}, $f
      if $G::debug;

    open $fp2, '>', $f2
      or die "$f2: $!";
    push @{$tfils_aref}, $f2;
    push @{$ofils_aref}, $f2
      if $G::debug;

    foreach my $o (@objs) {
      $o->do_all_conversions();
      $o->print_pretty($fp, 'c');
      $o->print_pretty($fp2, 'd');
      #$o->dump();
    }

  }

  # process objects and write them out
  #say "WARNING:  No final converion yet!";

  open my $fpo, '>', $ofil
    or die "$ofil: $!";

  say $fpo "module $stem;\n";

  my @sysmods = (sort keys %sysmod);
  if (@sysmods) {
    foreach my $h (@sysmods) {
      my $mod = $sysmod{$h};
      next if !$mod;
      say $fpo "import $mod;";
    }
    print $fpo "\n";
  }

  say $fpo "extern (C) {";

  foreach my $o (@objs) {
    $o->do_all_conversions();
    $o->print_final($fpo, $ifil);
  }

  # ender
  print $fpo "\n";
  print $fpo "} // extern (C) {\n";

} # convert1final

sub convert_with_gcc_E {
  my $ifil = shift @_; # tfil0
  my $ofil = shift @_; # tfil1

  my $incdirs  = "-I${BP::IDIR} -I${BP::IDIR2}";

  my $opts = '';
  #$opts .= ' -CC'; # keep C and C++ comments
  #$opts .= ' -H'; # list includes
  #$opts .= ' -v'; # report include paths
  $opts .= ' -P'; # omit line markers
  $opts .= ' -undef'; # Do not predefine any system-specific or GCC-specific macros. The standard predefined macros remain defined.
  $opts .= ' -x c'; # input file is C source code

  my $cmd = "gcc -E $opts $incdirs $ifil > $ofil";
  print "debug-cmd: '$cmd'\n"
    if $G::debug;

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
    if $G::debug;

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

  if (0 && $G::debug) {
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
    if (0 && $G::debug) {
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
	if (0 && $G::debug);

      # get a new file
      my $f = $s;

      # ignore <> files
      if (is_syshdr($f)) {
	print "WARNING: ignoring sys include file '$f' for now...\n"
	  if (0 && $G::debug);
	$sysref->{$s} = 1;
	# don't print
	next LINE;
      }

      # some files are ignored for now
      if (exists $tignore{$f}) {
	print "WARNING: ignoring include file '$f' for now...\n"
	  if (0 && $G::debug);
	next LINE;
      }

      # may be a "mapped" file
      if (exists $is_mapped{$f}) {
	my $ff = $is_mapped{$f};
	print "WARNING: using mapped file '$f' => '$ff' for now...\n"
	  if (0 && $G::debug);
	$f = $ff;
      }

      # one more shot for things in the subdir of $IDIR like './bu/bu.h'
      if (!-f $f) {
	# the following two formats are the same but confuse the 'seen' hash:
	#   'bu/bu_vlist.h'
	#   './bu/bu_vlist.h'
	my $s = $f;
	$s =~ s{\A \./}{}x;
	my $ff = "${BP::IDIR}/${s}";
	print "NOTE: using file '$f' => '$ff'...\n"
	  if (0 && $G::debug);
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
	if $G::debug;
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

# mandatory true return from a Perl module
1;
