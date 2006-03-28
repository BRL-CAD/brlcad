#!/bin/sh

#
# If the user specified a working directory, we cd into it.
#
if [ $# = 1 ] ; then
    if [ -d $1 ] ; then
	cd $1
    else
	/bin/echo $1 is not a directory
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

# Extract old release numbers from configure.ac
OLD_MAJOR=`awk -F\= '/^MAJOR_VERSION/ {print $2}' < configure.ac`
OLD_MINOR=`awk -F\= '/^MINOR_VERSION/ {print $2}' < configure.ac`
OLD_PATCH=`awk -F\= '/^PATCH_VERSION/ {print $2}' < configure.ac`


# Update configure.ac with the release we found in README
/bin/echo configure
if [ ! -f configure.ac.orig ] ; then
    mv configure.ac configure.ac.orig
fi

sed -e "s/$OLD_MAJOR\.$OLD_MINOR\.$OLD_PATCH/$MAJOR\.$MINOR\.$PATCH/" \
    -e "s/MAJOR_VERSION=$OLD_MAJOR/MAJOR_VERSION=$MAJOR/" \
    -e "s/MINOR_VERSION=$OLD_MINOR/MINOR_VERSION=$MINOR/" \
    -e "s/^PATCH_VERSION=$OLD_PATCH/PATCH_VERSION=$PATCH/" \
    < configure.ac.orig > configure.ac


# check to make sure it worked.
NEW_MAJOR=`grep '^MAJOR_VERSION=' configure.ac | awk -F \= '{print $2}'`
NEW_MINOR=`grep '^MINOR_VERSION=' configure.ac | awk -F \= '{print $2}'`
NEW_PATCH=`grep '^PATCH_VERSION=' configure.ac | awk -F \= '{print $2}'`

/bin/echo "$OLD_MAJOR $OLD_MINOR $OLD_PATCH"
/bin/echo "$NEW_MAJOR $NEW_MINOR $NEW_PATCH"
/bin/echo "$MAJOR $MINOR $PATCH"

# get a build environment so we can "make dist"
/bin/echo autogen
/bin/echo autogen >> $LOG_FILE 2>&1
/bin/sh ./autogen.sh >> $LOG_FILE 2>&1

/bin/echo configure
./configure --enable-everything >> $LOG_FILE 2>&1

# Prepare a source distribution
/bin/echo making dist
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
/bin/echo semaphores
HOSTS="wopr liu amdws2 vast cocoa"
for i in $HOSTS ; do
    echo $MAJOR.$MINOR.$PATCH > $i
done
