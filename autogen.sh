#!/bin/sh
# script to prepare the sources
###

libtoolize --force || glibtoolize --force
[ ! $? = 0 ] && echo "libtoolize failed" && echo 1
aclocal && [ ! $? = 0 ] && echo "aclocal failed" && exit 2
autoheader && [ ! $? = 0 ] && echo "autoheader failed" && exit 3
automake --add-missing --copy && [ ! $? = 0 ] && echo "automake failed" && exit 4
autoconf && [ ! $? = 0 ] && echo "autoconf failed" && exit 5

echo "BRL-CAD sources are now prepared. To build here, run:"
echo "  ./configure"
echo "  make"
