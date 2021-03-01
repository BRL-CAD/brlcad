#!/bin/bash

rm -f revs.txt git_diffs.txt diffcontents.txt diffsorted.txt rev_shas.txt revs_raw.txt
git log --all |grep svn:revision|awk -F':' '{print $3}'|sort -n|uniq > revs_raw.txt
tail -n +2 revs_raw.txt > revs.txt
rm -f revs_raw.txt

while read i; do
	REV=$i
	echo "$REV:"
	git log --all --grep="^svn:revision:$REV$" --pretty=format:"%H" > rev_shas.txt
	echo "" >> rev_shas.txt
	rm -f diffcontents.txt
	while read i; do
	        SHA1=$i
	        echo "$SHA1"
	        git diff $SHA1^! ':(exclude).gitignore' |grep "^[-+].*"|grep -v "[-][-][-]" | grep -v "+++" | grep -v "^[-]$"|grep -v "^+$" >> diffcontents.txt
	done < rev_shas.txt
	sort diffcontents.txt > diffsorted.txt
	DIFFMD5=$(cat diffsorted.txt |md5sum | awk '{print $1}')
	echo "$DIFFMD5;$REV" >> git_diffs.txt
done < revs.txt

