#!/bin/sh
#
# This script updates BRL-CAD Copyright notices to include the current
# year in the copyright lines.
#
# Author - 
#   Christopher Sean Morrison 
#
# Source -
#   The U.S. Army Research Laboratory
#   Aberdeen Proving Ground, Maryland 21005-5068  USA
#
# Distribution Notice -
#   Re-distribution of this software is restricted, as described in
#   your "Statement of Terms and Conditions for the Release of The
#   BRL-CAD Package" agreement.
#
# Copyright Notice -
#   This software is Copyright (C) 2004 by the United States Army
#   in all countries except the USA.  All rights reserved.
#
###

# locate ourselves for generating a file list
findgen="$1"

if [ ! -d "$findgen" ] ; then 

  if [ -r "`dirname $0`/../configure.ac" ] ; then
    findgen="`dirname $0`/.."
  else
    for dir in . .. ; do
      if [ -r "$findgen/configure.ac" ] ; then
	findgen="$dir"
	break
      fi
    done
  fi
  
# sanity check
  if [ ! -d "$findgen/sh" ] ; then
    echo "ERROR: Unable to find our path relative to configure.ac"
    exit 1
  fi

fi  

# generate a list of files to check, excluding directories that are
# not BRL-CAD sources, CVS, or start with a dot.
files="`find $findgen -type f | \
        grep -v 'libtcl' | \
        grep -v 'libtk' | \
        grep -v 'incrTcl' | \
        grep -v 'iwidgets' | \
        grep -v 'jove' | \
        grep -v 'libutahrle' | \
        grep -v 'libz' | \
        grep -v 'libpng' | \
        grep -v 'CVS' | \
        grep -v 'misc/' | \
        grep -v \"$findgen/\.\" | \
        grep -v '\.g$' | \
        grep -v '\.pix$' |\
        grep -v '\.jpg$' |\
        grep -v '\.pdf$' |\
        grep -v '\.dll$' |\
        grep -v '\.gif$' |\
        grep -v '\.png$' |\
        grep -v '\.bak$' |\
        grep -v '\.old$' \
      `"

for file in $files ; do
  echo -n $file
  
  # sanity checks
  if [ ! -f "$file" ] ; then
    echo "."
    echo "WARNING: $file was lost, skipping"
    continue
  elif [ ! -r "$file" ] ; then
    echo "."
    echo "WARNING: $file is not readable"
    continue
  elif [ ! -w "$file" ] ; then
    echo "."
    echo "WARNING: $file is not writeable"
    continue
  fi
  echo -n "."
  if [ -f "$file.copyright.new" ] ; then
    echo "."
    echo "WARNING: $file.copyright.new is in the way (moving it to .bak)"
    mv $file.copyright.new $file.copyright.new.bak
  elif [ -f "$file.copyright.old" ] ; then
    echo "."
    echo "WARNING: $file.copyright.old is in the way (moving it to .bak)"
    mv $file.copyright.old $file.copyright.old.bak
  fi
  echo -n "."

  year=`date +%Y`
  sed -E "s/Copyright ?\([cC]\) ?([0-9][0-9][0-9][0-9]) ?-? ?[0-9]?[0-9]?[0-9]?[0-9]?([ .;]+)(by +the +United +States +Army)/Copyright (C) \1-$year\2\3/" < $file > $file.copyright.new

  filediff="`diff $file $file.copyright.new`"
  if [ "x$filediff" = "x" ] ; then
    echo "."
    rm $file.copyright.new
  elif [ ! "x`echo $filediff | grep \"No newline at end of file\"`" = "x" ] ; then
    echo ". `basename $file` has no newline -- SKIPPING"
    rm $file.copyright.new
  elif [ ! "x`echo $filediff | grep \"Binary files\"`" = "x" ] ; then
    echo ". `basename $file` is binary -- SKIPPING"
    rm $file.copyright.new
  else
    echo ". `basename $file` modified"
    mv $file $file.copyright.old
    mv $file.copyright.new $file
  fi

  sed -E "s/Copyright \(C\) $year-$year/Copyright (C) $year/" < $file > $file.copyright.new

  filediff="`diff $file $file.copyright.new`"
  if [ "x$filediff" = "x" ] ; then
    rm $file.copyright.new
  elif [ ! "x`echo $filediff | grep \"No newline at end of file\"`" = "x" ] ; then
    rm $file.copyright.new
  elif [ ! "x`echo $filediff | grep \"Binary files\"`" = "x" ] ; then
    rm $file.copyright.new
  else
    echo "WARNING: $file had Copyright span for the same year"
    mv $file $file.copyright.2.old
    mv $file.copyright.new $file
  fi

  # done iteration over files
done

# Local Variables: ***
# mode:sh ***
# tab-width: 8 ***
# sh-indentation: 2 ***
# sh-basic-offset: 2 ***
# indent-tabs-mode: t ***
# End: ***
# ex: shiftwidth=2 tabstop=8
