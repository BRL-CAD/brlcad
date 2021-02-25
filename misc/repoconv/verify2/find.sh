#!/bin/bash
REV=$1
rm -f files.txt shas.txt
svn diff -c$1 file:///home/user/brlcad_repo/brlcad|grep ^Index| awk '{print $2}'|sed -e 's/\// /'|awk '{print $2}' > files.txt
LOGMSG=$(svn log -c$1 file:///home/user/brlcad_repo/brlcad |tail +4|head -n 1)
while read p; do
	git log --pretty=format:"%H;%s" --follow --full-history -- $p > logs
	grep "$LOGMSG" logs | awk -F';' '{print $1}' > rsha1s.txt
	while read s; do
		echo "$s;$REV" >> full.txt
	done < rsha1s.txt
	rm rsha1s.txt logs
done < files.txt
cat full.txt |sort |uniq > shas.txt
rm full.txt files.txt
while read p; do
	SHA1=$(echo $p | awk -F';' '{print $1}')
	git log -1 --pretty=format:"%H;%s" $SHA1
done < shas.txt
cat shas.txt
