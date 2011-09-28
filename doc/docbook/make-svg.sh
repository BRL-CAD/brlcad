#!/bin/bash

# the math2svg Puthon program
#    see http://http://grigoriev.ru/svgmath/ and
#   http://sourceforge.net/projects/svgmath/) for download and
#   installation instructions
MATH2SVG=math2svg.py

# location of the user's math2svg program
SVGM="/usr/local/bin/$MATH2SVG"

# user-desired working directory (must exist)
ODIR=./svgwrk

EXIT=0
if [[ -z $1 ]] ; then
  echo "Usage: $0 <mml file to convert to svg>"
  echo
  EXIT=1
fi


ERR=
if ! test -e $SVGM ; then
  echo "ERROR:  Python file '$MATH2SVG' not found at '$SVGM'."
  echo "        Modify SVGM in '$0' as required."
  ERR=1
fi
if ! test -d $ODIR ; then
  echo "ERROR:  Working subdirectory '$ODIR' not found."
  echo "        Modify ODIR in '$0' as required."
  ERR=1
fi

if [[ -n $EXIT ]] ; then
  if [[ -n $ERR ]] ; then
    echo "See inside '$0' for details."
  fi
  exit
fi

if [[ -n $ERR ]] ; then
  echo "ERROR exit."
  exit
fi


s=`basename $1 .mml`
OFIL="$ODIR/$s.svg"
TFIL="$ODIR/t.svg"
SVG_STANDALONE='--standalone'
IFIL=$1

# remove semantics element
sed 's|<semantics>||' < $1 > t1
sed 's|</semantics>||' < t1 > t2

`$SVGM $SVG_STANDALONE --output=$OFIL t2 2> /dev/null`
`xmllint --format $OFIL > $TFIL`
`mv $TFIL $OFIL`
rm t1 t2
echo "See svg file: $OFIL"
echo "Look at it in Inkscape to check or modify."