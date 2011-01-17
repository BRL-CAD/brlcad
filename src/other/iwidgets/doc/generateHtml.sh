#!/bin/ksh
#

manpages=`\ls *.n`

for doc in $manpages
do
  echo "Generating html/$doc.html"
  nroff -man $doc | rman -f html > html/$doc.html
done
