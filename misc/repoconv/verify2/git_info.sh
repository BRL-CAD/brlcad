#!/bin/bash
rm -f git_diffs.txt git_logs.txt
git log --all --pretty=format:"%H" > sha1.txt
while read i; do
	SHA1=$i
	LOGMSGMD5=$(git log -1 --pretty=format:"%B" $SHA1 |grep -v '^cvs:' | grep -v '^svn:' | perl -0777 -pe 's/[-[:space:]]*//g' | md5sum| awk '{print $1}')
	DIFFMD5=$(git diff $SHA1^! ':(exclude).gitignore' |grep "^[-+].*"|grep -v "[-][-][-]" | grep -v "+++" | grep -v "^[-]$"|grep -v "^+$" |md5sum | awk '{print $1}')
	echo "$DIFFMD5;$SHA1" >> git_diffs.txt
	echo "$LOGMSGMD5;$SHA1" >> git_logs.txt
done < sha1.txt

