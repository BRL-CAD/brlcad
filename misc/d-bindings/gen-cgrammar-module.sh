#/bin/bash

PM=CGrammar

if [ -z "$1" ] ; then
  echo "Usage: $0 go"
  echo
  echo "Generates the Perl module for the C grammar: '$PM.pm'."
  exit
fi

cmd="perl -MParse::RecDescent - c-grammar.txt $PM"

`$cmd`

echo "See generated file '$PM.pm'."
