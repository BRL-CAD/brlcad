#! /bin/sh
awk '$1=="C" && length($2) == 1 { chr = $2 ; num = 0;okay=1}
     $1=="C" && length($2) != 1 { okay=0 }
     $1=="m" || $1=="n" { if(okay) {
			     printf("%d %d %s %s %d %d\n",index(" !\"#$%&'"'"'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~",chr)+31,num,chr,$1,$2,$3)
			     num++}}' $1 |
sort -n +0n -1n +1n -2n|
awk 'BEGIN { ind=0; count=0;pr = 0
	     print "#include \"../tools/font.h\""
	     print "move_t Moves[] = {" }
     $2==0 { for ( ; ind+32<$1 ; ind++) Base[ind] = count;
	     if ( pr%4 != 0 )
		printf("\n")
	     printf("/* %s */",$3)
	     pr = 0 }
	   { if ( pr%4 == 0 && pr != 0)
		printf("       ");
	     printf(" '\''%s'\'', %3d, %3d,",$4,$5,$6)
             if ( pr%4 == 3 )
		printf("\n");
	     count++;pr++
	   }
     END   { Base[ind++] = count
	     if(pr%4!=0) printf("\n")
		print "};"
	     print "int Base[] = {"
	     for(i=0 ; i<ind ; i++ ) {
		if(i%8==0) 
			printf("   ")
		printf(" %4d,", Base[i]);
		if(i%8==7) 
			printf("\n")
	   }
	   if ( i%4 != 0 ) printf("\n");
	   print "};"
	 }'
