#!/bin/sh
#                     M A K E _ T G Z . S H
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
# Script for compressing a tape archive binary distribution via gzip
#
# Author: Christopher Sean Morrison
#
######################################################################

PKG_NAME="$1"
if [ "x$PKG_NAME" = "x" ] ; then
    echo "Usage: $0 package_name"
    echo "ERROR: must specify a package name"
    exit 1
fi

TAR_NAME="${PKG_NAME}.bin.tar"
if [ ! -f "${TAR_NAME}" ] ; then
    echo "ERROR: unable to locate tape archive $TAR_NAME"
    exit 1
fi
if [ ! -r "${TAR_NAME}" ] ; then
    echo "ERROR: unable to read tape archive $TAR_NAME"
    exit 1
fi

gzip "${TAR_NAME}"
if [ $? != 0 ] ; then
    echo "ERROR: unable to successfully gzip compress archive \"${TAR_NAME}\""
    exit 1
fi
if [ ! -f "${TAR_NAME}.gz" ] ; then
    echo "ERROR: compressed tape archive \"${TAR_NAME}.gz\" does not exist"
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
