#!/bin/sh
#
# This is a simple script to translate autotools style
# configure options into CMake variables.  The resulting
# translation is then run as a CMake configure.

srcpath=$(dirname $0)
options="$srcpath"
while [ "$1" != "" ]
do
   case $1
   in
