#!/usr/bin/perl
#                       R E N A M E . P L
# BRL-CAD
#
# Copyright (c) 2007 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public License
# as published by the Free Software Foundation.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this file; see the file named COPYING for more
# information.
#
###################################
# Usage rename.pl FROM TO "*.ext"
###################################

$command = "find ./ -name \"$ARGV[2]\"";
print "$command\n";
@list = qx($command);

foreach $file (@list) {
  chop $file;
  print "converting: $file\n";

  $command = "sed s/$ARGV[0]/$ARGV[1]/g $file > /tmp/foo.txt";
  system($command);

  $command = "rm -f $file";
  system($command);

  $command = "mv /tmp/foo.txt $file";
  system($command);
}

# Local Variables:
# mode: Perl
# tab-width: 8
# c-basic-offset: 4
# perl-indent-level: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
