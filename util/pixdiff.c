/* pixdiff.c */
#include <stdio.h>

#define NPIX	256*3
unsigned char buf1[NPIX], buf2[NPIX];

main(argc, argv)
char **argv;
{
	FILE *f1, *f2;
	register unsigned char *i1, *i2;

	if( argc != 3 )  {
		fprintf(stderr,"Usage: pixdiff i1 i2\n");
		exit(0);
	}

	if( (f1 = fopen( argv[1], "r" ) ) == NULL )  {
		perror( argv[1] );
		exit(1);
	}
	if( (f2 = fopen( argv[2], "r" ) ) == NULL )  {
		perror( argv[2] );
		exit(1);
	}
	while(1)  {
		if( fread( buf1, sizeof(buf1), 1, f1 ) <= 0  || feof(f1) )
			exit(0);
		if( fread( buf2, sizeof(buf2), 1, f2 ) <= 0  || feof(f2) )
			exit(0);
		for( i1=buf1, i2=buf2; i1 < &buf1[NPIX]; )  {
			if( *i1 != *i2++ )
				*i1++ = 0xFF;
			else
				*i1++ = 0;
		}
		write( 1, buf1, sizeof(buf1) );
	}
}
