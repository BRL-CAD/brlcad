p=`cat version.number`
c=`echo 0 1 $p +p | dc`
echo "char	*version = \"1.$c\";" > version.c
cc -c version.c
echo $c > version.number
