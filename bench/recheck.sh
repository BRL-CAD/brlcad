#!/bin/sh
#                      R E C H E C K . S H
# BRL-CAD
#
# Copyright (c) 2004 United States Government as represented by the
# U.S. Army Research Laboratory.
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
# A Shell script to re-check the most recent results of the benchmark
#  @(#)$Header$ (BRL)

# Ensure /bin/sh
export PATH || (echo "This isn't sh."; sh $0 $*; kill $$)

eval `machinetype.sh -b`	# sets MACHINE, UNIXTYPE, HAS_TCP
if test -f ../.util.$MACHINE/pixdiff ; then
  PIXDIFF=../.util.$MACHINE/pixdiff
  PIX_FB=../.fb.$MACHINE/pix-fb
  LD_LIBRARY_PATH=../.libbu.$MACHINE:../.libbn.$MACHINE:../.librt.$MACHINE:../.libfb.$MACHINE:../.libpkg.$MACHINE:../.libsysv.$MACHINE:$LD_LIBRARY_PATH
else
  if test -f ../util/pixdiff ;  then
    PIXDIFF=../util/pixdiff
    PIX_FB=../fb/pix-fb
  else
    echo "Can't find pixdiff"
    exit 1
  fi
  LD_LIBRARY_PATH=../libbu:../libbn:../librt:../libfb:../libpkg:../libsysv:$LD_LIBRARY_PATH
fi
export LD_LIBRARY_PATH

# Alliant NFS hack
if test x${MACHINE} = xfx ; then
  cp ${PIXDIFF} /tmp/pixdiff
  cp ${PIX_FB} /tmp/pix-fb
  PIXDIFF=/tmp/pixdiff
  PIX_FB=/tmp/pix-fb
fi

echo +++++ moss.pix
${PIXDIFF} ../pix/moss.pix moss.pix | ${PIX_FB}

echo +++++ world.pix
${PIXDIFF} ../pix/world.pix world.pix  | ${PIX_FB}

echo +++++ star.pix
${PIXDIFF} ../pix/star.pix star.pix  | ${PIX_FB}

echo +++++ bldg391.pix
${PIXDIFF} ../pix/bldg391.pix bldg391.pix  | ${PIX_FB}

echo +++++ m35.pix
${PIXDIFF} ../pix/m35.pix m35.pix  | ${PIX_FB}

echo +++++ sphflake.pix
${PIXDIFF} ../pix/sphflake.pix sphflake.pix  | ${PIX_FB}

# Local Variables:
# mode: sh
# tab-width: 8
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
