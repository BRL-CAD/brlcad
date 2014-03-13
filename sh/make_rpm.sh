#!/bin/sh
#                     M A K E _ R P M . S H
# BRL-CAD
#
# Copyright (c) 2005-2014 United States Government as represented by
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
    ferror "Only \"fedora\" and \"openSUSE\" supported at this time" "Exiting..."
fi

# check needed packages
E=0
if test ! -x "`which rpm 2>/dev/null`" ; then
    ferror "Missing \"rpm\" command" "Exiting..."
fi
fcheck(){
    if ! `rpm -q $1 &>/dev/null` ; then
	echo "* Missing $1..."
	LLIST=$LLIST" "$1
	E=1
    else
	echo "Found package $1..."
    fi
}

if test "$DNAME" = "fedora" ;then
    fcheck rpm-build
    fcheck fakeroot
    fcheck gcc-c++
    fcheck make
    fcheck cmake
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
BVERSION="`sed 's/[^0-9]//g' include/conf/MAJOR`.`sed 's/[^0-9]//g' include/conf/MINOR`.`sed 's/[^0-9]//g' include/conf/PATCH`"
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

NJOBS=`getconf _NPROCESSORS_ONLN`
if test ! $NJOBS -gt 0 2>/dev/null ;then
    NJOBS=1
fi

# #
rm -Rf $TMPDIR
mkdir -p $TMPDIR/tmp
cp -Rf misc/debian/* $TMPDIR

# create "version" file
echo $BVERSION >$TMPDIR/version

# compile and install in tmp dir
cmake -DBRLCAD_BUNDLED_LIBS=ON \
      -DBRLCAD_FLAGS_OPTIMIZATION=ON \
      -DBRLCAD_ENABLE_STRICT=OFF \
      -DBRLCAD_FLAGS_DEBUG=OFF \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=/usr/brlcad \
      -DDATA_DIR=share \
      -DMAN_DIR=share/man
make -j$NJOBS
fakeroot make install DESTDIR=`pwd`"/$TMPDIR/tmp"

# copy menu files and others
mkdir -p $TMPDIR/tmp/etc/profile.d
cp -f $TMPDIR/brlcad.sh $TMPDIR/tmp/etc/profile.d

mkdir -p $TMPDIR/tmp/etc/xdg/menus/applications-merged
cp -f $TMPDIR/brlcad.menu $TMPDIR/tmp/etc/xdg/menus/applications-merged

mkdir -p $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/archer.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-db.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc-animation.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/brlcad-doc-mged.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/mged.desktop $TMPDIR/tmp/usr/share/applications
cp -f $TMPDIR/rtwizard.desktop $TMPDIR/tmp/usr/share/applications

mkdir -p $TMPDIR/tmp/usr/share/desktop-directories
cp -f $TMPDIR/brlcad.directory $TMPDIR/tmp/usr/share/desktop-directories
cp -f $TMPDIR/brlcad-doc.directory $TMPDIR/tmp/usr/share/desktop-directories

mkdir -p $TMPDIR/tmp/usr/share/mime/packages
cp -f $TMPDIR/brlcad.xml $TMPDIR/tmp/usr/share/mime/packages

mkdir -p $TMPDIR/tmp/usr/brlcad
cp -f $TMPDIR/version $TMPDIR/tmp/usr/brlcad

# copy icons
for I in 16x16 24x24 36x36 48x48 64x64 96x96 128x128 256x256
do
    mkdir -p $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/archer.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/brlcad.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/brlcad-db.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/brlcad-doc.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/mged.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps
    cp -f $TMPDIR/icons/$I/rtwizard.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/apps

    mkdir -p $TMPDIR/tmp/usr/share/icons/hicolor/$I/mimetypes
    cp -f $TMPDIR/icons/$I/brlcad-v4.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/mimetypes
    cp -f $TMPDIR/icons/$I/brlcad-v5.png $TMPDIR/tmp/usr/share/icons/hicolor/$I/mimetypes
done

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

file="/usr/share/applications/mimeapps.list"
section="Added Associations"
app="mged.desktop;archer.desktop;rtwizard.desktop;"
key_v4="application/brlcad-v4=$app"
key_v5="application/brlcad-v5=$app"

if [ ! -e $file ]
then
    touch $file || :
fi

if [ -f $file ]
then
    sed --follow-symlinks -i "/application\/brlcad-v[45]/d" $file || :
    line=$(sed -n "/^\[$section\]/=" $file | tail -1) || :
    if [ -z "$line" ]
    then
	echo "[$section]\n$key_v4\n$key_v5" >> $file || :
    else
	sed --follow-symlinks -i "$line a$key_v4\n$key_v5" $file || :
    fi
fi

update-mime-database /usr/share/mime || :
update-desktop-database -q || :
gtk-update-icon-cache -qf /usr/share/icons/hicolor || :' >> $TMPDIR/brlcad.spec

if test "$DNAME" = "openSUSE" ;then
    echo -e 'SuSEconfig || :' >> $TMPDIR/brlcad.spec
fi

echo -e '
%postun

file="/usr/share/applications/mimeapps.list"
section="Added Associations"

if [ -f $file ] && [ $1 -eq 0 ]
then
    sed --follow-symlinks -i "/application\/brlcad-v[45]/d" $file
fi

if [ -f $file ] && [ -z "$(sed "/\[$section\]/d" $file)" ]
then
    rm $file || :
fi

update-mime-database /usr/share/mime || :
update-desktop-database -q || :
gtk-update-icon-cache -qf /usr/share/icons/hicolor || :' >> $TMPDIR/brlcad.spec

if test "$DNAME" = "openSUSE" ;then
    echo -e 'SuSEconfig || :' >> $TMPDIR/brlcad.spec
fi

echo -e '
%files' >> $TMPDIR/brlcad.spec

find $TMPDIR/tmp/usr/brlcad -type d | sed 's:'$TMPDIR'/tmp:%dir ":' | sed 's:$:":' >> $TMPDIR/brlcad.spec
find $TMPDIR/tmp/ ! -type d | sed 's:'$TMPDIR'/tmp:":' | sed 's:$:":' >> $TMPDIR/brlcad.spec

# create rpm file
fakeroot rpmbuild -vv --buildroot=`pwd`/$TMPDIR/tmp -bb --target $ARCH $TMPDIR/brlcad.spec > $TMPDIR/rpmbuild.log

RPMFILE=`grep "brlcad-$BVERSION-$RELEASE.$ARCH.rpm" "$TMPDIR/rpmbuild.log" | awk '{print $(NF)}'`

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
