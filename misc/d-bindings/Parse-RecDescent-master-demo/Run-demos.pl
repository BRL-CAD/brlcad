#!/usr/bin/env perl

use strict;
use warnings;

# global vars
my %progs; # defined in BEGIN block below

if (!@ARGV) {
  print <<"HERE";
Usage: $0 mode [...options...]

Executes all "demo*pl" scripts and redirects output to stdout
separated by comments identifying the script and the input file
(if any).

Modes:

  -r            Run all programs.
  -r=X[,Y,...]  Run a subset of all programs (the list must have no spaces).
  -list         List all programs and their arguments (if any).

Options:

  -d            Debug.

HERE

  exit;
}

# modes
my $run  = 0;
my $list = 0;

sub zero_modes {
  $run  = 0;
  $list = 0;
}

# options
my $debug = 0;

# other
my %ifils = ();

foreach my $arg (@ARGV) {
  my $saved_arg = $arg;
  my $val = undef;
  my $idx = index $arg, '=';
  if ($idx >= 0) {
    $val = substr $arg, $idx+1;
    $arg = substr $arg, 0, $idx;
  }

  if (0 || $debug) {
    print "DEBUG:\n";
    print "  original  arg: '$saved_arg'\n";
    print "  processed arg: '$arg'\n";
    if (defined $val) {
      print "  processed val: '$val'\n";
    }
    else {
      print "  val was not defined\n";
    }
  }
  # modes
  if ($arg =~ m{\A \-r}x) {
    zero_modes();
    if (defined $val) {
      my @fils = split(',', $val);
      foreach my $f (@fils) {
	if (exists $progs{$f}) {
	  $ifils{$f} = 1;
	}
	else {
	  print "WARNING:  File '$f' is not in the progs list.\n";
	}
      }
      $run = 2;
    }
    else {
      $run = 1;
    }
  }
  elsif ($arg =~ m{\A \-l}x) {
    zero_modes();
    $list = 1;
  }
  # opions
  elsif ($arg =~ m{\A \-d}x) {
    $debug = 1;
  }
  else {
    die "FATAL:  Unknown arg '$saved_arg'.\n";
  }
}

die "FATAL:  No valid files entered for the '-r=X' option.\n"
  if ($run == 2  && !(keys %ifils));

my @progs = (sort keys %progs);

# option '-list' =============================
if ($list) {
  print "List of programs:\n";

  my $maxlen = 0;
  my @prog_list = ();
  foreach my $prog (@progs) {
    my $args = $progs{$prog};
    my $nam  = $prog;
    $nam .= " $args"
      if $args;
    my $len = length $nam;
    $maxlen = $len
      if $len > $maxlen;
    push @prog_list, $nam;
  }

  my $np = @prog_list;
  for (my $i = 0; $i < $np; $i += 2) {
    my $first  = $progs[$i];
    my $second = $progs[$i+1];
    if (!defined $second || !-f $second) {
      $second = 0;
    }

    printf "%-*.*s", $maxlen, $maxlen, $first;
    if ($second) {
      printf " | %-*.*s", $maxlen, $maxlen, $second;
    }
    print "\n";
  }

}
# option '-r[=X]' =============================
elsif ($run) {
  foreach my $prog (@progs) {
    next if ($run > 1 && !exists $ifils{$prog});

    my $args = $progs{$prog};
    print  STDOUT "=== begin program '$prog' ===\n";
    printf STDOUT "    === args '%s' ===\n", ($args ? $args : '(none)');

    my $msg = qx(perl $prog $args);
    if ($msg) {
      chomp $msg;
      print STDOUT "=== msg: '$msg'\n";
    }
    print STDOUT "=== end program '$prog' ===\n";

  }
}

#### subroutines ####

BEGIN {

  %progs
    = (
       # key: program name
       # val: arguments inputs (if any)
       'demo.pl'                     => '',
       'demo_Cgrammar.pl'            => 'demo.c',
       'demo_Cgrammar_v2.pl'         => 'demo.c',
       'demo_LaTeXish.pl'            => '',
       'demo_LaTeXish_autoact.pl'    => '',
       'demo_NL2SQL.pl'              => '',
       'demo_OOautoparsetree.pl'     => '',
       'demo_OOparsetree.pl'         => 'demo2.txt',
       'demo_PerlCSV.pl'             => '',
       'demo_another_Cgrammar.pl'    => '',
       'demo_arithmetic.pl'          => 'demo3.txt',
       'demo_autorule.pl'            => '',
       'demo_autoscoresep.pl'        => '',
       'demo_autostub.pl'            => '',
       'demo_bad.pl'                 => '',
       'demo_buildcalc.pl'           => '',
       'demo_calc.pl'                => '',
       'demo_codeblock.pl'           => '',
       'demo_cpp.pl'                 => 'demo.c',
       'demo_decomment.pl'           => '',
       'demo_decomment_nonlocal.pl'  => '',
       'demo_delete.pl'              => '',
       'demo_derived.pl'             => '',
       'demo_dot.pl'                 => '',
       'demo_embedding.pl'           => '',
       'demo_errors.pl'              => '',
       'demo_eval.pl'                => '',
       'demo_implicit.pl'            => 'demo4.txt',
       'demo_itemhash.pl'            => '',
       'demo_language.pl'            => '',
       'demo_leftassoc.pl'           => '',
       # not yet working:
       #'demo_leftop.pl'              => '',
       'demo_lexer.pl'               => '',
       'demo_lisplike.pl'            => '',
       'demo_logic.pl'               => 'demo2.txt',
       'demo_matchrule.pl'           => '',
       'demo_matchrule2.pl'          => '',
       'demo_mccoy.pl'               => 'demo5.txt',
       'demo_methods.pl'             => 'demo6.txt',
       'demo_operator.pl'            => 'demo2.txt',
       'demo_opreps.pl'              => '',
       'demo_parsetree.pl'           => '',
       'demo_perlparsing.pl'         => '',
       'demo_piecewise.pl'           => '',
       'demo_precalc.pl'             => '',
       'demo_quicklist.pl'           => '',
       'demo_randomsentence.pl'      => '',
       'demo_recipe.pl'              => '',
       'demo_restructure_easy.pl'    => '',
       'demo_restructure_painful.pl' => '',
       'demo_scoredsep.pl'           => '',
       'demo_selfmod.pl'             => 'demo7.txt',
       'demo_separators.pl'          => '',
       'demo_simple.pl'              => 'demo8.txt',
       'demo_simpleXML.pl'           => '',
       'demo_simplequery.pl'         => 'demo9.txt',
       'demo_skipcomment.pl'         => '',
       'demo_street.pl'              => 'demo10.txt',
       'demo_template.pl'            => '',
       'demo_textgen.pl'             => '',
       'demo_tokens.pl'              => '',

       # not yet working:
       #'demo_undumper.pl'            => '',

       # not yet working:
       #'demo_whoson.pl'              => '',
      ); # %progs

} # BEGIN
