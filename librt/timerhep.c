/*
 *			T I M E R H E P . C
 *
 * Function -
 *	To provide timing information for RT.
 *	THIS VERSION FOR Denelcor HEP/UPX (System III-like)
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

/* Resource locks */
int	res_pt;
int	res_seg;
int	res_malloc;
int	res_printf;

/* Memory clearing routine */
bzero( str, n )
register char *str;
register int n;
{
	while( n-- > 0 )
		*str++ = 0;
}

/* Standard System V stuff */
extern long time();
static long time0;


/*
 *			P R E P _ T I M E R
 */
void
prep_timer()
{
	(void)time(&time0);
	(void)intime_();
}


/*
 *			P R _ T I M E R
 * 
 */
double
pr_timer(str)
char *str;
{
	long now;
	double usert;
	long htime[6];

	(void)stats_(htime);
	(void)time(&now);
	usert = ((double)htime[0]) / 10000000.0;
	fprintf(stderr,"%s: %f secs, ", str, usert);
	fprintf(stderr,"%ld waves, %ld fp, %ld dm, %ld other\n",
		htime[0], htime[1], htime[2], htime[3], htime[4] );
	return( usert );
}

bcopy(from, to, count)		/* not efficient */
register char *from;
register char *to;
register int count;
{
	while( count-- > 0 )
		*to++ = *from++;
}
