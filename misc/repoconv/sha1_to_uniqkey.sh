#!/bin/bash

set -e

CWD=$(pwd)

if [ "$#" -ne "4" ]
then
	echo "sha1_update old_repo new_repo input_file output_file"
	exit 1
fi
old_repo=$1
new_repo=$2
input_file=$CWD/$3
output_file=$4

if [ ! -d $old_repo ]
then
	echo "$old_repo does not exist"
	exit 1
fi
if [ ! -d $new_repo ]
then
	echo "$new_repo does not exist"
	exit 1
fi
if [ ! -f $input_file ]
then
	input_file=$3
	if [ ! -f $input_file ]
	then
		echo "$input_file does not exist"
		exit 1
	fi
fi
if [ -f $output_file ]
then
	echo "$output_file already exists"
	exit 1
fi


cd $old_repo

rm -f sha1s_orig ukeys nkeys mapped_keys
while read p; do
  SHA1=$(echo "$p" | awk -F';' '{print $1}')
  if [ "$SHA1" != "" ]
  then
	  echo $SHA1 >> $CWD/sha1s_orig

	  TSMP=$(git log -1 --pretty=format:"%ct%n" $SHA1)

	  # https://stackoverflow.com/a/7132381
	  git branch -a --contains $SHA1 | grep -v remote | sort | uniq |sed -e 's/\*//'|sed -e 's/ //g' > $CWD/branchtmp
	  BMD5=$(md5sum $CWD/branchtmp |awk '{print $1}')
	  rm $CWD/branchtmp

	  git diff --raw $SHA1^ $SHA1 > $CWD/keytmp
	  DIFFKEY=$(md5sum $CWD/keytmp |awk '{print $1}')
	  rm $CWD/keytmp

	  echo "SHA1: $SHA1"
	  echo "Timestamp: $TSMP"
	  echo "Branches MD5: $BMD5"
	  echo "Diff MD5: $DIFFKEY"
	  echo "$TSMP;$BMD5;$DIFFKEY" >> $CWD/ukeys
  fi
  echo "" >> $CWD/ukeys
done < $input_file

cd $new_repo

echo ""
echo "Mapping from $old_repo SHA1 keys to $new_repo SHA1 keys:"
echo ""

while read p; do
  TSMP=$(echo "$p" | awk -F';' '{print $1}')
  BMD5=$(echo "$p" | awk -F';' '{print $2}')
  DIFFKEY=$(echo "$p" | awk -F';' '{print $3}')
  pwd
  echo "Searching for: $TSMP   $BMD5   $DIFFKEY"
  git log --all --since $TSMP --until $TSMP --pretty=format:"%H" > $CWD/nkeys
  echo "" >> $CWD/nkeys
  while read d; do
	  CSHA1=$d
	  echo "	CSHA1: $CSHA1"

	  git branch -a --contains $CSHA1 | grep -v remote | sort | uniq |sed -e 's/\*//'|sed -e 's/ //g' > $CWD/branchtmp
	  NBMD5=$(md5sum $CWD/branchtmp |awk '{print $1}')
	  echo "	NBMD5: $NBMD5"
	  rm $CWD/branchtmp

	  if [ "$BMD5" == "$NBMD5" ]
	  then
		  git diff --raw $CSHA1^ $CSHA1 > $CWD/keytmp
		  NDIFFKEY=$(md5sum $CWD/keytmp |awk '{print $1}')
		  echo "	NDIFFKEY: $NDIFFKEY"
		  if [ "$NDIFFKEY" == "$DIFFKEY" ]
		  then
			  echo "	MATCH"
			  echo $d >> $CWD/mapped_keys
		  fi
		  rm $CWD/keytmp
	  fi
  done < $CWD/nkeys
  rm $CWD/nkeys
done < $CWD/ukeys

cd $CWD
