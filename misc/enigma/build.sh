#!/bin/sh
# configures and builds enigma
###

if ! test -f AUTHORS ; then
    touch AUTHORS
fi
if ! test -f ChangeLog ; then
    touch ChangeLog
fi
if ! test -f INSTALL ; then
    touch INSTALL
fi
if ! test -f NEWS ; then
    touch NEWS
fi
aclocal && \
autoheader && \
automake --add-missing --copy && \
autoconf

./configure $*
make

