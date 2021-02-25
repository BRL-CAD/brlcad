#!/bin/bash
rm -f svn_logs.txt
SVN_REPO=/home/user/brlcad_repo/
svn log file://$SVN_REPO | grep '^r[0-9]'| awk '{print $1}' | awk -F',' '{print $1}' | awk -F'-' '{print $1}' | sed 's/^r//g' | sort -n |uniq > revs.txt
while read i; do
	REV=$i
	echo $REV
	LOGMSGMD5=$(svn log -c$REV file://$SVN_REPO | tail +4 | perl -0777 -pe 's/[-[:space:]]*//g' | md5sum| awk '{print $1}')
	echo "$LOGMSGMD5;$REV" >> svn_logs.txt
done < revs.txt
