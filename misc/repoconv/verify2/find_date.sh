#!/bin/bash
REV=$1
rm -f files.txt shas.txt
svn diff -c$1 file:///home/user/brlcad_repo/brlcad|grep ^Index| awk '{print $2}'|sed -e 's/\// /'|awk '{print $2}' > files.txt
LOGDATE=$(svn log -c$1 file:///home/cyapp/REPO_CONVERT_FINAL/brlcad_repo/brlcad | tail +2|head -n 1|awk -F'|' '{print $3}')
echo $LOGDATE
LOGYEAR=$(echo $LOGDATE | awk -F'-' '{print $1'})
LOGMONTH=$(echo $LOGDATE | awk -F'-' '{print $2'})
LOGDAY=$(echo $LOGDATE | awk -F'-' '{print $3'} | awk '{print $1}')
LOGDAY_PREV=$(expr $LOGDAY - 1)
while read p; do
	git log --pretty=format:"%H;%s" --follow --since "$LOGYEAR-$LOGMONTH-$LOGDAY_PREV" --until "$LOGYEAR-$LOGMONTH-$LOGDAY" --full-history -- $p > logs
	echo "" >> logs
	cat logs
	while read s; do
		SHA1=$(echo $s | awk -F';' '{print $1}')
		MSG=$(echo "$s" | awk -F';' '{print $2}')
		if [ "$MSG" == "*** empty log message ***" ]
		then
			echo "$SHA1;$REV" >> full.txt
		fi
	done < logs
done < files.txt
cat full.txt |sort |uniq > shas.txt
rm full.txt files.txt
while read p; do
	SHA1=$(echo $p | awk -F';' '{print $1}')
	git log -1 --pretty=format:"%H;%s" $SHA1
done < shas.txt
cat shas.txt
