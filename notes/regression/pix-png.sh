#!/bin/sh
echo "checking pix-png"
echo "----------------"

if [ -f pix-png.log ] ; then
  rm pix-png.log
fi

pix-png testimage.pix >pix-png.png
if [ ! -f pix-png.png ] ; then
  echo "ERROR: pix-png output not found"
else
  rm pix-png.png
fi

