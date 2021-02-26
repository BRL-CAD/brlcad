#!/bin/bash
rm -f git_only.txt
git log --all --pretty=format:"%H" > sha1.txt
while read i; do
	SHA1=$i
	REV=$(git log -1 $SHA1 --pretty=format:"%B" |grep svn:revision:|awk -F':' '{print $3}')
	if [ "$REV" == "" ]
	then
		SDEL=$(git log -1 $SHA1 --pretty=format:"%B" |grep "svn branch delete")
		PMV=$(git log -1 $SHA1 --pretty=format:"%B" |grep "preliminary file move commit")
		if [ "$SDEL" == "" ] && [ "$PMV" == "" ]
		then
			echo "$SHA1"
			echo "$SHA1" >> git_only_1.txt
		fi
	else
		echo "$REV"
	fi
done < sha1.txt

# First two commits are the sync commits - filter them out
tail -n +3 git_only_1.txt > git_only.txt
rm git_only_1.txt
