#!/usr/bin/env perl

# Written by Tom Browder (tom.browder@gmail.com), 2009-06-18, and
# contributed to the BRL-CAD community to use for any purpose
# without restriction.  User feedback welcome.

use warnings;
use strict;

# recognized BRL-CAD units
my %units
    = (
       # there are other possibilities, add them to this list as you need them
       'mm' => 1,
       'in' => 1,
       'cm' => 1,
       'm'  => 1,
       'ft' => 1,
       );

# default values
my $def_render = 1;
my $def_thresh = '0.1';
my $def_std    = '1.0';
my $def_min    = '0.0';
my $def_max    = '10000.0';
my $def_air    = '0';
my $def_los    = '100';
my $def_mat    = '0';
my $def_id     = '99999';

if (!@ARGV) {
  print<<"HERE";
Usage: $0 <input file> [-h][-f]

    Uses the text input file with desired parameters to write a script to
    stdout that, when sourced into a BRL-CAD database, creates one or more
    metaball regions.

Options:

    -h      Show details on writing an input script.
    -f      Write to stdout an example script usable in mged.

HERE
    exit;
}


foreach my $a (@ARGV) {
  help() if $a eq '-h';
  example() if $a eq '-f';
}


my $ifil = 0;
foreach my $a (@ARGV) {
  next if $a eq '-h';
  die "Unable to open file '$a': $!\n" if ! -r $a;
  $ifil = $a;
}


die "No input file entered.\n" if !$ifil;

my %metaballs = ();
parse_metaballs($ifil, \%metaballs);
write_meta_balls(\%metaballs);


#### subroutines ####
sub parse_metaballs {
  my $fname = shift;
  my $href  = shift;
  local(*FP);
  open(FP, "<$ifil");
  my @lines = <FP>;

  my $units = 0; # mandatory, one instance only
  my $group = 0; # optional, one instance only
  my $name  = 0; # name of current metaball definition
  my $nl = @lines;
  for (my $i = 0; $i < $nl; ++$i) {
    my $line = $lines[$i];
    strip_comment(\$line);
    my @d = split(' ', $line);
    next if !defined($d[0]);

    my $kw = shift @d;
    if (!($kw =~ s/\:$//)) {
      print "On line: '$line':\n";
      die "Unrecognized keyword '$kw'\n";
    }

    die "Need arg(s) on line: '$line'\n" if (!@d);

    # should have most used at the top
    if ($kw eq 'point') {
      die "Need 4 args on line: '$line'\n" if (4 > @d);
      my ($x, $y, $z, $r) = (@d);
      my %p = (
	       'x' => $x,
	       'y' => $y,
	       'z' => $z,
	       'r' => $r,
               );
      if (exists($href->{$name}{points})) {
	push @{$href->{$name}{points}}, {%p};
      }
      else {
	$href->{$name}{points} = [];
	push @{$href->{$name}{points}}, {%p};
      }
    }
    elsif ($kw eq 'metaball') {
      $name = shift @d;
      die "Duplicate metaball basename '$name'.\n" if exists($href->{$name});
      die "No units have been defined.\n" if !$units;
      $href->{$name}{units} = $units;
      $href->{$name}{group} = $group;
    }
    elsif ($kw eq 'id') {
      my $id = shift @d;
      $href->{$name}{id} = $id;
    }
    elsif ($kw eq 'air') {
      my $air = shift @d;
      $href->{$name}{air} = $air;
    }
    elsif ($kw eq 'units') {
      die "'units' definition must precede metaball definitions.\n" if $name;
      die "Only one 'units' definition allowed.\n" if $units;
      $units = shift @d;
      if (!exists($units{$units})) {
	print "Units '$units' not a recognized BRL-CAD entry.\n";
	print "but this script does not have an exhaustive list:\n";
	print "If you know the '$units' is correct, add it to the list\n";
	print "at the top of this file and rerun '$0'.\n";
	exit;
      }
    }
    elsif ($kw eq 'group') {
      die "'group' definition must precede metaball definitions.\n" if $name;
      die "Only one 'group' definition allowed.\n" if $group;
      $group = shift @d;
    }
    elsif ($kw eq 'threshold') {
      $href->{$name}{threshold} = $kw;
    }
    elsif ($kw eq 'std') {
      $href->{$name}{std} = $kw;
    }
    elsif ($kw eq 'min') {
      $href->{$name}{min} = $kw;
    }
    elsif ($kw eq 'max') {
      $href->{$name}{max} = $kw;
    }
    elsif ($kw eq 'mat') {
      $href->{$name}{mat} = $kw;
    }
    elsif ($kw eq 'los') {
      $href->{$name}{los} = $kw;
    }
    elsif ($kw eq 'rendermethod') {
      $href->{$name}{rendermethod} = $kw;
    }
  }

} # parse_metaballs

sub write_meta_balls {
  my $href = shift;
  my @names = (keys %{$href});
  # check the first metaball for units and group
  my $mb = $names[0];
  my $units = $href->{$mb}{units};
  my $group = $href->{$mb}{group};
  print "units $units\n";
  print "if ![catch {get $group}] {kill $group}\n" if $group;
  foreach my $m (@names) {
    my %m   = %{$href->{$m}};
    my @pts = @{$m{points}};
    my $np  = @pts;

    my $trm  = exists($m{rendermethod}) ? $m{rendermethod} : $def_render;
    my $tth  = exists($m{threshold})    ? $m{threshold}    : $def_thresh;
    my $tstd = exists($m{std}) ? $m{std} : $def_std;
    my $tmin = exists($m{min}) ? $m{min} : $def_min;
    my $tmax = exists($m{max}) ? $m{max} : $def_max;
    my $tair = exists($m{air}) ? $m{air} : $def_air;
    my $tlos = exists($m{los}) ? $m{los} : $def_los;
    my $tmat = exists($m{mat}) ? $m{mat} : $def_mat;
    my $tid  = exists($m{id})  ? $m{id}  : $def_id;

    # produce the solid
    my $solid = "${m}.s";
    print "if ![catch {get $solid}] {kill $solid}\n";
    print "in $solid metaball $trm $tth $np";
    foreach my $pref (@pts) {
      my $x = $pref->{'x'};
      my $y = $pref->{'y'};
      my $z = $pref->{'z'};
      my $r = $pref->{'r'};
      print " $x $y $z $r";
    }
    print "\n";

    # set solid attributes
    # from mged: attr set <object> <key> <value>
    print "attr set $solid Mb_StdChgWt $tstd\n";
    print "attr set $solid Mb_MinChgWt $tmin\n";
    print "attr set $solid Mb_MaxChgWt $tmax\n";

    # produce the region
    my $region = "${m}.r";
    print "if ![catch {get $region}] {kill $region}\n";
    print "r $region u $solid\n";

    # set the base region attributes
    # From mged: help edcomb
    # Usage: edcomb combname Regionflag regionid air los [material]
    #	(edit combination record info)
    # edcomb t.r R 3333 0 0 0
    print "edcomb $region R $tid $tair $tlos $tmat\n";

    # group the region if required
    print "g $group $region\n" if $group;
  }

} # write_meta_balls

sub strip_comment {
  # pass a reference to a line to this function
  my $lineref = shift;
  my $cmtchar = shift;
  $cmtchar = defined($cmtchar) ? $cmtchar : '#'; # have a default
  my $idx = index $$lineref, '#';
  if ($idx >= 0) {
    $$lineref = substr $$lineref, 0, $idx;
  }
  else {
    chomp $$lineref;
  }
} # strip_comment

sub help {
  print<<"HERE";
# All text following a '#' on a line and blank lines are ignored.

# these keywords must precede metaball definitions:
units: X # mandatory BRL-CAD dimension unit, only one instance allowed
group: X # optional, only one instance allowed, all reqions will be
# grouped here; WARNING: any existing group X will be killed

# metaball definition(s):
metaball: <basename>    # yields objects => <basename>.s, <basename>.r
 point: x y z r        # coords and "field strength" (radius)
 id:                   # region ID (a default of $def_id is used
 # if this is missing)
# optional entries
 air:          X       # aircode (default $def_air)
 rendermethod: X       # 0, 1, or 2 (default $def_render)
 threshold:    X       # threshold ? (default $def_thresh)
 std:          X       # lbs TNT expected, default $def_std
 min:          X       # min lbs TNT expected, default $def_min
 max:          X       # max lbs TNT expected, default $def_max
 mat:          X       # material code (default $def_mat)
 <additional points>

# The metaball definition is ended at EOF or at the start of the next
# metaball definition.

 <zero or more additional metaballs>
HERE
 exit;
} # help

sub example {
  print<<"HERE";
units: in
group: mballs

# metaball definition(s):
metaball: mb1
 id: 1
 point: 0 0 0 .5
 point: 4 0 0 .5
 point: 4 0 1 .5
 point: 8 0 1 .5

metaball: mb2
 id: 2
 point: 20 0 5 .5
HERE
 exit;
} # example
