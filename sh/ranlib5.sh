#!/bin/sh
# System V substitute for RANLIB which does not blather on stdout
#  $Header$

ar ts $1 2>&1 >/dev/null
