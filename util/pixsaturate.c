/*
 *			P I X S A T U R A T E . C
 *
 *  A saturation value of 0 gives monochrome,
 *  1.0 gives the original image,
 *  and values larger than 1.0 give a more saturated image.
 *
 *  Author -
 *	Michael John Muuss
 *	(based on a subroutine by Paul Haeberli)
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 *
 *  Remarks from Haeberli's routine:
 *	saturate-
 *		Change the saturation of a row of pixels.  If sat is
 *	set to 0.0 the result will be monochromatic, if sat is made
 *	1.0, the color will not change, if sat is made greater than 1.0,
 *	the amount of color is increased.
 *	
 *	The input and output pixel values are in the range 0..255.
 *
 *	This technique requires 6 multiplies, 5 adds and 3 bound
 *	checks per pixel.
 *
 *			Paul Haeberli - 1988
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>
#include <math.h>

#define RINTLUM	(79)
#define GINTLUM	(156)
#define BINTLUM	(21)

char	buf[3*16*1024];

main(argc, argv)
int	argc;
char	**argv;
{
	double	sat;			/* saturation */
	int	bw;			/* monochrome intensity */
	int	rwgt, gwgt, bwgt;
	int	rt, gt, bt;
	int	n;
	int	nby;
	register unsigned char	*cp;

	if( argc != 2 )  {
		fprintf(stderr, "Usage: pixsaturate saturation\n");
		exit(1);
	}
	sat = atof(argv[1]);

	rwgt = RINTLUM*(1.0-sat);
	gwgt = GINTLUM*(1.0-sat);
	bwgt = BINTLUM*(1.0-sat);

	while( (nby = fread( buf, 1, sizeof(buf), stdin )) > 0 )  {
		cp = (unsigned char *)buf;
		for( n = nby ; n > 0; n -= 3 )  {
			rt = cp[0];
			gt = cp[1];
			bt = cp[2];
			bw = (rwgt*rt + gwgt*gt + bwgt*bt)>>8;
			rt = bw+sat*rt;
			gt = bw+sat*gt;
			bt = bw+sat*bt;
			if(rt<0) rt = 0; else if(rt>255) rt=255;
			if(gt<0) gt = 0; else if(gt>255) gt=255;
			if(bt<0) bt = 0; else if(bt>255) bt=255;
			*cp++ = rt;
			*cp++ = gt;
			*cp++ = bt;
		}
		if( fwrite( buf, 1, nby, stdout ) != nby )  {
			perror("fwrite");
			exit(1);
		}
	}
	return(0);
}
