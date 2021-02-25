#!/bin/bash
REV=$1
rm -f files.txt shas.txt
svn diff -c$1 file:///home/user/brlcad_repo/brlcad|grep ^Index| awk '{print $2}'|sed -e 's/\// /'|awk '{print $2}' > files.txt
while read p; do
	echo "$p"
	SHA1=$(git log --follow --full-history --reverse -- $p | head -n 1|awk '{print $2}')
	echo "$SHA1;$REV" >> full.txt
done < files.txt
cat full.txt |sort|uniq > shas.txt
rm full.txt
cat shas.txt
