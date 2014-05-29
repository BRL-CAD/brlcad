#/bin/bash

PM=CGrammar
PM2=CGrammar2

if [ -z "$1" ] ; then
  echo "Usage: $0 go"
  echo
  echo "Generates the Perl modules for the C grammars:"
  echo "  $PM.pm"
  echo "  $PM2.pm"
  exit
fi

cmd="perl -MParse::RecDescent - c-grammar.txt $PM"
cmd2="perl -MParse::RecDescent - c-grammar2.txt $PM2"

`$cmd`
`$cmd2`

echo "See generated files '$PM.pm' and '$PM2.pm'."
