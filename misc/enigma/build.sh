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


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
