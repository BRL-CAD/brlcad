#define PADCHR		~(1<<15)		/* non data value.*/

char *
endstr( str ) 
char *str;
{	while( *str != 0 )	*str++;
	return( str );
}

strcpy( s, t )	/* === */
char	*s, *t;
{
	while( (*s++ = *t++) != '\0' );
	*s = '\0';
}
strappend( s, t )	/* === */
char	*s, *t;
{	s = endstr( s );
	while( (*s++ = *t++) != '\0' );
	*s = '\0';
}

maxmin( l, n, max, min )	/*  === */
int    *l, n,*max,*min;
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
