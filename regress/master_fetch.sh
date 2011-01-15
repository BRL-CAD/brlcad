#!/bin/sh
#                 M A S T E R _ F E T C H . S H
# BRL-CAD
#
# Copyright (c) 2010-2011 United States Government as represented by
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
# If the user specified a working directory, we cd into it.
#
if [ $# = 1 ] ; then
    if [ -d $1 ] ; then
	cd $1
    else
	echo "$1 is not a directory"
    fi
fi

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_fetch_${START_TIME}.log

# Delete any leftovers
rm -rf $HOSTS brlcad

# Fetch a clean copy of the repository
echo "fetching archive" > $LOG_FILE 2>&1
CVS_RSH=ssh
export CVS_RSH


# cvs -z3 -d:pserver:anonymous@cvs.sourceforge.net:/cvsroot/brlcad co -P brlcad >> $LOG_FILE 2>&1

cvs -z3 -d:ext:lbutler@cvs.sourceforge.net:/cvsroot/brlcad co -P brlcad


if [ ! -d brlcad ] ; then
    echo "unable to extract source from CVS repository"
    exit 1
fi

#
# prepare the build
#
/bin/sh brlcad/regress/master_prep.sh

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
