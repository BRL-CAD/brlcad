
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#define PADCHR		~(1<<15)		/* non data value.*/

char *
endstr(char *str)
{	while( *str != 0 )	*str++;
	return( str );
}

#ifndef HAVE_STRCPY
strcpy(char *s, char *t)	/* === */
    	       
{
	while( (*s++ = *t++) != '\0' );
	*s = '\0';
}
#endif

strappend(char *s, char *t)	/* === */
    	       
{	s = endstr( s );
	while( (*s++ = *t++) != '\0' );
	*s = '\0';
}

maxmin(int *l, int n, int *max, int *min)	/*  === */
                       
{
	*max = -PADCHR;
	*min =  PADCHR;
/*BUGoff/printf( "max=%d min=%d\n", *max, *min );/* BUG */
	while( --n>0 )
	{
		if( *l > *max )	*max = *l;
		if( *l < *min )	*min = *l;
		++l;
	}
/*BUGoff/printf( "max=%d min=%d\n", *max, *min );/* BUG */	
}
