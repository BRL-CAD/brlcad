/* F 2 A () ====  convert float to ascii  w.df format.	No leading blanks */
f2a(	f, s,	w, d )
float	f;		/* INPUT	===*/
char	   s[];		/* OUTPUT	===*/
int		w, d;	/* length	===*/
{ int	c, i, j;
  long	n, sign;
	if( w <= d + 2 )
	{	printf( "ftoascii: incorrect format  need w.df\n");
		printf( "w must be at least 2 bigger then d.\n" );
		printf( "w= %f\t d= %f\n", w, d );
		printf( "STOP\n");
		exit( 10 );
	}
	for( i = 1; i <= d; i++ ) f = f * 10.0;	/* shift left.*/
	if( f < 0.0 )	f -= 0.5;		/* round up */
	else		f += 0.5;
	n = f;					/* truncate.*/
	if( (sign = n) < 0 )	n = -n;		/* get sign.*/
	i = 0;			/* CONVERT to ASCII.*/
	do {	s[i++] = n % 10 + '0';
		if( i == d )	s[i++] = '.';
	} while( (n /= 10) > 0 );
	if( i < d )		/* zero fill the d field if (f < 1).*/
	{	for( j = i; j < d; j++ )	s[j] = '0';
		s[j++] = '.';
		i = j;
	}
	if( sign < 0 )	s[i++] = '-';		/* apply sign.*/
	if( i > w )	printf("ftoascii: field length too small\n");
	w = i;					/* do not blank fill.*/
	for ( j = i; j < w; j++ ) s[j] = ' ';	/* blank fill.*/
	s[w] = '\0';
	for(	i = 0,	j = w - 1;		/* reverse the array.*/
		i < j;
		i++,	j-- )
	{	c    = s[i];	s[i] = s[j];	s[j] =    c;	}
}
