#!/bin/sh
#                     M A K E _ D E B . S H
# BRL-CAD
#
# Copyright (c) 2005-2010 United States Government as represented by
# the U.S. Army Research Laboratory.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following
# disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# 3. The name of the author may not be used to endorse or promote
# products derived from this software without specific prior written
# permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
###

test -e

BVERSION=`cat include/conf/MAJOR`"."`cat include/conf/MINOR`"."`cat include/conf/PATCH`
BVERSION=`echo $BVERSION | sed 's/[^0-9.]//g'`
CDATE=`date -R`
CFILE="misc/debian/changelog"

if test ! -e /etc/debian_version ; then
    echo "Refusing to build on a non-debian system."
    exit 1
fi

# check needed packages
E=0
fcheck(){
    T="install ok installed"
    if test `dpkg -s $1 2>/dev/null | grep "$T" | wc -l` -eq 0 ; then
	LLIST=$LLIST"\n"$1
	E=1
    fi
}

fcheck build-essential
fcheck fakeroot
fcheck debhelper
fcheck bison
fcheck flex
fcheck libxi-dev
fcheck xsltproc
fcheck libgl1-mesa-dev

if [ $E -eq 1 ]; then
    echo "=========================================================="
    echo "Mandatory to install these packages first:"
    echo $LLIST
    echo "=========================================================="
    exit 1
fi

fcheck libpango1.0-dev

if [ $E -eq 1 ]; then
    echo "=========================================================="
    echo "For a better font rendering, install this package first:"
    echo $LLIST
    echo "=========================================================="
    echo "Pausing 15 seconds..."
    sleep 15
fi

# modify doc menu desktop files
fdoc(){
    L=`sed -n '/Exec=/=' $2`
    A=`sed -n $L'p' $2`
    if test ! "Exec=$1" = "$A" ;then
	sed -i "s:$A:Exec=$1:" $2
    fi
}

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/html/toc.html" "misc/debian/brlcad-doc.desktop"

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/db" "misc/debian/brlcad-db.desktop"

# update debian/chagelog if needed
if test -s $CFILE && test `sed -n '1p' $CFILE | grep "brlcad ($BVERSION-" | wc -l` = 0 ; then
    L1="1s/^/brlcad ($BVERSION-0) unstable; urgency=low\n\n"
    L2="  **** VERSION ENTRY AUTOMATICALLY ADDED BY \"sh\/make_deb.sh\" SCRIPT ****\n\n"
    L3=" -- Jordi Sayol <g.sayol@yahoo.es>  $CDATE\n\n/"
    sed -i "$L1$L2$L3" $CFILE
fi

# create link to misc/debian
if test ! -e ./debian && test ! -e ./debian/control ; then
    ln -fs misc/debian debian
fi

# create deb package
fakeroot debian/rules binary

# remove link to misc/debian
if test -L ./debian ; then rm debian ; fi

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
