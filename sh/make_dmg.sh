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
# clean installation. The script should generate a .dmg that works on
# 10.2+ but may require a recent version of Mac OS X to successfully
# generate the disk mounting image.
#
# Author: Christopher Sean Morrison
#
######################################################################

NAME="$1"
MAJOR_VERSION="$2"
MINOR_VERSION="$3"
PATCH_VERSION="$4"
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

DMG_CAPACITY=250
OPENUP=`dirname $0`/misc/macosx/openUp

PATH=/bin:/usr/bin:/usr/sbin
LC_ALL=C
umask 002

# TMPDIR=/tmp
if [ "x$TMPDIR" = "x" ] || [ ! -w $TMPDIR ] ; then
    if [ -w /usr/tmp ] ; then
	TMPDIR=/usr/tmp
    elif [ -w /var/tmp ] ; then
	TMPDIR=/var/tmp
    elif [ -w /tmp ] ; then
	TMPDIR=/tmp
    else
	TMPDIR=.
    fi
fi

VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
DMG_NAME="${NAME}-${VERSION}"
DMG="${DMG_NAME}.dmg"
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

if [ -f "${DMG}.sparseimage" ] ; then
    rm -f "${DMG}.sparseimage"
    if [ ! "x$?" = "x0" ] ; then
	echo "ERROR: unable to successfully remove the previous ${DMG}.sparseimage"
	exit 1
    fi
    if [ -f "${DMG}.sparseimage" ] ; then
	echo "ERROR: ${DMG}.sparseimage is in the way"
	exit 1
    fi
fi

hdiutil create "$DMG" -megabytes $DMG_CAPACITY -layout NONE -type SPARSE -volname $DMG_NAME
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: hdiutil failed to complete successfully"
    exit 1
fi
if [ ! -f "${DMG}.sparseimage" ] ; then
    echo "ERROR: hdiutil failed to create ${DMG}.sparseimage"
    exit 1
fi

hdidDisk=`hdid -nomount "${DMG}.sparseimage" | head -1 | grep '/dev/disk[0-9]*' | awk '{print $1}'`
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully get the hdid device name"
    exit 1
fi
if [ "x$hdidDisk" = "x" ] ; then
    echo "ERROR: unable to get the hdid device name"
    exit 1
fi

/sbin/newfs_hfs -w -v ${DMG_NAME} -b 4096 $hdidDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully create a new hfs filesystem on $hdidDisk"
    exit 1
fi

hdiutil eject $hdidDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully eject $hdidDisk"
    exit 1
fi

VOL_DIR="/Volumes/${DMG_NAME}"
if [ -d "$VOL_DIR" ] ; then
    echo "ERROR: there is already a disk mounted at $VOL_DIR"
    exit 1
fi

hdidMountedDisk=`hdid ${DMG}.sparseimage | head -1 | grep '/dev/disk[0-9]*' | awk '{print $1}'`
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully get the mounted hdid device name"
    exit 1
fi
if [ "x$hdidMountedDisk" = "x" ] ; then
    echo "ERROR: unable to get the mounted hdid device name"
    exit 1
fi

timeout=20
while [ $timeout -gt 0 ] ; do
    if [ -d "$VOL_DIR" ] ; then
	timeout=1
    fi
    timeout="`expr $timeout - 1`"
    sleep 1
done
if [ ! -d "$VOL_DIR" ] ; then
    echo "ERROR: timed out waiting for $DMG to mount"
    hdiutil eject $hdidMountedDisk
    exit 1
fi

PKG="${DMG_NAME}.pkg"
if [ -d "$PKG" ] ; then
    :
elif [ -d "/tmp/$PKG" ] ; then
    PKG="/tmp/${DMG_NAME}.pkg"
fi
if [ -d "$PKG" ] ; then
    if [ ! -r "$PKG" ] ; then
	echo "ERROR: unable to read the installer package"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi

    cp -pR "$PKG" "${VOL_DIR}/."
    if [ ! "x$?" = "x0" ] ; then
	echo "ERROR: unable to successfully copy $PKG to $VOL_DIR"
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

    argname="`basename $ARG`"
    if [ "x$argname" = "x" ] ; then
	echo "ERROR: unable to determine the $ARG base name"
	exit 1
    fi

    if [ -d "$ARG" ] ; then
	cp -pR "$ARG" "${VOL_DIR}/."
	if [ ! "x$?" = "x0" ] ; then
	    echo "ERROR: unable to successfully copy $ARG to $VOL_DIR"
	    hdiutil eject $hdidMountedDisk
	    exit 1
	fi
	if [ ! -d "${VOL_DIR}/${argname}" ] ; then
	    echo "ERROR: $argname failed to copy to the disk image"
	    exit 1
	fi

    elif [ -f "$ARG" ] ; then

	cp -p "$ARG" "${VOL_DIR}/."
	if [ ! "x$?" = "x0" ] ; then
	    echo "ERROR: unable to successfully copy $ARG to $VOL_DIR"
	    hdiutil eject $hdidMountedDisk
	    exit 1
	fi
	if [ ! -f "${VOL_DIR}/${argname}" ] ; then
	    echo "ERROR: $argname failed to copy to the disk image"
	    exit 1
	fi

	background="`echo $argname | sed 's/^[bB][aA][cC][kK][gG][rR][oO][uU][nN][dD]\.[a-zA-Z][a-zA-Z][a-zA-Z]*$/__bg__/'`"
	if [ "x$background" = "x__bg__" ] ; then

	    if [ ! -f "${VOL_DIR}/.background" ] ; then

		if [ -x /Developer/Tools/SetFile ] ; then
		    /Developer/Tools/SetFile -a V "${VOL_DIR}/${argname}"
		    if [ ! "x$?" = "x0" ] ; then
			echo "ERROR: unable to successfully set $argname invisible"
			hdiutil eject $hdidMountedDisk
			exit 1
		    fi
		fi

		cp -p "${VOL_DIR}/$argname" "${VOL_DIR}/.background"
		if [ ! "x$?" = "x0" ] ; then
		    echo "ERROR: unable to successfully copy $argname to .background"
		    hdiutil eject $hdidMountedDisk
		    exit 1
		fi
		if [ ! -f "${VOL_DIR}/.background" ] ; then
		    echo "ERROR: unable to copy the background image to .background on the disk image"
		    hdiutil eject $hdidMountedDisk
		    exit 1
		fi
		
		if [ -x /Developer/Tools/SetFile ] ; then
		    /Developer/Tools/SetFile -a V "${VOL_DIR}/.background"
		    if [ ! "x$?" = "x0" ] ; then
			echo "ERROR: unable to successfully set .background invisible"
			hdiutil eject $hdidMountedDisk
			exit 1
		    fi
		fi
	    fi
	fi
    else
	echo "ERROR: $ARG does not exist as a file or directory (sanity check)"
	hdiutil eject $hdidMountedDisk
	exit 1
    fi
done

if [ -x "$OPENUP" ] ; then
    $OPENUP "${VOL_DIR}/."
    if [ ! "x$?" = "x0" ] ; then
	echo "ERROR: sanity check failure -- unexpected error returned from $OPENUP ($?)"
	exit 1
    fi
fi
    
hdiutil eject $hdidMountedDisk
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully eject $hdidMountedDisk"
    exit 1
fi

# UDCO for pre 10.2
hdiutil convert "${DMG}.sparseimage" -o "$DMG" -format UDZO -imagekey zlib-level=9
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully compress $DMG via hdiutil convert"
    exit 1
fi
if [ ! -f "$DMG" ] ; then
    echo "ERROR: hdiutil failed to compress ${DMG}.sparseimage to $DMG"
    exit 1
fi

rm "${DMG}.sparseimage"
if [ ! "x$?" = "x0" ] ; then
    echo "ERROR: unable to successfully remove the ${DMG}.sparseimage temporary"
    exit 1
fi
if [ -f "${DMG}.sparseimage" ] ; then
    echo "ERROR: the ${DMG}.sparseimage temporary still exists"
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
