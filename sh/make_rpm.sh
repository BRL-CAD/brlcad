#!/bin/sh
#                     M A K E _ R P M . S H
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

ferror(){
    echo "=========================================================="
    echo $1
    echo $2
    echo "=========================================================="
    exit 1
}

# test if in project root
if test ! -f misc/debian/control ; then
    ferror "\"make_rpm.sh\" should run from project root directory." "Exiting..."
fi

# get distribution name
DNAME=""
fdist(){
    if test `cat /etc/*-release | grep -i "$1" | wc -l` -gt 0 ;then
	DNAME="$1"
    fi
}

fdist fedora
#fdist opensuse

if test "$DNAME" = "" ;then
    ferror "only \"fedora\" suported at this moment" "Exiting..."
fi

# set variables
BVERSION=`cat include/conf/MAJOR`"."`cat include/conf/MINOR`"."`cat include/conf/PATCH`
BVERSION=`echo $BVERSION | sed 's/[^0-9.]//g'`
TMPDIR="misc/$DNAME-tmp"

if test `uname -m | grep -iwE "i[3-6]86" | wc -l` -eq 1 ;then
    ARCH=`uname -m`
    SARCH="i[3-6]86"
    FARCH="x86-32"
fi
if test `uname -m | grep -iwE "x86_64" | wc -l` -eq 1 ;then
    ARCH=`uname -m`
    SARCH=$ARCH
    FARCH="x86-64"
fi

# check needed packages
echo "checking system..."
E=0
fcheck(){
    if test `yum list installed | grep -i "\<$1\.$SARCH" | wc -l` -eq 0 ; then
	LLIST=$LLIST" "$1
	E=1
    fi
}

if test "$DNAME" = "fedora" ;then
    fcheck rpm-build
    fcheck fakeroot
    fcheck gcc-c++
    fcheck make
    fcheck bison
    fcheck flex
    fcheck libXi-devel
    fcheck libxslt
    fcheck mesa-libGL-devel
    fcheck pango-devel
    fcheck ncurses-devel
fi

if [ $E -eq 1 ]; then
    ferror "Mandatory to install these packages first:" "$LLIST"
fi

# #
rm -Rf $TMPDIR
mkdir -p $TMPDIR/tmp
cp -Rf misc/debian/* $TMPDIR

# modify doc menu desktop files
fdoc(){
    L=`sed -n '/Exec=/=' $2`
    A=`sed -n $L'p' $2`
    if test ! "Exec=$1" = "$A" ;then
	sed -i "s:$A:Exec=$1:" $2
	echo "\"$2\" has been modified!"
    fi
}

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/html/toc.html" \
 "$TMPDIR/brlcad-doc.desktop"

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/db" \
 "$TMPDIR/brlcad-db.desktop"

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/html/manuals/mged/index.html" \
 "$TMPDIR/brlcad-doc-mged.desktop"

fdoc "xdg-open /usr/brlcad/share/brlcad/$BVERSION/html/manuals/Anim_Tutorial/index.html" \
 "$TMPDIR/brlcad-doc-animation.desktop"

# compile and install in tmp dir
./configure --enable-optimized --with-ogl --disable-debug
make -j`getconf _NPROCESSORS_ONLN`
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
cp -f $TMPDIR/brlcad.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-archer.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-db.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps
cp -f $TMPDIR/brlcad-doc.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/apps

mkdir -p $TMPDIR/tmp/usr/share/icons/hicolor/48x48/mimetypes
cp -f $TMPDIR/application-x-brlcad-extension.png $TMPDIR/tmp/usr/share/icons/hicolor/48x48/mimetypes

mkdir -p $TMPDIR/tmp/usr/share/mime/packages
cp -f $TMPDIR/application-x-brlcad-extension.xml $TMPDIR/tmp/usr/share/mime/packages

#Create brlcad.spec file
echo -e 'Name: brlcad
Version: '$BVERSION'
Release: 0
Summary: BRL-CAD open source solid modeling

Group: Productivity/Graphics/CAD
License: LGPL, BSD
URL: http://brlcad.org
Packager: Jordi Sayol <g.sayol@yahoo.es>

ExclusiveArch: '$ARCH' 
Provides: brlcad = '$BVERSION'-0, brlcad('$FARCH') = '$BVERSION'-0

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
source /etc/profile.d/brlcad.sh

update-desktop-database &> /dev/null

if [ -x "`which update-mime-database 2>/dev/null`" ]; then
	update-mime-database /usr/share/mime
fi

%postun
update-desktop-database &> /dev/null

if [ -x "`which update-mime-database 2>/dev/null`" ]; then
	update-mime-database /usr/share/mime
fi

%files' > $TMPDIR/brlcad.spec

find $TMPDIR/tmp/ -type d | sed 's:'$TMPDIR'/tmp:%dir ":' | sed 's:$:":' >> $TMPDIR/brlcad.spec
find $TMPDIR/tmp/ -type f | sed 's:'$TMPDIR'/tmp:":' | sed 's:$:":' >> $TMPDIR/brlcad.spec
find $TMPDIR/tmp/ -type l | sed 's:'$TMPDIR'/tmp:":' | sed 's:$:":' >> $TMPDIR/brlcad.spec

# create rpm file
fakeroot rpmbuild -vv --buildroot=`pwd`/$TMPDIR/tmp -bb --target $ARCH $TMPDIR/brlcad.spec > $TMPDIR/rpmbuild.log

RPMFILE=`cat $TMPDIR"/rpmbuild.log" | grep "brlcad-"$BVERSION"-0."$ARCH".rpm" | awk '{print $(NF)}'`

mv $RPMFILE ../brlcad-$BVERSION-0_$DNAME.$ARCH.rpm

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
