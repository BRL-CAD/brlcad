/*			R E A D . C
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 * Ballistic Research Laboratory
 * U. S. Army
 * March 17, 1980
 */

#include <stdio.h>

extern FILE	*infp;

extern char name_it[16];		/* argv[3] */

/*
 *			G E T L I N E
 */
int
getline( cp )
register char *cp;
{
	register int	c;
	register int	count = 80;

	while( (c = fgetc(infp)) == '\n' ) /* Skip blank lines.		*/
		;
	while( c != EOF && c != '\n' )  {
		*cp++ = c;
		count--;
		if( count <= 0 )  {
			printf("input buffer (80) overflow\n");
			break;
		}
		c = fgetc(infp);
	}
	if( c == EOF )
		return	EOF;
	while( count-- > 0 ) 
		*cp++ = 0;
	return	c;
}


/*		N A M E C V T	 */
namecvt( n, cp, c )
register char *cp;
register int n;
{
	static char str[32];

	sprintf( str, "%c%d%s", c, n, name_it );
	strncpy( cp, str, 16 );		/* truncate str to 16 chars.*/
}
