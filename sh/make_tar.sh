#!/bin/sh
#                     M A K E _ T A R . S H
# BRL-CAD
#
# Copyright (c) 2005-2007 United States Government as represented by
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
# Script for generating an uncompressed tape archive binary
# distribution.
#
# Author: Christopher Sean Morrison
#
######################################################################

NAME="$1"
MAJOR_VERSION="$2"
MINOR_VERSION="$3"
PATCH_VERSION="$4"
ARCHIVE="$5"
if [ "x$NAME" = "x" ] ; then
    echo "Usage: $0 name major_version minor_version patch_version archive_dir"
    echo "ERROR: must specify a package name"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "Usage: $0 name major_version minor_version patch_version archive_dir"
    echo "ERROR: must specify a major package version"
    exit 1
fi
if [ "x$MINOR_VERSION" = "x" ] ; then
    echo "ERROR: must specify a minor package version"
    echo "Usage: $0 name major_version minor_version patch_version archive_dir"
    exit 1
fi
if [ "x$PATCH_VERSION" = "x" ] ; then
    echo "Usage: $0 name major_version minor_version patch_version archive_dir"
    echo "ERROR: must specify a patch package version"
    exit 1
fi
if [ "x$ARCHIVE" = "x" ] ; then
    echo "Usage: $0 name major_version minor_version patch_version archive_dir"
    echo "ERROR: must specify an archive directory"
    exit 1
fi
if [ ! -d "$ARCHIVE" ] ; then
    echo "ERROR: specified archive path (${ARCHIVE}) is not a directory"
    exit 1
fi

VERSION="${MAJOR_VERSION}.${MINOR_VERSION}.${PATCH_VERSION}"
PKG_NAME="${NAME}-${VERSION}"
TAR_NAME="${PKG_NAME}.bin.tar"

# check for gnu tar
tar --version > /dev/null 2>&1
if [ $? != 0 ] ; then
    # non-gnu tar
    TAR_OPTS=cvf
else
    # gnu tar (P for absolute paths)
    TAR_OPTS=cvPf
fi

tar $TAR_OPTS  "${TAR_NAME}" "${ARCHIVE}"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully create a tape archive of $ARCHIVE"
    exit 1
fi
if [ ! -f "${TAR_NAME}" ] ; then
    echo "ERROR: tape archive \"${TAR_NAME}\" does not exist"
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
