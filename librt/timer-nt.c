/*
 *			T I M E R - N T . C
 *
 * Function -
 *	To provide timing information on Microsoft Windows NT.
 */
#ifndef lint
static const char RCStimer_nt[] = "@(#)$Header$ (BRL)";
#endif

#include <stdio.h>

/*
 *			P R E P _ T I M E R
 */
void
rt_prep_timer()
{

}


/*
 *			R E A D _ T I M E R
 * 
 */
double
rt_read_timer(str,len)
char *str;
{
	char line[132];

	sprintf(line,"timer-nt.c:  I don't know how to tell time on NT yet!");
	if(str && len > 0)  (void)strncpy( str, line, len );
	return( 0.42 );
}
