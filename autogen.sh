#!/bin/sh
# script to prepare the sources
###

which autoreconf > /dev/null 2>&1
if [ $? = 0 ] ; then
    echo -n "Autoconfiguring..."
    autoreconf --force --install
    [ ! $? = 0 ] && echo "autoreconf failed" && exit 1
else
    echo -n "Configuring..."
    libtoolize --automake --copy --force || glibtoolize --automake --copy --force
    [ ! $? = 0 ] && echo "libtoolize failed" && exit 1
    aclocal && [ ! $? = 0 ] && echo "aclocal failed" && exit 2
    autoheader && [ ! $? = 0 ] && echo "autoheader failed" && exit 3
    automake --add-missing --copy && [ ! $? = 0 ] && echo "automake failed" && exit 4
    autoconf && [ ! $? = 0 ] && echo "autoconf failed" && exit 5
fi
echo "done"
echo
echo "The BRL-CAD build system is now prepared.  To build here, run:"
echo "  ./configure"
echo "  make"
