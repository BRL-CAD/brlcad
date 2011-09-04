#!/bin/sh
#                     M A K E _ R P M . S H
# BRL-CAD
#
# Copyright (c) 2005-2011 United States Government as represented by
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

set -e

ferror(){
    echo "=========================================================="
    echo $1
    echo $2
    echo "=========================================================="
    exit 1
}

# test if in project root
if test ! -f misc/debian/control ; then
    ferror "\"make_rpm.sh\" should be run from project root directory." "Exiting..."
fi

# get distribution name
DNAME=""
fdist(){
    if test `cat /etc/*-release | grep -i "$1" | wc -l` -gt 0 ;then
	DNAME="$1"
    fi
}

fdist fedora
fdist openSUSE

if test "$DNAME" = "" ;then
    ferror "Only \"fedora\" and \"openSUSE\" suported at this time" "Exiting..."
fi

# check needed packages
E=0
if test ! -x "`which rpm 2>/dev/null`" ; then
    ferror "Missing \"rpm\" command" "Exiting..."
fi
fcheck(){
    if ! `rpm -q $1 &>/dev/null` ; then
	LLIST=$LLIST" "$1
	E=1
    fi
}

if test "$DNAME" = "fedora" ;then
    fcheck rpm-build
    fcheck fakeroot
    fcheck gcc-c++
    fcheck make
    fcheck cmake
    fcheck libtool
    fcheck bc
    fcheck sed
    fcheck bison
    fcheck flex
    fcheck libXi-devel
    fcheck libxslt
    fcheck mesa-libGLU-devel
    fcheck pango-devel
fi

if test "$DNAME" = "openSUSE" ;then
    fcheck rpm
    fcheck fakeroot
    fcheck gcc-c++
    fcheck make
    fcheck cmake
    fcheck libtool
    fcheck bc
    fcheck sed
    fcheck bison
    fcheck flex
    fcheck libXi6-devel
    fcheck libxslt
    fcheck Mesa-devel
    fcheck pango-devel
fi

if [ $E -eq 1 ]; then
    ferror "Mandatory to install these packages first:" "$LLIST"
fi

# set variables
BVERSION=`cat include/conf/MAJOR | sed 's/[^0-9]//g'`"."`cat include/conf/MINOR | sed 's/[^0-9]//g'`"."`cat include/conf/PATCH | sed 's/[^0-9]//g'`
TMPDIR="misc/$DNAME-tmp"
RELEASE="0"

if test `gcc -dumpmachine | grep -iE "i[3-6]86" | wc -l` -eq 1 ;then
    ARCH="i386"
    SARCH="i[3-6]86"
    FARCH="x86-32"
elif test `gcc -dumpmachine | grep -iE "x86_64" | wc -l` -eq 1 ;then
    ARCH=`uname -m`
    SARCH=$ARCH
    FARCH="x86-64"
else
    ferror "Unknown architecture. \"`gcc -dumpmachine`\"" "Exiting..."
fi

NJOBS=`getconf _NPROCESSORS_ONLN | sed "s/.*/&*2-1/" | bc`
if test ! $NJOBS -gt 0 2>/dev/null ;then
    NJOBS=1
fi

# #
rm -Rf $TMPDIR
mkdir -p $TMPDIR/tmp
cp -Rf misc/debian/* $TMPDIR

# compile and install in tmp dir
cmake -DBRLCAD-ENABLE_OPTIMIZED_BUILD=ON \
      -DBRLCAD-ENABLE_ALL_LOCAL_LIBS=ON \
      -DBRLCAD-ENABLE_STRICT=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/brlcad \
      -DDATA_DIR=share \
      -DMAN_DIR=share/man \
      -DBRLCAD_BUNDLED_LIBS=BUNDLED
make -j$NJOBS
fakeroot make install DESTDIR=`pwd`"/$TMPDIR/tmp"

# copy menu files
mkdir -p $TMPDIR/tmp/etc/profile.d
cp -f $TMPDIR/brlcad.sh $TMPDIR/tmp/etc/profile.d

mkdir -p $TMPDIR/tmp/etc/xdg/menus/applications-merged
cp -f $TMPDIR/brlcad.menu $TMPDIR/tmp/etc/xdg/menus/applications-merged

mkdir -p $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-archer.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-mged.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-rtwizard.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-db.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc-animation.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc-mged.desktop $TMPDIR/tmp/usr/share/applications

mkdir -p $TMPDIR/tmp/usr/share/desktop-directories
cp -f $TMPDIR/brlcad.directory $TMPDIR/tmp/usr/share/desktop-directories
cp -f $TMPDIR/brlcad-doc.directory $TMPDIR/tmp/usr/share/desktop-directories

mkdir -p $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-mged.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-archer.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-db.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-doc.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps

mkdir -p $TMPDIR/tmp/usr/share/icons/hicolor/48x48/mimetypes
cp -f $TMPDIR/brlcad-v4.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/mimetypes
cp -f $TMPDIR/brlcad-v5.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/mimetypes

mkdir -p $TMPDIR/tmp/usr/share/mime/packages
cp -f $TMPDIR/brlcad.xml $TMPDIR/tmp/usr/share/mime/packages

#Create brlcad.spec file
echo -e 'Name: brlcad
Version: '$BVERSION'
Release: '$RELEASE'
Summary: BRL-CAD open source solid modeling

Group: Productivity/Graphics/CAD
License: LGPL, BSD
URL: http://brlcad.org
Packager: Jordi Sayol <g.sayol@yahoo.es>

ExclusiveArch: '$ARCH'
Provides: brlcad = '$BVERSION'-'$RELEASE', brlcad('$FARCH') = '$BVERSION'-'$RELEASE > $TMPDIR/brlcad.spec

if test "$DNAME" = "fedora" ;then
    echo -e 'Requires: xorg-x11-fonts-ISO8859-1-75dpi' >> $TMPDIR/brlcad.spec
fi

echo -e '
%description
BRL-CAD is a powerful cross-platform Open Source combinatorial
Constructive Solid Geometry (CSG) solid modeling system that
includes interactive 3D solid geometry editing, high-performance
ray-tracing support for rendering and geometric analysis,
network-distributed framebuffer support, image and signal-processing
tools, path-tracing and photon mapping support for realistic image
synthesis, a system performance analysis benchmark suite, an embedded
scripting interface, and libraries for robust high-performance
geometric representation and analysis.

Homepage: http://brlcad.org

%post
set -e

F="/usr/share/applications/defaults.list"

if [ ! -f $F ]; then
	echo "[Default Applications]" > $F
else
	sed -i "/application\/brlcad-/d" $F
fi

echo "application/brlcad-v4=brlcad-mged.desktop" >> $F
echo "application/brlcad-v5=brlcad-mged.desktop" >> $F

source /etc/profile.d/brlcad.sh

update-desktop-database &> /dev/null || :

update-mime-database /usr/share/mime &>/dev/null || :

touch -c /usr/share/icons/hicolor &>/dev/null || :

gtk-update-icon-cache /usr/share/icons/hicolor &>/dev/null || :

SuSEconfig &>/dev/null || :

%postun
set -e

update-desktop-database &> /dev/null || :

update-mime-database /usr/share/mime &>/dev/null || :

touch -c /usr/share/icons/hicolor &>/dev/null || :

gtk-update-icon-cache /usr/share/icons/hicolor &>/dev/null || :

SuSEconfig &>/dev/null || :

%files' >> $TMPDIR/brlcad.spec

find $TMPDIR/tmp/ -type d | sed 's:'$TMPDIR'/tmp:%dir ":' | sed 's:$:":' >> $TMPDIR/brlcad.spec
find $TMPDIR/tmp/ -type f | sed 's:'$TMPDIR'/tmp:":' | sed 's:$:":' >> $TMPDIR/brlcad.spec
find $TMPDIR/tmp/ -type l | sed 's:'$TMPDIR'/tmp:":' | sed 's:$:":' >> $TMPDIR/brlcad.spec

# create rpm file
fakeroot rpmbuild -vv --buildroot=`pwd`/$TMPDIR/tmp -bb --target $ARCH $TMPDIR/brlcad.spec > $TMPDIR/rpmbuild.log

RPMFILE=`cat $TMPDIR"/rpmbuild.log" | grep "brlcad-"$BVERSION"-"$RELEASE"."$ARCH".rpm" | awk '{print $(NF)}'`

mv $RPMFILE ../brlcad-$BVERSION-$RELEASE.$DNAME.$ARCH.rpm

# #
rm -Rf $TMPDIR

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
