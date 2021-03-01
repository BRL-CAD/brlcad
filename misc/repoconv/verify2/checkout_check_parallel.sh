#!/bin/bash
CWD=$(pwd)

SVNREPO=/home/user/repo_dercs
GITREPO=/home/user/brlcad_conv16
REV=$1

rm -rf r$REV g$REV

cd $CWD
git -c advice.detachedHead=false clone $GITREPO g$REV && cd g$REV
BRANCH=$(git log --all --grep="^svn:revision:$REV$" --pretty=format:"%B" |grep :branch:|awk -F':' '{print $3}'|uniq)
SHA1=$(git log --all --grep="^svn:revision:$REV$" --pretty=format:"%H" |head -n 1)
echo "$REV -> $SHA1"
echo "BRANCH=$BRANCH"
git -c advice.detachedHead=false checkout $SHA1
cd $CWD
BRANCHDIR=""
if [ "$BRANCH" == "trunk" ]
then
	svn checkout -r$REV file://$SVNREPO/brlcad/trunk@$REV r$REV 1>/dev/null 2>/dev/null
	BRANCHDIR="r$REV"
else
	svn checkout -r$REV file://$SVNREPO/brlcad/branches/$BRANCH@$REV r$REV  1>/dev/null 2>/dev/null
	BRANCHDIR="r$REV/$BRANCH"
fi
DIFFSTATUS=$(diff --no-dereference -qrw -I '$Id' -I '$Revision' -I'$Header' -I'$Sour  ce' -I'$Date' -I'$Log' -I'$Locker' --exclude ".cvsignore" --exclude ".gitignore" --exclude "terra.dsp" --exclude   ".git" --exclude ".svn" --exclude "saxon65.jar" --exclude "xalan27.jar" g$REV $BRANCHDIR > r$REV.diff)
if [ -s r$REV.diff ]
then
	echo "$REV: difference found\n"
else
	rm r$REV.diff
fi
rm -rf r$REV
rm -rf g$REV

