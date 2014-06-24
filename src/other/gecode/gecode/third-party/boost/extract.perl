#!/usr/bin/perl
#  Main authors:
#     Christian Schulte <schulte@gecode.org>
#
#  Copyright:
#     Christian Schulte, 2005
#
#  Last modified:
#     $Date$ by $Author$
#     $Revision$
#
#  This file is part of Gecode, the generic constraint
#  development environment:
#     http://www.gecode.org
#
#  Permission is hereby granted, free of charge, to any person obtaining
#  a copy of this software and associated documentation files (the
#  "Software"), to deal in the Software without restriction, including
#  without limitation the rights to use, copy, modify, merge, publish,
#  distribute, sublicense, and/or sell copies of the Software, and to
#  permit persons to whom the Software is furnished to do so, subject to
#  the following conditions:
#
#  The above copyright notice and this permission notice shall be
#  included in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
#  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
#  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
#  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
#  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
#  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
#

$library = $ARGV[0];
$dst = $ARGV[1];

my @todo = ();
my %done = ();
push @todo, "$library";
$done{"$library"} = 1;
while ($f = pop @todo) {

  open FILE, "$f" or die "File missing: $f\n";
  while ($l = <FILE>) {
    if ($l =~ /^\#( )*include <(boost\/.*)>/) {
      $g = $2;
      $g =~ s|^\./||og;
      if (!$done{$g}) {
	push @todo, $g;
	$done{$g} = 1;
      }
    } elsif ($l =~ /^\#( )*include \"(boost\/.*)\"/) {
      $g = $2;
      $g =~ s|^\./||og;
      if (!$done{$g}) {
	push @todo, $g;
	$done{$g} = 1;
      }
    } elsif ($l =~ /^\#( )*include <(.*)>/) {
      $other{$2} = 1;
    } elsif ($l =~ /^\#( )*include (.*)/) {
      $k = $2; chop($k);
      $unresolved{$k} = 1;
    } elsif ($l =~ /^\#( )*define (BOOST_[A-Z_]*) (.*)/) {
      $k = $2; $v = $3;
      chop($v);
      if ($v =~ /\"(.*)\"/) {
	$v = $1;
      }
      if ($v =~ /<(.*)>/) {
	$v = $1;
      }
      if ($def{"$k"}) {
	$def{"$k"} = $def{"$k"} . ",$v";
      } else {
	$def{"$k"} = $v;
      }
    }
  }
  close FILE;

  foreach $u (keys(%unresolved)) {
    if ($def{"$u"}) {
      foreach $d (split(',',$def{$u})) {
	if (!$done{$d}) {
	  push @todo, $d;
	  $done{$d} = 1;
	}
      }
    }
  }
}

foreach $g (sort(keys(%done))) {
  if ($g =~ /(.*)\/.*/) {
    $dir{$1} = 1;
  }
}
foreach $d (sort(keys(%dir))) {
  print "mkdir -p \"${dst}$d\"\n";
;
}
foreach $g (sort(keys(%done))) {
  if ($g =~ /(.*)\/.*/) {
    $d = $1;
    print "cp \"$g\" \"${dst}$d\"\n";
  }
}
