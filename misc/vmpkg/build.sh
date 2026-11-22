#!/bin/sh
#                     B U I L D . S H
# BRL-CAD
#
# Copyright (c) 2026 United States Government as represented by
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
. /opt/rh/gcc-toolset-13/enable
set -u
PATH=/opt/vmpkg-tools/bin:/opt/vmpkg-tools/cmake/bin:$PATH
export PATH
CC=/opt/rh/gcc-toolset-13/root/usr/bin/gcc
CXX=/opt/rh/gcc-toolset-13/root/usr/bin/g++
export CC CXX
# Preserve the x86-64 baseline even when the VM exposes its host CPU features.
CFLAGS="${CFLAGS:-} -march=x86-64 -mtune=generic"
CXXFLAGS="${CXXFLAGS:-} -march=x86-64 -mtune=generic"
export CFLAGS CXXFLAGS
driver=guest-build.py
if [ "${1:-}" = check ]; then
    driver=guest-check.py
    shift
fi
exec /usr/bin/python3.11 "$(dirname "$0")/$driver" "$@"
