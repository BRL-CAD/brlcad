/*                      T I M E R C O S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup timer */
/** @{ */
/** @file timercos.c
 *
 *	To provide timing information for RT.
 *	THIS VERSION FOR Cray COS "C"
 */
#ifndef lint
static const char RCScos[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys.h>

static float cpu0;

extern long time();

static long time0;
static long ns0;


/*
 *			P R E P _ T I M E R
 */
void
rt_prep_timer()
{
	(void)time(&time0);
	ns0 = clock();		/* Reset time to 0? */
	cpu0 = 0;
	x_sys(F_JTI, &cpu0 );
}


/*
 *			R E A D _ T I M E R
 *
 */
double
rt_read_timer(str,len)
char *str;
{
	long now, ns;
	double realt;
	double usert;
	char line[132];
	float cpunow;

	/* Real time */
	(void)time(&now);
	realt = now-time0;

	/* CPU time */
	ns = clock();		/* seems to be wall clock! */
	realt = ((double)(ns-ns0)) / 1.0e9;
	cpunow = 0;
	x_sys(F_JTI, &cpunow);
	usert = cpunow-cpu0;
	if( usert < 0.00001 )  usert = 0.00001;
	if( realt < 0.00001 )  realt = usert;
	sprintf(line,"%g CPU secs in %g elapsed secs (%g%%)",
		usert, realt,
		usert/realt*100 );
	(void)strncpy( str, line, len );
	return( usert );
}

/* Because COS memset() zaps registers, wrap in insulating function */
bzero( str, n )
{
	memset( str, '\0', n );
}

/*
 * Routines that seem to be missing or broken under COS on the XMP.
 */
perror(str)
char *str;
{
	fprintf(stderr,"%s:  file access failure\n");
}
chmod(str,val)
char *str;
{
	;
}
char *sbrk(i)
{
	return( (char *)0 );
}

/******* Needed only on broken versions of COS ********/
/*
 *	ascii to floating (atof)
 */
#include <ctype.h>
/******#include <values.h>******/
/* These values work with any binary representation of integers
 * where the high-order bit contains the sign. */

/* a number used normally for size of a shift */
#define BITSPERBYTE	8
#define BITS(type)	(BITSPERBYTE * (int)sizeof(type))

/* short, regular and long ints with only the high-order bit turned on */
#define HIBITS	((short)(1 << BITS(short) - 1))
#define HIBITI	(1 << BITS(int) - 1)
#define HIBITL	(1L << BITS(long) - 1)

/* largest short, regular and long int */
#define MAXSHORT	((short)~HIBITS)
#define MAXINT	(~HIBITI)
#define MAXLONG	(~HIBITL)

/* various values that describe the binary floating-point representation
 * _EXPBASE	- the exponent base
 * DMAXEXP 	- the maximum exponent of a double (as returned by frexp())
 * FMAXEXP 	- the maximum exponent of a float  (as returned by frexp())
 * DMINEXP 	- the minimum exponent of a double (as returned by frexp())
 * FMINEXP 	- the minimum exponent of a float  (as returned by frexp())
 * MAXDOUBLE	- the largest double
			((_EXPBASE ** DMAXEXP) * (1 - (_EXPBASE ** -DSIGNIF)))
 * MAXFLOAT	- the largest float
			((_EXPBASE ** FMAXEXP) * (1 - (_EXPBASE ** -FSIGNIF)))
 * MINDOUBLE	- the smallest double (_EXPBASE ** (DMINEXP - 1))
 * MINFLOAT	- the smallest float (_EXPBASE ** (FMINEXP - 1))
 * DSIGNIF	- the number of significant bits in a double
 * FSIGNIF	- the number of significant bits in a float
 * DMAXPOWTWO	- the largest power of two exactly representable as a double
 * FMAXPOWTWO	- the largest power of two exactly representable as a float
 * _IEEE	- 1 if IEEE standard representation is used
 * _DEXPLEN	- the number of bits for the exponent of a double
 * _FEXPLEN	- the number of bits for the exponent of a float
 * _HIDDENBIT	- 1 if high-significance bit of mantissa is implicit
 * LN_MAXDOUBLE	- the natural log of the largest double  -- log(MAXDOUBLE)
 * LN_MINDOUBLE	- the natural log of the smallest double -- log(MINDOUBLE)
 */
#define MAXDOUBLE	0.7237005577332262e+76
#define MAXFLOAT	((float)0.7237005145e+76)
#define MINDOUBLE	5.3976053469342702e-79
#define MINFLOAT	((float)MINDOUBLE)
#define _IEEE		0
#define _DEXPLEN	15
#define _HIDDENBIT	0
#define DMINEXP	(-(DMAXEXP + 1))
#define FMINEXP	(-(FMAXEXP + 1))

#define _FEXPLEN	15
#define DSIGNIF	(BITS(double) - _DEXPLEN + _HIDDENBIT - 1)
#define FSIGNIF	(BITS(float)  - _FEXPLEN + _HIDDENBIT - 1)
#define DMAXPOWTWO	((double)(1L << BITS(long) - 2) * \
				(1L << DSIGNIF - BITS(long) + 1))
#define FMAXPOWTWO	((float)(1L << FSIGNIF - 1))
#define DMAXEXP	((1 << _DEXPLEN - 1) - 1 + _IEEE)
#define FMAXEXP	((1 << _FEXPLEN - 1) - 1 + _IEEE)
#define LN_MAXDOUBLE	(M_LN2 * DMAXEXP)
#define LN_MINDOUBLE	(M_LN2 * (DMINEXP - 1))
#define H_PREC	(DSIGNIF % 2 ? (1L << DSIGNIF/2) * M_SQRT2 : 1L << DSIGNIF/2)
#define X_EPS	(1.0/H_PREC)
#define X_PLOSS	((double)(long)(M_PI * H_PREC))
#define X_TLOSS	(M_PI * DMAXPOWTWO)
#define M_LN2	0.69314718055994530942
#define M_PI	3.14159265358979323846
#define M_SQRT2	1.41421356237309504880
#define MAXBEXP	DMAXEXP /* for backward compatibility */
#define MINBEXP	DMINEXP /* for backward compatibility */
#define MAXPOWTWO	DMAXPOWTWO /* for backward compatibility */
/******#include <values.h>******/

extern double ldexp();

#	define POW1_25LEN	7

static double pow1_25[POW1_25LEN] = { 0.0 };

#define STORE_PTR
#define GOT_DIGIT
#define RET_ZERO(val)	if (!val) return (0.0)

double
atof(p)
register char *p;
{
	register int c;
	int exp = 0, neg_val = 0;
	double fl_val;

	while (isspace(c = *p)) /* eat leading white space */
		p++;
	switch (c) { /* process sign */
	case '-':
		neg_val++;
	case '+': /* fall-through */
		p++;
	}
	{	/* accumulate value */
		register long high = 0, low = 0, scale = 1;
		register int decpt = 0, nzeroes = 0;

		while (isdigit(c = *p++) || c == '.' && !decpt++) {
			if (c == '.')
				continue;
			GOT_DIGIT;
			if (decpt) { /* handle trailing zeroes specially */
				if (c == '0') { /* ignore zero for now */
					nzeroes++;
					continue;
				}
				while (nzeroes > 0) { /* put zeroes back in */
					exp--;
					if (high < MAXLONG/10) {
						high *= 10;
					} else if (scale < MAXLONG/10) {
						scale *= 10;
						low *= 10;
					} else
						exp++;
					nzeroes--;
				}
				exp--; /* decr exponent if decimal pt. seen */
			}
			if (high < MAXLONG/10) {
				high *= 10;
				high += c - '0';
			} else if (scale < MAXLONG/10) {
				scale *= 10;
				low *= 10;
				low += c - '0';
			} else
				exp++;
		}
		RET_ZERO(high);
		fl_val = (double)high;
		if (scale > 1)
			fl_val = (double)scale * fl_val + (double)low;
	}
	STORE_PTR; /* in case there is no legitimate exponent */
	if (c == 'E' || c == 'e') { /* accumulate exponent */
		register int e_exp = 0, neg_exp = 0;

		switch (*p) { /* process sign */
		case '-':
			neg_exp++;
		case '+': /* fall-through */
		case ' ': /* many FORTRAN environments generate this! */
			p++;
		}
		if (isdigit(c = *p)) { /* found a legitimate exponent */
			do {
				/* limit outrageously large exponents */
				if (e_exp < DMAXEXP)
					e_exp = 10 * e_exp + c - '0';
			} while (isdigit(c = *++p));
			if (neg_exp)
				exp -= e_exp;
			else
				exp += e_exp;
			STORE_PTR;
		}
	}
	/*
	 * The following computation is done in two stages,
	 * first accumulating powers of (10/8), then jamming powers of 8,
	 * to avoid underflow in situations like the following (for
	 * the DEC representation): 1.2345678901234567890e-37,
	 * where exp would be about (-37 + -18) = -55, and the
	 * value 10^(-55) can't be represented, but 1.25^(-55) can
	 * be represented, and then 8^(-55) jammed via ldexp().
	 */
	if (exp != 0) { /* apply exponent */
		register double *powptr = pow1_25, fl_exp = fl_val;

		if (*powptr == 0.0) { /* need to initialize table */
			*powptr = 1.25;
			for (; powptr < &pow1_25[POW1_25LEN - 1]; powptr++)
				powptr[1] = *powptr * *powptr;
			powptr = pow1_25;
		}
		if ((c = exp) < 0) {
			c = -c;
			fl_exp = 1.0;
		}
		if (c > DMAXEXP/2) /* outrageously large exponents */
			c = DMAXEXP/2; /* will be handled by ldexp */
		for ( ; ; powptr++) {
			/* binary representation of ints assumed; otherwise
			 * replace (& 01) by (% 2) and (>>= 1) by (/= 2) */
			if (c & 01)
				fl_exp *= *powptr;
			if ((c >>= 1) == 0)
				break;
		}
		fl_val = ldexp(exp < 0 ? fl_val/fl_exp : fl_exp, 3 * exp);
	}
	return (neg_val ? -fl_val : fl_val); /* apply sign */
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
