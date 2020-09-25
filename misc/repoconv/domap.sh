#!/bin/bash

# To generate a map between the msg/time keys and sha1 values, run the
# following script from within the repository you wish to map to (i.e.
# the target repository to which the information in the original map
# is to be applied:

git log --all --pretty=format:"%H" > sha1s.txt

while IFS="" read -r p || [ -n "$p" ]
do
	MSGSHA1=$(git log -n1 --pretty=format:"%B" $p | sha1sum | head -c 40)
	CDATE=$(git log -n1 --pretty=format:"%ct" $p)
	echo $MSGSHA1$CDATE:$p >> ../msgtime_sha1_map
done < sha1s.txt

rm sha1s.txt
