#ifndef lint
static char rcsid[] = "$Header$";
#endif
#include <stdio.h>
#include <math.h>
#include "./msr.h"
extern int Debug;
/*	msran	minimal standard random number generator
 *
 * Entry:
 *	setseed	seed to use
 *	method  method to use to generate numbers;
 *
 * Exit:
 *	returns	a pointer to a msr_unif structure.
 *	returns 0 on error.
 *
 * Uses:
 *	None.
 *
 * Calls:
 *	malloc
 *
 *	Stephen K. Park and Keith W. Miller
 *	Random number generators: good ones are hard to find
 *	CACM vol 31 no 10, Oct 88
 *	Translated to 'C' by Christopher T. Johnson
 *
 * $Log$
 * Revision 1.1  90/04/20  02:05:36  cjohnson
 * Initial revision
 * 
 * 
 */
#define	A	16807
#define M	2147483647
#define DM	2147483647.0
#define Q	127773		/* Q = M / A */
#define R	2836		/* R = M % A */
struct msr_unif *
msr_unif_init(setseed,method)
long setseed;
int method;
{
	struct msr_unif *p;
	p = (struct msr_unif *) malloc(sizeof(struct msr_unif));
	if (!p) {
		fprintf(stderr,"msr_init: Unable to allocat msr_unif.\n");
		return(NULL);
	}
	p->msr_longs = (long *) malloc(MSRMAXTBL*sizeof(long));
	p->msr_doubles=(double *) malloc(MSRMAXTBL*sizeof(double));
	p->msr_seed = 1;
	p->msr_long_ptr = 0;
	p->msr_double_ptr = 0;

	if (setseed&0x7fffffff) p->msr_seed=setseed&0x7fffffff;
	return(p);
}

/*	msr_unif_long_fill	fill a random number table.
 *
 * Use the msrad algorithm to fill a random number table
 * with values from 1 to 2^31-1.  These numbers can extracted from
 * the random number table via high speed macros and msr_unif_fill called
 * when the table is exauseted.
 *
 * Entry:
 *	p	pointer to a msr_unif structure.
 *
 * Exit:
 *	if p->msr_longs != NULL 
 *		msr_longs is reloaded with random numbers;
 *		msr_long_ptr is set to MSRMAXTBL
 *	endif
 *	msr_seed is updated.
 *	A long between 1 and 2^31-1 is returned.
 *
 * Calls:
 *	None.  msran is inlined for speed reasons.
 *
 * Uses:
 *	None.
 *
 * Author:
 *	Christopher T. Johnson.
 *
 */
long
msr_unif_long_fill(p)
struct msr_unif *p;
{
	register long test,work_seed;
	register int i;

	work_seed = p->msr_seed;

	if (p->msr_longs) {
		for (i=0; i < MSRMAXTBL; i++) {
			test = A*(work_seed % Q) - R*(work_seed / Q);
			p->msr_longs[i] = work_seed = (test < 0) ?
			     test+M : test;
		}
		p->msr_long_ptr = MSRMAXTBL;
	}
	test = A*(work_seed % Q) - R*(work_seed / Q);
	p->msr_seed = (test < 0) ? test+M : test;

	return(p->msr_seed);
}

double
msr_unif_double_fill(p)
struct msr_unif *p;
{
	register long test,work_seed;
	register int i;

	work_seed = p->msr_seed;

	if (p->msr_doubles) {
		for (i=0; i < MSRMAXTBL; i++) {
			test = A*(work_seed % Q) - R*(work_seed / Q);
			work_seed = (test < 0) ? test+M : test;
			p->msr_doubles[i] = ( work_seed - M/2) * 1.0/DM;
		}
		p->msr_double_ptr = MSRMAXTBL;
	}
	test = A*(work_seed % Q) - R*(work_seed / Q);
	p->msr_seed = (test < 0) ? test+M : test;

	return((p->msr_seed - M/2) * 1.0/DM);
}

struct msr_gauss *
msr_gauss_init(setseed,method)
long setseed;
int method;
{
	struct msr_gauss *p;
	p = (struct msr_gauss *) malloc(sizeof(struct msr_gauss));
	if (!p) {
		fprintf(stderr,"msr_init: Unable to allocat msr_gauss.\n");
		return(NULL);
	}
	p->msr_gausses=(double *) malloc(MSRMAXTBL*sizeof(double));
	p->msr_gauss_doubles=(double *) malloc(MSRMAXTBL*sizeof(double));
	p->msr_gauss_seed = 1;
	p->msr_gauss_ptr = 0;
	p->msr_gauss_dbl_ptr = 0;

	if (setseed&0x7fffffff) p->msr_gauss_seed=setseed&0x7fffffff;
	return(p);
}

double
msr_gauss_fill(p)
struct msr_gauss *p;
{
	register int i;
	/* register */ double v1,v2,r,fac;

	if (p->msr_gausses) {
		for (i=0; i< MSRMAXTBL; ) {
			MSR_UNIF_CIRCLE((struct msr_unif *)p,v1,v2,r);
			if (r<0.00001) continue;
			fac = sqrt(-2.0*log(r)/r);
			p->msr_gausses[i++] = v1*fac;
			p->msr_gausses[i++] = v2*fac;
		}
		p->msr_gauss_ptr = MSRMAXTBL;
	}

	do {
		MSR_UNIF_CIRCLE((struct msr_unif *)p,v1,v2,r);
	} while (r < 0.00001);
	fac = sqrt(-2.0*log(r)/r);
	return(v1*fac);
}
