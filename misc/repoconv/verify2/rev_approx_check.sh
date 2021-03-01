#!/bin/bash
  
# Note - ssdeep needs files >=4k.  For smaller files, we just use https://superuser.com/a/459121
# to get a sense of shared lines, unless/until something better turns up...

CWD=$(pwd)

if [ "$#" -ne "3" ]
then
        echo "rev_deep_check SVN_REPO GIT_REPO input_file"
        exit 1
fi
SVNREPO=$1 
GITREPO=$2
input_file=$CWD/$3

if [ ! -d $SVNREPO ]
then
        echo "$SVNREPO does not exist"
        exit 1
fi
if [ ! -d $GITREPO ]
then 
        echo "$GITREPO does not exist"
        exit 1
fi
if [ ! -f $input_file ]
then
        input_file=$3
        if [ ! -f $input_file ]
        then
                echo "$input_file does not exist"
                exit 1
        fi
fi

cat $input_file |sort -n > intmp
mv intmp $input_file
while read i; do
	REV=$i
	svn log -c$REV file://$SVNREPO
	cd $GITREPO
	git log --all --grep="^svn:revision:$REV$" --pretty=format:"%H" > rev_shas.txt
	echo "" >> rev_shas.txt
	rm -f diffcontents.txt
	while read i; do
		SHA1=$i
		if [ "$SHA1" != "" ]
		then
			git diff $SHA1^! ':(exclude).gitignore' |grep "^[-+].*"|grep -v "[-][-][-]" | grep -v "+++" | grep -v "^[-]$"|grep -v "^+$" >> diffcontents.txt
		fi
	done < rev_shas.txt
	sort diffcontents.txt > $CWD/gitdiff
	rm diffcontents.txt rev_shas.txt
	cd $CWD
	svn diff --ignore-properties -c$REV file://$SVNREPO |grep "^[-+].*"|grep -v "[-][-][-]" | grep -v "+++" |grep -v "^[-]$"|grep -v "^+$" |sort > svndiff
	if [ -s svndiff ]
	then
		if [ -s gitdiff ]
		then
			md5sum svndiff
			md5sum gitdiff
			ssdeep -b svndiff 2>/dev/null > svndiff.hash
			SCORE=$(ssdeep -bm svndiff.hash gitdiff 2>/dev/null|cut -c 37-)
			if [ "$SCORE" != "" ]
			then
				echo "Similarity score (100->0):$SCORE"
			fi
			cat svndiff | sort > svndiffsort
			cat gitdiff | sort > gitdiffsort
			mv svndiffsort svndiff
			mv gitdiffsort gitdiff
			comm -12 svndiff gitdiff > commonlines
			SVNWC=$(wc -c svndiff|awk '{print $1}')
			GITWC=$(wc -c gitdiff|awk '{print $1}')
			COMMWC=$(wc -c commonlines|awk '{print $1}')
			echo "SVN:Git:common line counts: $SVNWC:$GITWC:$COMMWC"
		else
			echo "DIFFERENCE: Non-empty SVN diff but empty git diff"
		fi
	else
		if [ -s gitdiff ]
		then
			echo "DIFFERENCE: Empty svn diff but non empty git diff"
		else
			echo "both diffs empty"
		fi
	fi
	echo ""
	echo ""
done < $input_file

