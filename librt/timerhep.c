/*
 *			T I M E R H E P . C
 *
 * Function -
 *	To provide timing information for RT.
 *	THIS VERSION FOR Denelcor HEP/UPX (System III-like)
 */
#ifndef lint
static const char RCShep[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

/* Standard System V stuff */
extern long time(time_t *);
static long time0;


/*
 *			P R E P _ T I M E R
 */
void
rt_prep_timer(void)
{
	(void)time(&time0);
	(void)intime_();
}


/*
 *			R E A D _ T I M E R
 * 
 */
double
rt_read_timer(char *str, int len)
{
	long now;
	double usert;
	long htime[6];
	char line[132];

	(void)stats_(htime);
	(void)time(&now);
	usert = ((double)htime[0]) / 10000000.0;
	if( usert < 0.00001 )  usert = 0.00001;
	sprintf(line,"%f secs: %ld wave, %ld fp, %ld dmem, %ld other",
		usert,
		htime[0], htime[1], htime[2], htime[3], htime[4] );
	(void)strncpy( str, line, len );
	return( usert );
}
