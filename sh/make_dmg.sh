#!/bin/sh
#                     M A K E _ D M G . S H
# BRL-CAD
#
# Copyright (c) 2005 United States Government as represented by
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
#
# Script for generating a Mac OS X disk mounting image (.dmg) from a
# clean installation.  The dmg should be compatible with all versions
# of the autodiskmount daemon (tested on 10.2 and 10.3).
#
# Author: Christopher Sean Morrison
#
######################################################################

NAME="$1"
MAJOR_VERSION="$2"
MINOR_VERSION="$3"
PATCH_VERSION="$4"
DMG_CAPACITY=100

if [ "x$NAME" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version [background] [contents ...]"
    echo "ERROR: must specify a title for the package name"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version [background] [contents ...]"
    echo "ERROR: must specify a major package version"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "ERROR: must specify a minor package version"
    echo "Usage: $0 title major_version minor_version patch_version [background] [contents ...]"
    exit 1
fi
if [ "x$PATCH_VERSION" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version [background] [contents ...]"
    echo "ERROR: must specify a patch package version"
    exit 1
fi
shift 4

#if [ "x`id -u`" != "x0" ] ; then
#    echo "This script requires superuser privileges, restarting via sudo"
#    sudo "$0" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
#    exit $?
#fi

VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
DMG="${NAME}-${VERSION}.dmg"
if [ -d "$DMG" ] ; then
    echo "ERROR: there is a directory of same name in the way of creating $DMG"
    exit 1
fi
if [ -f "$DMG" ] ; then
    remove=""
    while [ "x$remove" = "x" ] ; do
	echo "WARNING: the disk mounting image ($DMG) already exists, remove it?"
	echo -n "yes/no? "
	read remove
	case x$remove in
	    x[yY][eE][sS])
	        remove=yes
		;;
	    x[yY])
	        remove=yes
		;;
	    x[nN][oO])
	        remove=no
		;;
	    x[nN])
	        remove=no
		;;
	    *)
		remove=""
		;;
	esac
    done

    if [ "x$remove" = "xyes" ] ; then
	rm -f "$DMG"
    fi
    if [ -f "$DMG" ] ; then
	echo "ERROR: cannot continue with $DMG in the way"
	exit 1
    fi
fi

hdiutil create "$DMG" -megabytes $DMG_CAPACITY -layout NONE
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: hdiutil failed to complete successfully"
    exit 1
fi
if [ ! -f "$DMG" ] ; then
    echo "ERROR: hdiutil failed to create $DMG"
    exit 1
fi

hdidDisk=`hdid -nomount "$DMG" | head -1 | grep '/dev/disk[0-9]*' | awk '{print $1}'`
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully get the hdid device name"
    exit 1
fi
if [ "x$hdidDisk" = "x" ] ; then
    echo "ERROR: unable to get the hdid device name"
    exit 1
fi

/sbin/newfs_hfs -w -v ${NAME}-${VERSION} -b 4096 $hdidDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully create a new hfs filesystem on $hdidDisk"
    exit 1
fi

hdiutil eject $hdidDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully eject $hdidDisk"
    exit 1
fi

hdidMountedDisk=`hdid $DMG | head -1 | grep '/dev/disk[0-9]*' | awk '{print $1}'`
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully get the mounted hdid device name"
    exit 1
fi
if [ "x$hdidMountedDisk" = "x" ] ; then
    echo "ERROR: unable to get the mounted hdid device name"
    exit 1
fi

timeout=20
VOL_DIR="/Volumes/${NAME}-${VERSION}"
while [ $timeout -gt 0 ] ; do
    if [ -d "$VOL_DIR" ] ; then
	timeout=1
    fi
    timeout="`expr $timeout - 1`"
    sleep 1
done
if [ ! -d "$VOL_DIR" ] ; then
    echo "ERROR: timed out waiting for $DMG to mount"
    exit 1
fi

PKG_NAME="${NAME}-${VERSION}.pkg"
if [ -d "${PKG_NAME}" ] ; then
    if [ ! -r "${PKG_NAME}" ] ; then
	echo "ERROR: unable to read the installer package"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi

    cp -pR "$PKG_NAME" "${VOL_DIR}/."
    if [ ! "x$?" = "x0" ] ; then
	echo "ERROR: unable to successfully copy $PKG_NAME to $VOL_DIR"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi
fi

while [ ! "x$*" = "x" ] ; do
    ARG="$1"
    shift

    if [ ! -r "$ARG" ] ; then
	if [ ! -f "$ARG" ] ; then
	    echo "ERROR: specified content ($ARG) does not exist"
	    hdiutil eject $hdidMountedDisk
	    exit 1
	fi
	echo "ERROR: specified content ($ARG) is not readable"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi

    if [ -d "$ARG" ] ; then
	cp -pR "$ARG" "${VOL_DIR}/."
	if [ ! "x$?" = "x0" ] ; then
	    echo "ERROR: unable to successfully copy $ARG to $VOL_DIR"
	    hdiutil eject $hdidMountedDisk
	    exit 1
	fi
    elif [ -f "$ARG" ] ; then

	cp -p "$ARG" "${VOL_DIR}/."
	if [ ! "x$?" = "x0" ] ; then
	    echo "ERROR: unable to successfully copy $ARG to $VOL_DIR"
	    hdiutil eject $hdidMountedDisk
	    exit 1
	fi

	filename="`basename $ARG`"
	if [ ! "x$filename" = "x" ] ; then
	    background="`echo $filename | sed 's/^[bB][aA][cC][kK][gG][rR][oO][uU][nN][dD]\.[a-zA-Z][a-zA-Z][a-zA-Z]*$/__bg__/'`"
	    if [ "x$background" = "x__bg__" ] ; then
		echo "$ARG is a background"
	    fi
	fi
    else
	echo "ERROR: $ARG does not exist as a file or directory (sanity check)"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi
done

hdiutil eject $hdidMountedDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully eject $hdidMountedDisk"
    exit 1
fi

mv "$DMG" "${DMG}.prep.dmg"
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully rename $DMG to ${DMG}.prep.dmg"
    exit 1
fi
if [ ! -f "${DMG}.prep.dmg" ] ; then
    echo "ERROR: failed to rename $DMG to ${DMG}.prep.dmg"
    exit 1
fi

hdiutil convert "${DMG}.prep.dmg" -format UDCO -o "$DMG"
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully compress $DMG via hdiutil convert"
    exit 1
fi
if [ ! -f "$DMG" ] ; then
    echo "ERROR: hdiutil failed to compress ${DMG}.prep.dmg to $DMG"
    exit 1
fi

rm "${DMG}.prep.dmg"
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully remove the ${DMG}.prep.dmg temporary"
    exit 1
fi
if [ -f "${DMG}.prep.dmg" ] ; then
    echo "ERROR: the ${DMG}.prep.dmg temporary still exists"
    exit 1
fi


# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
