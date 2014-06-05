#!/usr/bin/env perl

use v5.14; # say, switch

# converts a grammar to a Perl module

use strict;
use warnings;

use Data::Dumper; $Data::Dumper::Indent = 1;

use File::Basename;
use lib('.');

my $p = basename($0);
my $usage  = "Usage:  $p go [-d][-f][-h]";

my $ifil = 'c-grammar.txt'; # in P::RD format
my $pm   = 'RG';            # in R::G  format

my $ofil = "${pm}.pm";

if (!@ARGV) {
  say <<"HERE";
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

my %prod  = (); # hash of production names and their children
my @prods = (); # retain order as read

my $maxCAPSlen = extract_grammar(\%prod, \@prods);

say  "# DEBUG: max caps len = $maxCAPSlen"
  if $debug;

if ($debug) {
  # chars allowed for CAPS prod name plus a colon plus one space
  my $spaces = $maxCAPSlen + 2;

  say "# C grammar:";
  foreach my $p (@prods) {
    my $caps = ($p =~ m{\A [A-Z_]+ \z}x) ? 1 : 0;
    my @c = @{$prod{$p}};
    my $nc = @c;
    if (!$caps) {
      say "$p:";
    }
    else {
      my $len = length $p;
      my $sp  = $spaces;
      $sp -= ($len + 1); # space for prod name plus colon
      printf "$p:%-*.*s", $sp, $sp, ' ';
    }
    for (my $i = 0; $i < $nc; ++$i) {
      my $c = $c[$i];
      if ($caps && $i) {
	my $sp = $spaces;
	printf "%-*.*s", $sp, $sp, ' ';
      }
      if (!$caps) {
	print "\t";
      }
      print "| " if $i;
      print "$c";
      print "\n";
    }
    print "\n" if !$caps;
  }
  #print Dumper(\%prod);
}

say "Normal end.";

#### subroutines ####
sub extract_grammar {
  my $prod_href  = shift @_; # \%prod
  my $prods_aref = shift @_; # \@prods

  # certain lines can't be processed
  my @pipes_ok
    = (
       '/(typedef|extern|static|auto|register)(?![a-zA-Z0-9_])/',
       '/(void|char|short|int|long|float|double|signed|unsigned)(?![a-zA-Z0-9_])/',
       '/(struct|union)(?![a-zA-Z0-9_])/',
       '/(const|volatile)(?![a-zA-Z0-9_])/',
       '/(auto|break|case|char|const|continue|default|do|double|enum|extern|float|for|goto|if|int|long|register|return|short|signed|sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|while)(?![a-zA-Z0-9_])/',
       'CONSTANT:       /[+-]?(?=\d|\.\d)\d*(\.\d*)?([Ee]([+-]?\d+))?/',
       '( type_specifier | type_qualifier )(s)',
       '( storage_class_specifier | type_specifier | type_qualifier )(s)',
      );
  my %pipes_ok;
  @pipes_ok{@pipes_ok} = ();

  # process each production as we encounter it
  my $curr_prod     = ''; # name
  my @curr_children = (); # all children

  # track some prod name lengths
  my $maxCAPSlen = 0;

  my $linenum = 0;
  while (defined(my $line = <$fp>)) {
    ++$linenum;

    $line = strip_comment($line);
    if ($line !~ /\S+/) {
      say "DEBUG:  skipping empty line '$line'."
	if (0 && $debug);
      next;
    }
    next if $line =~ /<autotree>/;
    chomp $line;

    if (0 && $debug) {
      say "DEBUG: non-empty line '$line'";
    }

    # trim
    $line =~ s{\A \s+}{}x;
    $line =~ s{\s+ \z}{}x;

    # add some spaces on the line for more reliable tokenizing
    # careful, ignore chars between '/' and '/', etc.
    # add space around some pipes
    my $t1 = $line;
    if (!exists $pipes_ok{$line}) {
      $line =~ s{([a-zA-Z]) (\|)}{$1 $2}xg;
      $line =~ s{(\|) (a-zA-Z])}{ $1 $2}xg;
      # some lines  have been trimmed, remove leading pipes
      $line =~ s{\A (\|)}{}x;

      my $t2 = $line;
      if (0 && $debug && $t1 ne $t2) {
	say "DEBUG: original line with pipes:";
	say "  '$t1'";
	say "DEBUG: line with spaced pipes:";
	say "  '$t2'";
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
	say "DEBUG: original line with colon:";
	say "  '$s'";
      }
      # affect only colons at end (or beginning?) of identifiers
      $line =~ s{\A ([a-zA-Z_]+)\:}{$1 \:}x;
      if (0 && $debug) {
	my $s = $line;
	say "       line with colon separated:";
	say "  '$s'";
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
	  if exists $prod{$curr_prod};
	if (@curr_children) {
	  # special case for lines beginning with '(' (a hack, could be improved for generality)
	  my $c = substr $curr_children[0], 0, 1;
	  if ($c eq '(') {
	    $prod_href->{$curr_prod} = [join(' ', @curr_children)];
	  }
	  else {
	    $prod_href->{$curr_prod} = [@curr_children];
	  }
	  @curr_children = ();
	}
	else {
	  $prod_href->{$curr_prod} = [];
	  say "WARNING:  production rule '$curr_prod' has no children.";
	}
	# save the prod order
	push @{$prods_aref}, $curr_prod;
      }
      $curr_prod = $key;

      # prod names in CAPS are special
      if ($key =~ m{[A-Z]+}) {
	my $len = length $key;
	$maxCAPSlen = $len
	  if $len > $maxCAPSlen;
      }

      # rejoin the line and reprocess it
      my $pline = join(' ', @d);
      # split on space-separated pipes
      my @t = split(' \| ', $pline);
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
	# split on space-separated pipes
	if (!exists $pipes_ok{$pline}) {
	  my @t = split(' \| ', $pline);
	  foreach my $t (@t) {
	    # trim
	    $t =~ s{\A \s*}{}x;
	    $t =~ s{\s* \z}{}x;
	    push @curr_children, $t
	      if $t;
	  }
	}
	else {
	  push @curr_children, $pline
	      if $pline;
	}
      }
      else {
	say STDERR "ERROR: No \$curr_prod for line $linenum:";
	say STDERR "  line: '$line'";
	die "FATAL:  No \$curr_prod.";
      }
    }
  }

  # the last prod is remaining
  # we may need to save the current one
  die "FATAL: dup production rule '$curr_prod'"
    if exists $prod{$curr_prod};
  if (@curr_children) {
    $prod_href->{$curr_prod} = [@curr_children];
    @curr_children = ();
  }
  else {
    $prod_href->{$curr_prod} = [];
    say "WARNING:  production rule '$curr_prod' has no children.";
  }
  push @{$prods_aref}, $curr_prod;

  return $maxCAPSlen;

} # extract_grammar

sub longhelp {
  say <<"HERE";
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
