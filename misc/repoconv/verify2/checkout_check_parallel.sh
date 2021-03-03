#!/bin/bash
CWD=$(pwd)

SVNREPO=/home/user/repo_dercs
GITREPO=/home/user/brlcad_conv16
REV=$1

rm -rf r$REV g$REV

cd $CWD
git -c advice.detachedHead=false clone $GITREPO g$REV && cd g$REV
BRANCH=$(git log --all --grep="^svn:revision:$REV$" --pretty=format:"%B" |grep :branch:|awk -F':' '{print $3}'|uniq)
if [ "$BRANCH" != "" ]
then
	SHA1=$(git log --all --grep="^svn:revision:$REV$" --pretty=format:"%H" |head -n 1)
	if [ "$SHA1" != "" ]
	then
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
		find g$REV/ -empty -delete
		find $BRANCHDIR/ -empty -delete
		DIFFSTATUS=$(diff --no-dereference -qrw -I '$Id' -I '$Revision' -I'$Header' -I'$Sour  ce' -I'$Date' -I'$Log' -I'$Locker' --exclude ".cvsignore" --exclude ".gitignore" --exclude "terra.dsp" --exclude   ".git" --exclude ".svn" --exclude "saxon65.jar" --exclude "xalan27.jar" --exclude "cup.g" --exclude "Cup.g" --exclude "xm4.asc" --exclude "dde1.1" --exclude "reg1.0" --exclude "libitcl" --exclude "cvs2cl.pl" --exclude "mug.jpg" --exclude "alias-pix.txt" g$REV $BRANCHDIR > r$REV.diff)
		if [ -s r$REV.diff ]
		then
			echo "$REV: difference found"
		else
			rm r$REV.diff
		fi
		rm -rf r$REV
	else
		cd $CWD
	fi
else
	cd $CWD
fi
rm -rf g$REV
