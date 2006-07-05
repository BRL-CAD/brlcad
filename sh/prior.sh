
psql -f prior.psql -U forge -h forge forge | \
sed -E -e 's/[  ]{2,}//g' -e 's/[ ]*\|[ ]*/\|/g' -e 's/\|/,/g' | \
awk -F , '{ if (match($4, "[12]+")) { sub(1, "Open", $4) ;  printf "%s,Forge,%s,%s,%s\n", $1, $2, $3 + 10, $4 	 } }'
exit
