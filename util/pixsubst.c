#include <stdio.h>
#include <stdlib.h>

extern int errno;

int
main(int argc, char **argv)
{

	unsigned char pix[3],pixin[3],pixout[3];
	int npixels;

	if( argc != 4 && argc != 7 )
	{
		fprintf( stderr , "Usage:\n" );
		fprintf( stderr , "\t%s [R_in G_in B_in] R_out G_out B_out  < pix_in > pix_out\n", argv[0] );
		fprintf( stderr , "\t\tRGB_in is changed to RGB_out\n" );
		fprintf( stderr , "\t\tif RGB_in is not provided, the first pixel input is used\n" );
		exit( 1 );
	}

	if( argc == 7 )
	{
		argv++;
		pixin[0] = '\0' + atoi( *argv );
		argv++;
		pixin[1] = '\0' + atoi( *argv );
		argv++;
		pixin[2] = '\0' + atoi( *argv );
	}

	argv++;
	pixout[0] = '\0' + atoi( *argv );
	argv++;
	pixout[1] = '\0' + atoi( *argv );
	argv++;
	pixout[2] = '\0' + atoi( *argv );

	if( argc == 4 )
	{
		if( (npixels=fread( pixin , sizeof( unsigned char ) , 3 , stdin ) ) != 3 )
		{
			fprintf( stderr, "Unexpected end of input!!!\n" );
			exit( 1 );
		}
		fwrite( pixout , sizeof( unsigned char ) , npixels , stdout );
	}

	while( (npixels=fread( pix , sizeof( unsigned char ) , 3 , stdin ) ) == 3 )
	{
		if( pix[0] == pixin[0] && pix[1] == pixin[1] && pix[2] == pixin[2] )
			fwrite( pixout , sizeof( unsigned char ) , npixels , stdout );
		else
			fwrite( pix , sizeof( unsigned char ) , npixels , stdout );
	}
	return 0;
}
