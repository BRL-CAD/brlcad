#!/bin/sh
if [ -f d-a.log ] ; then
  rm d-a.log
fi
d-a
if [ -f d-a.log ] ; then
  echo "WARNING: d-a output when none expected"
  rm d-a.log
fi
d-a <badtestascii.txt >d-a.log
if [ -f d-a.log ] ; then
  echo "WARNING: d-a output when none expected"
  rm d-a.log
fi
d-a <testascii.txt >d-a.log
if [ -f d-a.log ] ; then 
  echo "WARNING: d-a output when none expected"
  rm d-a.log
fi
d-a <testdouble.bin >d-a.log
if [ ! -f d-a.log ] ; then
  echo "ERROR: d-a output not found"
fi
d-a <testdouble2.bin >d-a.log
if [ ! -f d-a.log ] ; then
  echo "ERROR: d-a output not found"
fi
d-a <testdouble3.bin >d-a.log
if [ ! -f d-a.log ] ; then
  echo "ERROR: d-a output not found"
fi
