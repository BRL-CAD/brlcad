#!/bin/sh
#                  M A S T E R _ P R E P . S H
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
	echo $1 is not a directory
    fi
fi

export MYNAME=`hostname | awk -F '.' '{print $1}'`
export START_TIME=`date "+%y%m%d%H%M"`
export LOG_FILE=`pwd`/${MYNAME}_prep_${START_TIME}.log

# Process the checkout we just got
cd brlcad

# extract New release numbers from README
eval `awk '/Release/ {print $2}' < README | \
	awk -F. '{print "export MAJOR=" $1  "\nexport MINOR=" $2 "\nexport PATCH=" $3}'`


# Update configure.ac with the release we found in README
echo configure
if [ ! -f configure.ac.orig ] ; then
    mv configure.ac configure.ac.orig
fi

# Extract old release numbers from configure.ac.orig
OLD_MAJOR=`awk -F\= '/^MAJOR_VERSION/ {print $2}' < configure.ac.orig`
OLD_MINOR=`awk -F\= '/^MINOR_VERSION/ {print $2}' < configure.ac.orig`
OLD_PATCH=`awk -F\= '/^PATCH_VERSION/ {print $2}' < configure.ac.orig`


sed -e "s/$OLD_MAJOR\.$OLD_MINOR\.$OLD_PATCH/$MAJOR\.$MINOR\.$PATCH/" \
    -e "s/MAJOR_VERSION=$OLD_MAJOR/MAJOR_VERSION=$MAJOR/" \
    -e "s/MINOR_VERSION=$OLD_MINOR/MINOR_VERSION=$MINOR/" \
    -e "s/^PATCH_VERSION=$OLD_PATCH/PATCH_VERSION=$PATCH/" \
    < configure.ac.orig > configure.ac


# check to make sure it worked.
NEW_MAJOR=`grep '^MAJOR_VERSION=' configure.ac | awk -F \= '{print $2}'`
NEW_MINOR=`grep '^MINOR_VERSION=' configure.ac | awk -F \= '{print $2}'`
NEW_PATCH=`grep '^PATCH_VERSION=' configure.ac | awk -F \= '{print $2}'`

echo "checkout configure.ac version $OLD_MAJOR $OLD_MINOR $OLD_PATCH"
echo "Version from README           $MAJOR $MINOR $PATCH"
echo "New configure version         $NEW_MAJOR $NEW_MINOR $NEW_PATCH"

if [ X$NEW_PATCH != X$PATCH -o \
      X$NEW_MINOR != X$MINOR -o \
      X$NEW_MAJOR != X$MAJOR ] ; then
    echo "did not set new version number properly"
    exit -1
fi

# get a build environment so we can "make dist"
echo autogen
echo autogen >> $LOG_FILE 2>&1
sh ./autogen.sh >> $LOG_FILE 2>&1

echo configure
./configure --enable-everything >> $LOG_FILE 2>&1

# Prepare a source distribution
echo making dist
make dist >> $LOG_FILE 2>&1
cd ..

if [ ! -d hosts ] ; then
    mkdir hosts
fi

cd hosts

echo $MAJOR $MINOR $PATCH
# create the source tree that all machine will build from
tar xzf ../brlcad/brlcad-$MAJOR.$MINOR.$PATCH.tar.gz

# Let the other regression hosts start doing their work
echo semaphores
HOSTS="wopr liu amdws2 vast cocoa"
for i in $HOSTS ; do
    echo $MAJOR.$MINOR.$PATCH > $i
done

# Local Variables:
# tab-width: 8
# mode: sh
# sh-indentation: 4
# sh-basic-offset: 4
# indent-tabs-mode: t
# End:
# ex: shiftwidth=4 tabstop=8
