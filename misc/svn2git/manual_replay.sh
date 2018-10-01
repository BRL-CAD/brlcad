#!/bin/bash

# References:
# svn + patch:  https://stackoverflow.com/a/659486
# executing commands: https://stackoverflow.com/a/4651495
# bash iterating: https://stackoverflow.com/q/169511
# bash functions: http://tldp.org/LDP/abs/html/complexfunct.html

SVN_DIR="$1"
GIT_DIR="$2"
ACCNT_MAP="$3"
SVN_TMP=/home/user/svn_tmp/
mkdir ${SVN_TMP}

svn_process () {
	if [ -z "$1" ]
	then
		echo "Call to svn_process with no revision"
		exit 1
	fi
	if [ -z "$2" ]
	then
		echo "Call to svn_process with no strip count for patch"
		exit 1
	fi
	svn diff -c$1 ${SVN_DIR} | patch -f --remove-empty-files -d ${GIT_DIR} -p$2
	COMMIT_DATE="$(svn propget --revprop -r $1 svn:date ${SVN_DIR})"
	COMMIT_AUTHOR="$(svn propget --revprop -r $1 svn:author ${SVN_DIR})"
	if [ -z "$3" ]
	then
		COMMIT_MSG="$(svn propget --revprop -r $1 svn:log ${SVN_DIR})"
	else
		COMMIT_MSG="$3"
	fi
	echo "Commit $1:"
	cd ${GIT_DIR}
	if [[ `git status --porcelain` ]]; then
		if [[ -z "${COMMIT_AUTHOR// }" ]]
		then
			GIT_AUTHOR="Unknown <unknown@unknown>"
		else
			GIT_AUTHOR="$(grep -e "${COMMIT_AUTHOR} " ${ACCNT_MAP} | cut -d' ' -f 2-10)"
		fi
		echo "${GIT_AUTHOR}"
		git add -A
		git commit -m "${COMMIT_MSG}" --author="${GIT_AUTHOR}" --date=${COMMIT_DATE}
		# https://git-scm.com/docs/git-notes
		if [ -z "$4" ]
		then
			git notes add -m "svn:revision:$1 svn:author:${COMMIT_AUTHOR}"
		else
			git notes add -m "svn:revision:$1 svn:author:${COMMIT_AUTHOR} svn:branch:$4"
		fi
	else
		echo "*******************************"
		echo "WARNING - commit $1 is a no op!"
		echo "${COMMIT_DATE}"
		echo "${COMMIT_MSG}"
		echo "${COMMIT_AUTHOR}"
		echo "*******************************"
	fi
	return 0
}

if [ ]; then
#if : ; then
# Get Git repo initialized
mkdir ${GIT_DIR}
cd ${GIT_DIR}
git init


# Commit 1 is a no-op - cvs2svn repo initialization

svn_process 2 2
svn_process 3 2

# Commit 4 made the branch "unlabeled-1.2.1" - make a
# jove 1.2.1 branch in git instead
cd ${GIT_DIR}
git branch jove-1.2.1
git checkout jove-1.2.1
git rm -r *
svn_process 4 3 "Create jove-1.2.1 branch (cvs2svn auto created)" unlabeled-1.2.1
cd ${GIT_DIR}
git checkout master

for i in {5..8}; do
	svn_process $i 2
done

# Commit 9 occurs in "unlabeled-1.2.1" - use jove-1.2.1 instead
cd ${GIT_DIR}
git checkout jove-1.2.1
svn_process 9 3
cd ${GIT_DIR}
git checkout master


svn_process 10 2

# The actual first BRL-CAD commit
svn_process 11 2

# Commit 12 creates the tag rt, which according to the commit message was
# manufactured by cvs2svn (why??).  Is it intended to mark the start of
# actual BRL-CAD code development?  Use a lightweight tag since we have
# no info.
cd ${GIT_DIR}
git tag rt

for i in {13..395}; do
	svn_process $i 2
done

# Commit 396 creates the tag rt-2, which according to the commit message was
# manufactured by cvs2svn (why??).  Since it has no useful information, in this
# particular case use a lightweight git tag.
cd ${GIT_DIR}
git tag rt-2


for i in {397..582}; do
	svn_process $i 2
done

# Commit 583 made the branch "unlabeled-2.5.1" - make a
# proc_reg branch in git instead
cd ${GIT_DIR}
git branch proc_reg
git checkout proc_reg
git rm -r *
svn_process 583 3 "Create proc_reg branch (cvs2svn auto created)" unlabeled-2.5.1
cd ${GIT_DIR}
git checkout master

for i in {584..817}; do
	svn_process $i 2
done

# Commit 818 made the branch "unlabeled-2.12.1" - make a
# vdeck-1 branch in git instead
cd ${GIT_DIR}
git branch vdeck-1
git checkout vdeck-1
git rm -r *
svn_process 818 3 "Create vdeck-1 branch (cvs2svn auto created)" unlabeled-2.12.1
cd ${GIT_DIR}
git checkout master

for i in {819..845}; do
	svn_process $i 2
done


# Commit 846 made the branch "unlabeled-2.6.1" - make a
# vdeck-2 branch in git instead
cd ${GIT_DIR}
git branch vdeck-2
git checkout vdeck-2
git rm -r *
svn_process 846 3 "Create vdeck-2 branch (cvs2svn auto created)" unlabeled-2.6.1
cd ${GIT_DIR}
git checkout master

svn_process 847 2
svn_process 848 2

fi

if [ ]; then
#if : ; then
# Commit 849 is a problem - it is spread across 3 branches.
cd ${SVN_TMP}
rm -rf ${SVN_TMP}/*
svn co -r849 ${SVN_DIR}/brlcad/branches
cd branches/unlabeled-2.12.1
svn diff -c849 > ../../patch-2.12.1
cd ../../branches/unlabeled-2.6.1
svn diff -c849 > ../../patch-2.6.1
cd ../../
rm -rf branches
svn co -r849 ${SVN_DIR}/brlcad/trunk
cd trunk
svn diff -c849 > ../patch-trunk
COMMIT_DATE="$(svn propget --revprop -r 849 svn:date ${SVN_DIR})"
COMMIT_AUTHOR="$(svn propget --revprop -r 849 svn:author ${SVN_DIR})"
COMMIT_MSG="$(svn propget --revprop -r 849 svn:log ${SVN_DIR})"
cd ${GIT_DIR}
git checkout vdeck-1
patch -f --remove-empty-files -d ${GIT_DIR} -p0 < ${SVN_TMP}/patch-2.12.1
git add -A
git commit -m "${COMMIT_MSG}" --author="${GIT_AUTHOR}" --date=${COMMIT_DATE}
git notes add -m "svn:revision:849 svn:author:${COMMIT_AUTHOR}"
git checkout vdeck-2
patch -f --remove-empty-files -d ${GIT_DIR} -p0 < ${SVN_TMP}/patch-2.6.1
git add -A
git commit -m "${COMMIT_MSG}" --author="${GIT_AUTHOR}" --date=${COMMIT_DATE}
git notes add -m "svn:revision:849 svn:author:${COMMIT_AUTHOR}"
git checkout master
patch -f --remove-empty-files -d ${GIT_DIR} -p0 < ${SVN_TMP}/patch-trunk
git add -A
git commit -m "${COMMIT_MSG}" --author="${GIT_AUTHOR}" --date=${COMMIT_DATE}
git notes add -m "svn:revision:849 svn:author:${COMMIT_AUTHOR}"

fi

if [ ]; then
#if : ; then

rm -rf ${SVN_TMP}/*

for i in {850..878}; do
	svn_process $i 2
done

# Commit 879 was made in the branch "unlabeled-2.5.1"
cd ${GIT_DIR}
git checkout proc_reg
svn_process 879 3
cd ${GIT_DIR}
git checkout master

for i in {880..943}; do
	svn_process $i 2
done

# 944 only sets cvs2svn properties - no op in git

for i in {945..1389}; do
	svn_process $i 2
done

fi

#if [ ]; then
if : ; then

# 1390 claims to be the BRL CAD Distribution Release 1.10, but there are
# multiple point in the repository that indicate this.  Make a branch,
# not a tag.
cd ${GIT_DIR}
git branch rel-1-10
git checkout rel-1-10

for i in {1391..1395}; do
	svn_process $i 2
done
cd ${GIT_DIR}
git checkout master
git merge rel-1-10
git branch rel-1-12
git checkout rel-1-12
for i in {1396..1410}; do
	svn_process $i 2
done
git checkout master
git merge rel-1-12
git branch rel-1-15
git checkout rel-1-15
for i in {1411..1415}; do
	svn_process $i 2
done
git checkout master
git merge rel-1-15
git branch rel-1-16
git checkout rel-1-16
for i in {1416..1428}; do
	svn_process $i 2
done
git checkout master
git merge rel-1-16
git branch rel-1-18
git checkout rel-1-18
for i in {1429..1437}; do
	svn_process $i 2
done
git checkout master
git merge rel-1-18
git branch rel-1-20
for i in {1438..1443}; do
	svn_process $i 2
done
git checkout master
git merge rel-1-20
GIT_COMMITTER_DATE="1987-02-13T06:41:12.000000Z" git tag -a -m "Actual 1.20 distribution version." rel-1-20
fi

if [ ] ; then
#if : ; then
for i in {1444..1560}; do
	svn_process $i 2
done
git branch rel-2-3
for i in {1561..1860}; do
	svn_process $i 2
done
git branch rel-3-0


if [ ] ; then
#if : ; then
for i in {2104..2105}; do
	svn_process $i 2
done
fi
