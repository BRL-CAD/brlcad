#!/bin/bash
rm -f svn_diffs.txt
SVN_REPO=/home/user/brlcad_repo/
svn log file://$SVN_REPO | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq > revs.txt
while read i; do
	REV=$i
	echo $REV
	DIFFMD5=$(svn diff --ignore-properties -c$REV file://$SVN_REPO |grep "^[-+].*"|grep -v "[-][-][-]" | grep -v "+++" |grep -v "^[-]$"|grep -v "^+$" |md5sum | awk '{print $1}')
	echo "$DIFFMD5;$REV" >> svn_diffs.txt
done < revs.txt
