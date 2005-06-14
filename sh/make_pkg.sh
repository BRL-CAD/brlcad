#!/bin/sh
#                     M A K E _ P K G . S H
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
# Script for generating a Mac OS X Installer package (.pkg) from a
# clean package installation.  The package should be compatible with
# old and new versions of Installer.
#
# Author: Christopher Sean Morrison
#
######################################################################

NAME="$1"
MAJOR_VERSION="$2"
MINOR_VERSION="$3"
PATCH_VERSION="$4"
ARCHIVE="$5"
RESOURCES="$6"
if [ "x$NAME" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version archive_dir [resource_dir]"
    echo "ERROR: must specify a title for the package name"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version archive_dir [resource_dir]"
    echo "ERROR: must specify a major package version"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "ERROR: must specify a minor package version"
    echo "Usage: $0 title major_version minor_version patch_version archive_dir [resource_dir]"
    exit 1
fi
if [ "x$PATCH_VERSION" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version archive_dir [resource_dir]"
    echo "ERROR: must specify a patch package version"
    exit 1
fi
if [ "x$ARCHIVE" = "x" ] ; then
    echo "Usage: $0 title major_version minor_version patch_version archive_dir [resource_dir]"
    echo "ERROR: must specify an archive directory"
    exit 1
fi
if [ ! -d "$ARCHIVE" ] ; then
    echo "ERROR: specified archive path (${ARCHIVE}) is not a directory"
    exit 1
fi
if [ "x$RESOURCES" = "x" ] ; then
    RESOURCES="none>>make_pkg_sh<<none"
else
    if [ ! -d "$RESOURCES" ] ; then
	echo "ERROR: specified resource path (${RESOURCES}) is not a readable directory"
	exit 1
    fi
fi    

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

PRE_PWD="`pwd`"
VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
PKG_NAME="${NAME}-${VERSION}"
PKG="${PKG_NAME}.pkg"

if [ -f "$PKG" ] ; then
    echo "ERROR: there is a file with the same name in the way of creating $PKG"
    exit 1
fi

if [ "x`id -u`" != "x0" ] ; then
    echo "$0 requires superuser privileges, restarting via sudo"
    sudo "$0" "$1" "$2" "$3" "$4" "$5" "$6" "$7" "$8" "$9"
    retval="$?"
    if [ ! "x$retval" = "x0" ] ; then
	exit $retval
    fi
    if [ ! -d "$PKG" ] ; then
	if [ -d "${TMPDIR}/$PKG" ] ; then
	    cp -pR ${TMPDIR}/$PKG .
	    if [ ! "x$?" = "x0" ] ; then
		echo "ERROR: unsuccessfully moved ${TMPDIR}/$PKG to `pwd`"
		exit 1
	    fi
	    if [ ! -d "$PKG" ] ; then
		echo "ERROR: unable to move ${TMPDIR}/$PKG to `pwd`"
		exit 1
	    fi
	else
	    echo "ERROR: sanity check .. could not find $PKG"
	    exit 1
	fi
    fi
    if [ -d "$PKG" ] ; then
	echo "CREATED `pwd`/$PKG"
    fi

    exit 0
fi

exists_writeable=no
if [ -d "$PKG" ] ; then
    if [ -w "$PKG" ] ; then
	exists_writeable=yes
    fi

else
    mkdir "$PKG" > /dev/null 2>&1
    if [ ! -d "$PKG" ] ; then
	echo "WARNING: unable to create the package directory in `pwd` (perhaps it's an NFS filesystem?)"
	
	if [ ! -w ${TMPDIR}/. ] ; then
	    echo "ERROR: unable to write to ${TMPDIR} for creating the package"
	    exit 1
	fi

	cd ${TMPDIR}

	if [ -d "$PKG" ] ; then
	    if [ -w "$PKG" ] ; then
		exists_writeable=yes
	    fi
	else
	    mkdir "$PKG"
	    if [ ! -d "$PKG" ] ; then
		echo "ERROR: unable to use ${TMPDIR} for creating the package"
		exit 1
	    fi

	    rmdir "$PKG"
	    if [ ! "x$?" = "x0" ] ; then
		echo "ERROR: unexpected failure while testing removal of $PKG"
		exit 1
	    fi
	fi
	
	if [ ! -d "${RESOURCES}" ] ; then
	    if [ -d "${PRE_PWD}/${RESOURCES}" ] ; then
		RESOURCES="${PRE_PWD}/${RESOURCES}"
	    fi
	fi
    else
	rmdir "$PKG"
	if [ ! "x$?" = "x0" ] ; then
	    echo "ERROR: unexpected failure while testing removal of $PKG"
	    exit 1
	fi
    fi
fi

if [ "x$exists_writeable" = "xyes" ] ; then
    remove=""
    while [ "x$remove" = "x" ] ; do
	echo "WARNING: Installer package ($PKG) already exists in `pwd`, remove it?"
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
	rm -rf "$PKG"
    fi
fi

if [ -f "$PKG" ] ; then
    echo "ERROR: cannot continue with $PKG in the way"
    exit 1
fi
if [ -d "$PKG" ] ; then
    echo "ERROR: cannot continue with directory $PKG in the way"
    exit 1
fi
    
mkdir "$PKG" > /dev/null 2>&1
if [ ! -d "$PKG" ] ; then
    echo "ERROR: unable to create the package directory"
    exit 1
fi

mkdir "$PKG/Contents"
if [ ! -d "$PKG/Contents" ] ; then
    echo "ERROR: unable to create the package contents directory"
    exit 1
fi

mkdir "$PKG/Contents/Resources"
if [ ! -d "$PKG/Contents/Resources" ] ; then
    echo "ERROR: unable to create the package resources directory"
    exit 1
fi

if [ ! "x$RESOURCES" = "xnone>>make_pkg_sh<<none" ] ; then
    if [ ! -d "$RESOURCES" ] ; then
	echo "ERROR: sanity check failure -- resources directory disappeared?"
	exit 1
    fi
    
    cp -R "${RESOURCES}/" "$PKG/Contents/Resources"
    if [ $? != 0 ] ; then
	echo "ERROR: unable to copy the resource directory contents"
	exit 1
    fi
fi

cat > "$PKG/Contents/PkgInfo" <<EOF
pmkrpkg1
EOF
if [ ! -f "$PKG/Contents/PkgInfo" ] ; then
    echo "ERROR: unable to create PkgInfo file"
    exit 1
fi

cat > "$PKG/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>CFBundleGetInfoString</key>
        <string>${NAME} ${VERSION}</string>
        <key>CFBundleIdentifier</key>
        <string>org.brlcad.${NAME}</string>
        <key>CFBundleName</key>
        <string>${NAME}</string>
        <key>CFBundleShortVersionString</key>
        <string>${MAJOR_VERSION}.${MINOR_VERSION}</string>
        <key>IFMajorVersion</key>
        <integer>${MAJOR_VERSION}</integer>
        <key>IFMinorVersion</key>
        <integer>${MINOR_VERSION}</integer>
        <key>IFPkgFlagAllowBackRev</key>
        <true/>
        <key>IFPkgFlagAuthorizationAction</key>
        <string>RootAuthorization</string>
        <key>IFPkgFlagDefaultLocation</key>
        <string>/</string>
        <key>IFPkgFlagInstallFat</key>
        <false/>
        <key>IFPkgFlagIsRequired</key>
        <true/>
        <key>IFPkgFlagRelocatable</key>
        <false/>
        <key>IFPkgFlagRestartAction</key>
        <string>NoRestart</string>
        <key>IFPkgFlagRootVolumeOnly</key>
        <false/>
        <key>IFPkgFlagUpdateInstalledLanguages</key>
        <false/>
        <key>IFPkgFormatVersion</key>
        <real>0.10000000149011612</real>
</dict>
</plist>
EOF
if [ ! -f "$PKG/Contents/Info.plist" ] ; then
    echo "ERROR: unable to create Info.plist file"
    exit 1
fi

mkdir "$PKG/Contents/Root"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create the archive root"
    exit 1
fi
if [ ! -d "$PKG/Contents/Root" ] ; then
    echo "ERROR: $PKG/Contents/Root could not be created"
    exit 1
fi

chmod 1775 "$PKG/Contents/Root"
if [ $? != 0 ] ; then
    echo "ERROR: unable to set the mode on the archive root"
    exit 1
fi

chown root:admin "$PKG/Contents/Root"
if [ $? != 0 ] ; then
    echo "ERROR: unable to set the owner/group on the archive root"
    exit 1
fi

pax -L -p e -rw "$ARCHIVE" "$PKG/Contents/Root"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create the archive root of $ARCHIVE"
    exit 1
fi

pax -z -w -x cpio -s ",$PKG/Contents/Root,.," "$PKG/Contents/Root" > "$PKG/Contents/Archive.pax.gz"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create a compressed pax archive"
    exit 1
fi
if [ ! -f "$PKG/Contents/Archive.pax.gz" ] ; then
    echo "ERROR: compressed pax archive does not exist"
    exit 1
fi

mkbom "$PKG/Contents/Root" "$PKG/Contents/Archive.bom"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully generate a bill of materials"
    exit 1
fi
if [ ! -f "$PKG/Contents/Archive.bom" ] ; then
    echo "ERROR: bill of materials file does not exist"
    exit 1
fi

rm -rf "$PKG/Contents/Root"
if [ -d "$PKG/Contents/Root" ] ; then
    echo "ERROR: unable to remove temporary BOM root"
    exit 1
fi

NUM_FILES=`find -L "${ARCHIVE}" -type f | wc | awk '{print $1}'`
if [ "x$NUM_FILES" = "x" ] ; then
    echo "ERROR: unable to get a file count from $ARCHIVE"
    exit 1
fi
if [ "x$NUM_FILES" = "x0" ] ; then
    echo "ERROR: there are no files to archive"
    exit 1
fi

INST_SIZE=`du -L -k -s "${ARCHIVE}" | awk '{print $1}'`
if [ "x$INST_SIZE" = "x" ] ; then
    echo "ERROR: unable to get a usage size from $ARCHIVE"
    exit 1
fi
if [ "x$INST_SIZE" = "x0" ] ; then
    echo "ERROR: install size is empty"
    exit 1
fi

COMP_SIZE=`ls -l "$PKG/Contents/Archive.pax.gz" | awk '{print $5}'`
COMP_SIZE=`echo "$COMP_SIZE 1024 / p" | dc`
if [ "x$COMP_SIZE" = "x" ] ; then
    echo "ERROR: unable to get the compressed archive size"
    exit 1
fi
if [ "x$COMP_SIZE" = "x0" ] ; then
    echo "ERROR: compressed archive is empty"
    exit 1
fi

cat > "$PKG/Contents/Resources/${PKG_NAME}.sizes" <<EOF
NumFiles $NUM_FILES
InstalledSize $INST_SIZE
CompressedSize $COMP_SIZE
EOF
if [ ! -f "$PKG/Contents/Resources/${PKG_NAME}.sizes" ] ; then
    echo "ERROR: unable to create the ${PKG_NAME}.size file"
    exit 1
fi

cat > "$PKG/Contents/Resources/${PKG_NAME}.info" <<EOF
Title $NAME
Version $VERSION
Description $NAME $VERSION
DefaultLocation /
DeleteWarning Don't do it... untested.

### Package Flags

NeedsAuthorization YES
Required YES
Relocatable NO
RequiresReboot NO
UseUserMask YES
OverwritePermissions NO
InstallFat NO
RootVolumeOnly NO
EOF
if [ ! -f "$PKG/Contents/Resources/${PKG_NAME}.info" ] ; then
    echo "ERROR: unable to create the ${PKG_NAME}.info file"
    exit 1
fi

cat > "$PKG/Contents/Resources/Description.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple Computer//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
        <key>IFPkgDescriptionDeleteWarning</key>
        <string>Don't do it... untested.</string>
        <key>IFPkgDescriptionDescription</key>
        <string>${NAME} ${VERSION}</string>
        <key>IFPkgDescriptionTitle</key>
        <string>${NAME}</string>
        <key>IFPkgDescriptionVersion</key>
        <string>${VERSION}</string>
</dict>
</plist>
EOF
if [ ! -f "$PKG/Contents/Resources/Description.plist" ] ; then
    echo "ERROR: unable to create the Description.plist file"
    exit 1
fi

ln -s ../Archive.bom "$PKG/Contents/Resources/${PKG_NAME}.bom"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create a symbolic link to the Archive.bom"
    exit 1
fi
if [ ! -h "$PKG/Contents/Resources/${PKG_NAME}.bom" ] ; then
    echo "ERROR: symbolic link ${PKG_NAME}.bom does not exist"
    exit 1
fi

ln -s ../Archive.pax.gz "$PKG/Contents/Resources/${PKG_NAME}.pax.gz"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create a symbolic link to the Archive.pax.gz"
    exit 1
fi
if [ ! -h "$PKG/Contents/Resources/${PKG_NAME}.pax.gz" ] ; then
    echo "ERROR: symbolic link ${PKG_NAME}.pax.gz does not exist"
    exit 1
fi

cd "$PRE_PWD"
# woo hoo .. done

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
