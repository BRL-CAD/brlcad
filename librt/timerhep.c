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
 *			T I M E R _ P R E P
 */
void
time_start()
{
	(void)time(&time0);
	(void)intime_();
}


/*
 *			T I M E R _ P R I N T
 * 
 */
double
timer_print(str)
char *str;
{
	long now;
	double usert;
	long hep[6];

	(void)stats_(hep);
	(void)time(&now);
	usert = ((double)hep[0]) / 10000000.0;
	fprintf(stderr,"%s: %f secs\n", str, usert);
	fprintf(stderr,"HEP %ld clocks, %ld waves, %ld fp, %ld dm, %ld other\n",
		hep[0], hep[1], hep[2], hep[3], hep[4] );
	return( usert );
}
