#!/bin/bash
rm -f git_svn_map.txt
git log --all --pretty=format:"%H" > sha1.txt
while read i; do
	SHA1=$i
	REV=$(git log -1 $SHA1 --pretty=format"%B" |grep svn:revision:|awk -F':' '{print $3}')
	echo "$SHA1;$REV" >> git_svn_map.txt
done < sha1.txt

