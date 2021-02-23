#!/bin/bash
rm -f sha1s_orig ukeys nkeys mapped_keys
while read p; do
  SHA1=$(echo "$p" | awk -F';' '{print $1}')
  if [ "$SHA1" != "" ]
  then
	  echo $SHA1 >> sha1s_orig

	  TSMP=$(git log -1 --pretty=format:"%ct%n" $SHA1)

	  # https://stackoverflow.com/a/7132381
	  git branch -a --contains $SHA1 | grep -v remote | sort | uniq |sed -e 's/\*//'|sed -e 's/ //g' > branchtmp
	  BMD5=$(md5sum branchtmp |awk '{print $1}')
	  rm branchtmp

	  git diff --raw $SHA1^ $SHA1 > keytmp
	  DIFFKEY=$(md5sum keytmp |awk '{print $1}')
	  rm keytmp

	  echo "SHA1: $SHA1"
	  echo "Timestamp: $TSMP"
	  echo "Branches MD5: $BMD5"
	  echo "Diff MD5: $DIFFKEY"
	  echo "$TSMP;$BMD5;$DIFFKEY" >> ukeys
  fi
done < $1

echo ""
echo "Remapping:"
echo ""

while read p; do
  TSMP=$(echo "$p" | awk -F';' '{print $1}')
  BMD5=$(echo "$p" | awk -F';' '{print $2}')
  DIFFKEY=$(echo "$p" | awk -F';' '{print $3}')
  echo "Searching for: $TSMP   $BMD5   $DIFFKEY"
  git log --all --since $TSMP --until $TSMP --pretty=format:"%H" > nkeys
  echo "" >> nkeys
  echo "nkeys:"
  cat nkeys
  echo ""
  while read d; do
	  CSHA1=$d
	  echo "	CSHA1: $CSHA1"

	  git branch -a --contains $CSHA1 | grep -v remote | sort | uniq |sed -e 's/\*//'|sed -e 's/ //g' > branchtmp
	  NBMD5=$(md5sum branchtmp |awk '{print $1}')
	  echo "	NBMD5: $NBMD5"
	  rm branchtmp

	  if [ "$BMD5" == "$NBMD5" ]
	  then
		  git diff --raw $CSHA1^ $CSHA1 > keytmp
		  NDIFFKEY=$(md5sum keytmp |awk '{print $1}')
		  echo "	NDIFFKEY: $NDIFFKEY"
		  if [ "$NDIFFKEY" == "$DIFFKEY" ]
		  then
			  echo "	MATCH"
			  echo $d >> mapped_keys
		  fi
		  rm keytmp
	  fi
  done < nkeys
  rm nkeys
done < ukeys
