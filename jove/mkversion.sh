#!/bin/sh
# @(#)$Header$ (BRL)

if test ! -f version.number
then
	echo 0 > version.number
fi

c=`cat version.number`
rm -f version.number version.c
echo $c | awk '{version = $1 + 1; };END{printf "%d\n", version > "version.number"; }'
c=`cat version.number`

echo "char	version[] = \"2.$c\";" > version.c
