#!/bin/bash
rm -f sha1s_orig ukeys nkeys mapped_keys
while read p; do
  SHA1=$(echo "$p" | awk -F';' '{print $1}')
  if [ "$SHA1" != "" ]
  then
	  echo "SHA1: $SHA1"
	  echo $SHA1 >> sha1s_orig
	  TSMP=$(git log -1 --pretty=format:"%ct%n" $SHA1)
	  echo "Timestamp: $TSMP"
	  git diff --raw $SHA1^ $SHA1 > keytmp
	  DIFFKEY=$(md5sum keytmp |awk '{print $1}')
	  rm keytmp
	  echo "Diffkey: $DIFFKEY"
	  echo "$TSMP;$DIFFKEY" >> ukeys
  fi
done < $1

echo ""
echo "Remapping:"
echo ""

while read p; do
  TSMP=$(echo "$p" | awk -F';' '{print $1}')
  DIFFKEY=$(echo "$p" | awk -F';' '{print $2}')
  echo "$p -> $TSMP   $DIFFKEY"
  git log --all --since $TSMP --until $TSMP --pretty=format:"%H" > nkeys
  echo "" >> nkeys
  echo "nkeys:"
  cat nkeys
  echo ""
  while read d; do
	  CSHA1=$d
	  echo "	CSHA1: $CSHA1"
	  git diff --raw $CSHA1^ $CSHA1 > keytmp
	  NDIFFKEY=$(md5sum keytmp |awk '{print $1}')
	  echo "	NDIFFKEY: $NDIFFKEY"
	  if [ "$NDIFFKEY" == "$DIFFKEY" ]
	  then
		  echo "	MATCH"
		  echo $d >> mapped_keys
	  fi
	  rm keytmp
  done < nkeys
  rm nkeys
done < ukeys
