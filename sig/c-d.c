#include <stdio.h>
#include <math.h>

static char usage[] = "\
Usage: c-d -r -i -m -p -z < complex_data > doubles\n";

int	rflag = 0;
int	iflag = 0;
int	mflag = 0;
int	pflag = 0;
int	zflag = 0;

double	ibuf[512];
double	obuf[512];
double	*obp;

main( argc, argv )
int argc; char **argv;
{
	int	i, num, onum;

	if( argc <= 1 || isatty(fileno(stdin)) ) {
		fprintf( stderr, usage );
		exit( 1 );
	}

	while( argc > 1 && argv[1][0] == '-' )  {
		switch( argv[1][1] )  {
		case 'r':
			rflag++;
			break;
		case 'i':
			iflag++;
			break;
		case 'm':
			mflag++;
			break;
		case 'p':
			pflag++;
			break;
		case 'z':
			zflag++;
			break;
		}
		argc--;
		argv++;
	}

	while( (num = fread( &ibuf[0], sizeof( ibuf[0] ), 512, stdin)) > 0 ) {
		onum = 0;
		obp = obuf;
		for( i = 0; i < num; i += 2 ) {
			if( rflag ) {
				*obp++ = ibuf[i];
				onum++;
			}
			if ( iflag ) {
				*obp++ = ibuf[i+1];
				onum++;
			}
			if( mflag ) {
				*obp++ = hypot( ibuf[i], ibuf[i+1] );
				onum++;
			}
			if( pflag ) {
				if( ibuf[i] == 0 && ibuf[i+1] == 0 )
					*obp++ = 0;
				else
					*obp++ = atan2( ibuf[i], ibuf[i+1] );
				onum++;
			}
			if( zflag ) {
				*obp++ = 0.0;
				onum++;
			}
		}
		fwrite( &obuf[0], sizeof( obuf[0] ), onum, stdout );
	}
}
