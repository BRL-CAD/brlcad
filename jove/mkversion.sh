#!/bin/sh
# @(#)$Header$ (BRL)

if test ! -f version.number
then
	echo 0 > version.number
fi
p=`cat version.number`
c=`echo 0 1 $p +p | dc`

rm -f version.c version.number

echo "char	*version = \"2.$c\";" > version.c
echo $c > version.number

cc -c version.c
