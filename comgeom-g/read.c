/*			R E A D . C
 * This module contains all of the routines necessary to read in
 * a COMGEOM input file, and put the information into internal form.
 * Ballistic Research Laboratory
 * U. S. Army
 * March 17, 1980
 */
extern double atof();
extern char *malloc();

#include <stdio.h>

#include "./ged_types.h"
#include "./3d.h"

extern int sol_total, sol_work;
extern int	reg_total;
extern int	version;

extern long regionpos;
extern long endpos;

extern FILE	*infp;
extern FILE	*outfp;		/* Output only fd */

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
	static char str[16];
	extern char name_it[16];		/* argv[3] */

	sprintf( str, "%c%d%s", c, n, name_it );
	strncpy( cp, str, 16 );		/* truncate str to 16 chars.*/
}


/*		M O V E N A M E
 * Ensure that name field is null padded */
movename( from, to )
register char *from, *to;
{ register int i;
	for( i=0; i<16; i++ )		to[i] = 0;
	while( *to++ = *from++ );
}
