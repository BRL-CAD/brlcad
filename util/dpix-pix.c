/*
 *			D P I X - P I X . C
 *
 *  Convert double precision images in .dpix form to a .pix file.
 *  By default, will determin min/max values to drive exposure
 *  calculations, and perform linear interpolation on the way to 1-byte
 *  values.
 *
 *  Reads the binary input file, finds the minimum and maximum values
 *  read, and linearly interpolates these values between 0 and 255.
 *
 *  Author -
 *	S. Muuss, J.D., Feb 04, 1990.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <stdlib.h>

#include "machine.h"
#include "externs.h"

#define NUM	(1024 * 16)	/* Note the powers of 2 -- v. efficient */
static double		doub[NUM];
static unsigned char	cha[NUM];

int
main(argc, argv)
int	argc;
char	*argv[];
{
	int		count;			/* count of items */
	int		got;			/* count of bytes */
	int		fd;			/* UNIX file descriptor */
	register double	*dp;			/* ptr to d */
	register double *ep;
	double		m;			/* slope */
	double		b;			/* intercept */

	if( argc < 2 )  {
		fprintf(stderr, "Usage: dpix-pix file.dpix > file.pix\n");
		exit(1);
	}

	if( (fd = open(argv[1], 0)) < 0 )  {
		perror(argv[1]);
		exit(1);
	}

	if( isatty(fileno(stdout)) )  {
		fprintf(stderr, "dpix-pix:  binary output directed to terminal, aborting\n");
		exit(2);
	}

	/* Note that the minimum is set to 1.0e20, the computer's working
	 * equivalent of positive infinity.  Thus any subsequent value
	 * must be larger. Likewise, the maximun is set to -1.0e20, the
	 * equivalent of negative infinity, and any values must thus be
	 * bigger than it.
	 */
	{
		register double	min, max;		/* high usage items */

		min = 1.0e20;
		max = -1.0e20;

		while(1)  {
			got = read( fd, (char *)&doub[0], NUM*sizeof(doub[0]) );
			if( got <= 0 )
				break;
			count = got / sizeof(doub[0]);
			ep = &doub[count];
			for(dp = &doub[0]; dp < ep;)  {
				register double val;
				if( (val = *dp++) < min )
					min = val;
				else if ( val > max )
					max = val;
			}
		}

		lseek( fd, 0L, 0 );		/* rewind(fp); */
	

		/* This section uses the maximum and the minimum values found to
		 * compute the m and the b of the line as specified by the
		 * equation y = mx + b.
		 */
		fprintf(stderr, "min=%f, max=%f\n", min, max);
		if (max < min)  {
			printf("MINMAX: max less than min!\n");
			exit(1);
		}

		m = (255 - 0)/(max - min);
		b = (-255 * min)/(max - min);
	}

	while (1)  {
		register char	*cp;		/* ptr to c */
		register double	mm;		/* slope */
		register double	bb;		/* intercept */

		mm = m;
		bb = b;

		got = read( fd, (char *)&doub[0], NUM*sizeof(doub[0]) );
		if (got <=  0 )
			break;
		count = got / sizeof(doub[0]);
		ep = &doub[count];
		cp = (char *)&cha[0];
		for(dp = &doub[0]; dp < ep;)  {
			*cp++ = mm * (*dp++) + bb;
		}

		/* fd 1 is stdout */
		got = write( 1, (char *)&cha[0], count*sizeof(cha[0]) );
		if( got != count*sizeof(cha[0]) )  {
			perror("write");
			exit(2);
		}
	}

	exit(0);
}
