#!/bin/sh
echo "checking #"
echo "---------#"

if [ -f #.log ] ; then
  rm #.log
fi

# command here
if [ ! -f # ] ; then
  echo "ERROR: # output not found"
fi

echo "done checking #"
