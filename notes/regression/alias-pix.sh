#!/bin/sh
echo "checking alias-pix"
echo "------------------"

if [ -f alias-pix.log ] ; then
  rm alias-pix.log
fi

# command here

if [ ! -f alias-pix.log ] ; then
  echo "ERROR: alias-pix output not found"
fi

echo "done checking #"


echo "done checking alias-pix"

