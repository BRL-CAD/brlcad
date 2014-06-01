#!/usr/bin/env perl

# converts a grammar to a Perl module

use strict;
use warnings;

use Data::Dumper; $Data::Dumper::Indent = 1;



use File::Basename;
use lib('.');
my $p = basename($0);
my $usage  = "Usage:  $p go [-d][-f][-h]";

my $ifil = 'c-grammar.txt';
my $pm   = 'GS';
my $ofil = "${pm}.pm";

if (!@ARGV) {
  print <<"HERE";
$usage

Use '-h' for extended help.

HERE

  exit;
}

my $debug = 0;
my $force = 1;

foreach my $arg (@ARGV) {
  if ($arg =~ m{\A -d}x) {
    $debug = 1;
  }
  elsif ($arg =~ m{\A -f}x) {
    $force = 1;
  }
  elsif ($arg =~ m{\A -h}x) {
    longhelp();
  }
  elsif ($arg =~ m{\A [\-]*[^dfh]*}x) {
    ; # okay
  }
  else {
    die "FATAL:  Unknown arg '$arg'.\n";
  }
}

die "FATAL:  Unable to find grammar file '$ifil'.\n"
  if ! -f $ifil;

open my $fp, '<', $ifil
  or die "$ifil: $!";

my %prods = (); # hash of production names and their children

# process each production as we encounter it
my $curr_prod     = ''; # name
my @curr_children = (); # all children

my $linenum = 0;
while (defined(my $line = <$fp>)) {
  ++$linenum;

  $line = strip_comment($line);
  if ($line !~ /\S+/) {
    print "DEBUG:  skipping empty line '$line'.\n"
      if (0 && $debug);
    next;
  }
  next if $line =~ /<autotree>/;
  chomp $line;

  if (0 && $debug) {
    print "DEBUG: non-empty line '$line'\n";
  }

  # trim
  $line =~ s{\A \s+}{}x;
  $line =~ s{\s+ \z}{}x;

  # add some spaces on the line for more reliable tokenizing
  # careful, ignore chars between '/' and '/', etc.
  my $t1 = $line;
  if ($line ne '/(typedef|extern|static|auto|register)(?![a-zA-Z0-9_])/'
      && $line ne '/(void|char|short|int|long|float|double|signed|unsigned)(?![a-zA-Z0-9_])/'
      && $line ne '/(struct|union)(?![a-zA-Z0-9_])/'
      && $line ne '/(const|volatile)(?![a-zA-Z0-9_])/'
      && $line ne '/(auto|break|case|char|const|continue|default|do|double|enum|extern|float|for|goto|if|int|long|register|return|short|signed|sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while)(?![a-zA-Z0-9_])/'
      && $line ne 'CONSTANT:       /[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/'
     ) {
    $line =~ s{([a-zA-Z]) (\|)}{$1 $2}xg;
    $line =~ s{(\|) (a-zA-Z])}{$1 $2}xg;
    my $t2 = $line;
    if (0 && $debug && $t1 ne $t2) {
      print "DEBUG: original line with pipes:\n";
      print "  '$t1'\n";
      print "DEBUG: line with spaced pipes:\n";
      print "  '$t2'\n";
    }
  }

  # check for a production line
  my $have_colon = ($line =~ m{\A [a-zA-Z_]+\s*\:}x) ? 1 : 0;

  # check for malformed lines (or my misunderstanding)
  if ($line =~ m{\A [a-zA-Z_]+\s*\:\S}x) {
    die "bad colon line '$line'";
  }

  if ($have_colon) {
    if (0 && $debug) {
      my $s = $line;
      print "DEBUG: original line with colon:\n";
      print "  '$s'\n";
    }
    # affect only colons at end (or beginning?) of identifiers
    $line =~ s{\A ([a-zA-Z_]+)\:}{$1 \:}x;
    if (0 && $debug) {
      my $s = $line;
      print "       line with colon separated:\n";
      print "  '$s'\n";
    }
  }

  my @d = split(' ', $line);

  # is this a production definition line"
  my $key = shift @d;
  my $newprod = ($key =~ s{\: \z}{}x) ? 1 : 0;
  my $key2 = $newprod ? '' : shift @d;
  if (defined $key2) {
    if ($key2 eq ':') {
      $newprod = 1;
      $key2 = '';
    }
    else {
      unshift @d, $key2;
    }
  }

  if ($newprod) {
    # beginning of a production rule with non-empty @d as subrules
    if ($curr_prod) {
      # we may need to save the current one
      die "FATAL: dup production rule '$curr_prod'"
	if exists $prods{$curr_prod};
      if (@curr_children) {
	$prods{$curr_prod} = [@curr_children];
	@curr_children = ();
      }
      else {
	$prods{$curr_prod} = [];
	print "WARNING:  production rule '$curr_prod' has no children.\n";
      }
    }
    $curr_prod = $key;
    # rejoin the line and reprocess it
    my $pline = join(' ', @d);
    my @t = split('\|', $pline);
    foreach my $t (@t) {
      # trim
      $t =~ s{\A \s*}{}x;
      $t =~ s{\s* \z}{}x;
      push @curr_children, $t
	if $t;
    }
  }
  else {
    if ($curr_prod) {
      # rejoin the line and reprocess it
      my $pline = join(' ', ($key, @d));
      my @t = split('\|', $pline);
      foreach my $t (@t) {
	# trim
	$t =~ s{\A \s*}{}x;
	$t =~ s{\s* \z}{}x;
	push @curr_children, $t
	  if $t;
      }
    }
    else {
      print STDERR "ERROR: No \$curr_prod for line $linenum:\n";
      print STDERR "  line: '$line'\n";
      die "FATAL:  No \$curr_prod.";
    }
  }
}

# any remaining?
# we may need to save the current one
die "FATAL: dup production rule '$curr_prod'"
  if exists $prods{$curr_prod};
if (@curr_children) {
  $prods{$curr_prod} = [@curr_children];
  @curr_children = ();
}
else {
  $prods{$curr_prod} = [];
  print "WARNING:  production rule '$curr_prod' has no children.\n";
}

if ($debug) {
  print "DEBUG: dumping results:\n";
  print Dumper(\%prods);
}

#### subroutines ####
sub longhelp {
  print <<"HERE";
$usage

  -d  Debug
  -f  Force overwriting existing files
  -h  Extended help

Extracts grammar from file '$ifil' and creates a Perl
data module named '${ofil}'.

HERE

  exit;
} # help

sub strip_comment {
  my $line = shift @_;

  if (1) {
    $line =~ s{\A ([^\#]*) \# [\s\S]* \z}{$1}x;
  }
  else {
    my $idx = index $line, '#';
    if ($idx >= 0) {
      $line = substr $line, 0, $idx;
    }
  }
  return $line;
} # strip_comment
