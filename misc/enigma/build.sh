#!/bin/sh
# configures and builds enigma
###

aclocal && \
autoheader && \
automake --add-missing --copy && \
autoconf

./configure $*
make

