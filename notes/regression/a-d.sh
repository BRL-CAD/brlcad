#!/bin/sh
echo "checking a-d"
echo "------------"
if [ -f a-d.log ] ; then
  rm a-d.log
fi
a-d
if [ -f a-d.log ] ; then
  echo "WARNING: a-d output when none expected"
  rm a-d.log
fi
a-d <badtestascii.txt >a-d.log
if [ -f a-d.log ] ; then
  echo "WARNING: a-d output when none expected"
  rm a-d.log
fi
a-d <testascii.txt >a-d.log
if [ -f a-d.log ] ; then 
  echo "WARNING: a-d output when none expected"
  rm a-d.log
fi
a-d <testasciiint.txt >a-d.log
if [ ! -f a-d.log ] ; then
  echo "ERROR: a-d output not found"
fi
a-d <testasciifloat.txt >a-d.log
if [ ! -f a-d.log ] ; then
  echo "ERROR: a-d output not found"
fi
a-d <testasciidouble.txt >a-d.log
if [ ! -f a-d.log ] ; then
  echo "ERROR: a-d output not found"
fi
a-d <testlongtestasciiint012345678901234567890123456789012345678901234567890.txt >a-d.log
if [ ! -f a-d.log ] ; then
  echo "ERROR: a-d broken on long file name"
fi

echo "done checking a-d"
