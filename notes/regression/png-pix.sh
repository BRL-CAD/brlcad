#!/bin/sh
echo "checking png-pix"
echo "----------------"

if [ -f png-pix.log ] ; then
  rm png-pix.log
fi

png-pix testimage.png >png-pix.pix
if [ ! -f png-pix.pix ] ; then
  echo "ERROR: png-pix output not found"
else
  rm png-pix.pix
fi

