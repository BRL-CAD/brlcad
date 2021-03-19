#!/bin/sh
FNAME=$1
echo "FNAME=$FNAME"
rm -rf tkhtml
rm -rf tkhtml3
svn revert -R *
svn up .
rm -rf /home/user/github/brlcad/*
git -C /home/user/github/brlcad reset --hard
git -C /home/user/github/brlcad checkout main
git -C /home/user/github/brlcad log --pretty=format:"%H" --follow src/other/tkhtml/src/$FNAME > sha1.txt
echo "" >> sha1.txt
while read SHA1; do
	REV=$(git -C /home/user/github/brlcad log -1 --pretty=format:"%B" $SHA1|grep svn:revision:|awk -F':' '{print $3}')
	if [ "$REV" != "" ]
	then
		echo ""
		echo ""
		echo $SHA1
		git -C /home/user/github/brlcad checkout $SHA1 > /dev/null
		echo $REV
		echo $REV
		rm -rf tkhtml
		rm -rf tkhtml3
		svn revert -R *
		svn up .
		svn update -r$REV tkhtml > /dev/null
		svn update -r$REV tkhtml3 > /dev/null
		OFF=""
		NFF=""
		if [ -s /home/user/github/brlcad/src/other/tkhtml/src/$FNAME ]
		then
			OFF=/home/user/github/brlcad/src/other/tkhtml/src/$FNAME
		else
			if [ -s /home/user/github/brlcad/src/other/tkhtml3/src/$FNAME ]
			then
				OFF=/home/user/github/brlcad/src/other/tkhtml3/src/$FNAME
			fi
		fi
		if [ "$OFF" != "" ]
		then
			OSHA1=$(git hash-object $OFF)
		fi
		if [ -s /home/user/brlcad/src/other/tkhtml/src/$FNAME ]
		then
			NFF=/home/user/brlcad/src/other/tkhtml/src/$FNAME
		else
			if [ -s /home/user/brlcad/src/other/tkhtml3/src/$FNAME ]
			then
				NFF=/home/user/brlcad/src/other/tkhtml3/src/$FNAME
			fi
		fi
		if [ "$NFF" != "" ]
		then
			NSHA1=$(git hash-object $NFF)
			echo "$FNAME-$REV -> $NSHA1"
			cp $NFF $FNAME-$REV
			cp $NFF $FNAME-$REV.blob
			SB=$(stat -c %s $NFF)
			sed -i "1s;^;data $SB\n;" $FNAME-$REV.blob
			sed -i '1s;^;blob\n;' $FNAME-$REV.blob
			echo "$OSHA1;$NSHA1" >> map.txt
			echo "$OSHA1 -> $NSHA1" >> info.txt
			echo "$FNAME-$REV -> $NSHA1" >> info.txt
		else
			echo "$REV: nothing"
		fi
	fi
done < sha1.txt
git -C /home/user/github/brlcad checkout main
