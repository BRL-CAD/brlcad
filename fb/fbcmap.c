/*
 *			C M A P
 *
 * Mike Muuss, 7/17/82
 * VAX version 10/18/83
 *
 * Compile:	cc cmap.c iklib.o -o cmap
 *
 * Usage:	cmap [flavor]
 *
 * When invoked with no arguments, or with a flavor of 0,
 * the standard ramp color-map is written.
 * Other flavors provide interesting alternatives.
 */

long cmap[1024];		/* Color map work area */

main(argc, argv)
char *argv[];
{
	register int i;
	register int flavor = 0;
	register int fudge;

	if( argc > 1 )
		flavor = atoi(argv[1] );

	ikopen();

	switch( flavor )  {

	case 0:
		/* Standard */
		for( i = 0; i < 256; i++ )  {
			cmap[i] =
				( i << (2+20) ) |		/* B */
				( i << (2+10) ) |		/* G */
				( i << (2+0 ) );		/* R */
		}
		break;
	case 1:
		for( i = 0; i < 256; i++ )  {
			cmap[i] =
				( i << (2+20) ) |		/* B */
				( i << (2+10) ) |		/* G */
				( i << (2+0 ) );		/* R */
			swap( &cmap[i] );
		}
		break;
	case 2:
		for( i = 0; i < 256; i++ )  {
			cmap[i] = (
				( i << (2+20) ) |		/* B */
				( i << (2+10) ) |		/* G */
				( i << (2+0 ) )			/* R */
			) & 0xFFFF;
		}
		break;
	case 3:
		/* Inverse Standard */
		for( i = 0; i < 256; i++ )  {
			cmap[255-i] =
				( i << (2+20) ) |		/* B */
				( i << (2+10) ) |		/* G */
				( i << (2+0 ) );		/* R */
		}
		break;
	case 4:
		/* Experimental correction, for POLAROID 8x10 print film */
		cmap[0] = 0;		/* BLACK */
#define BOOST(point, bias) \
	((int)((bias)+((float)(point)/256.*(255-(bias)))))
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST(i, 70);
			cmap[i] |= ( fudge << (2+20) );		/* B */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = i;
			cmap[i] |= ( fudge << (2+10) );		/* G */
		}
		for( i = 1; i < 256; i++ )  {
			fudge = BOOST( i, 30 );
			cmap[i] |= ( fudge << (2+0 ) );		/* R */
		}
		break;
	case 5:
		/* Standard, with low intensities set to black */
		for( i = 100; i < 256; i++ )  {
			cmap[i] =
				( i << (2+20) ) |		/* B */
				( i << (2+10) ) |		/* G */
				( i << (2+0 ) );		/* R */
		}
		break;
	case 6:
		/* Amplify middle of the range, for Moss's dim pictures */
#define UPSHIFT	64
		cmap[0] = 0L;			/* Black */
		for( i=1; i< 256-UPSHIFT; i++ )  {
			register int j = i + UPSHIFT;
			cmap[i] =
				( j << (2+20) ) |		/* B */
				( j << (2+10) ) |		/* G */
				( j << (2+0 ) );		/* R */
		}
		for( i=256-UPSHIFT; i < 256; i++ )  {
			cmap[i] = 0x3FFFFFFF;		/* Full Scale */
		}
		break;

	default:
		printf("Sorry, flavor %d not implemented\n", flavor);
		exit(1);
	}

	/*
	 * Replicate first copy of color map onto second copy,
	 * and also do the "overlay" portion too.
	 */
	 for( i=0; i < 256; i++ )
		cmap[i+256] = cmap[i+512] = cmap[i+512+256] = cmap[i];

	ikwmap( cmap );
}

/* For goeffy results only, a short-in-long swapper */
swap(val)
long *val;
{
	union {
		long val32;
		short val16[2];
	} temp;
	short tmp;

	temp.val32 = *val;

	tmp = temp.val16[0];
	temp.val16[0] = temp.val16[1];
	temp.val16[1] = tmp;
	*val = temp.val32;
}
