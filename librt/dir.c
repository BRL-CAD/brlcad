/*
 *			D I R . C
 *
 * Ray Tracing program, GED database directory manager.
 *
 * Author -
 *	Michael John Muuss
 *
 *	U. S. Army Ballistic Research Laboratory
 *	April 2, 1984
 *
 * $Revision$
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$";
#endif

#include "vmath.h"
#include "db.h"
#include "rtdir.h"
#include "debug.h"

extern char	*malloc();

static struct directory *DirHead = DIR_NULL;

int	ged_fd;		/* FD of object file */

/*
 *			D I R _ B U I L D
 *
 * This routine reads through the GED database and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 */
void
dir_build(filename)
char *filename;
{
	static union record	record;
	static long	addr;

	if( (ged_fd = open(filename, 0)) < 0 )  {
		perror(filename);
		bomb("Unable to continue");
	}

	while(1)  {
		addr = lseek( ged_fd, 0L, 1 );
		if( (unsigned)read( ged_fd, (char *)&record, sizeof record )
				!= sizeof record )
			break;

		/*
		 * Check for a deleted record
		 */
		if( record.u_id == ARS_A )  {
			if( record.a.a_name[0] != '\0' )
				dir_add( record.a.a_name, addr );

			/* Skip remaining B type records.	*/
			(void)lseek(	ged_fd,
					(long)(record.a.a_totlen) *
					(long)(sizeof record),
					1 );
			continue;
		}

		if( record.u_id == SOLID )  {
			if( record.s.s_name[0] != '\0' )
				dir_add( record.s.s_name, addr );
			continue;
		}
		if( record.u_id != COMB )  {
			(void)printf( "dir_build:  unknown record %c (0%o)\n",
				record.u_id, record.u_id );
			/* skip this record */
			continue;
		}

		/* Check for a deleted combination record */
		if( record.c.c_name[0] != '\0' )
			dir_add( record.c.c_name, addr );

		/* Skip over combination member records */
		(void)lseek( ged_fd,
			(long)record.c.c_length * (long)sizeof record,
			1 );
	}
}

/*
 *			D I R _ L O O K U P
 *
 * This routine takes a name, and looks it up in the
 * directory table.  If the name is present, a pointer to
 * the directory struct element is returned, otherwise
 * NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only
 * the return code indicates failure.
 */
struct directory *
dir_lookup( str, noisy )
register char *str;
{
	register struct directory *dp;

	for( dp = DirHead; dp != DIR_NULL; dp=dp->d_forw )  {
		if ( strcmp( str, dp->d_namep ) == 0 )
			return(dp);
	}

	if( noisy )
		(void)printf("dir_lookup:  could not find '%s'\n", str );
	return( DIR_NULL );
}

/*
 *			D I R A D D
 *
 * Add an entry to the directory
 */
struct directory *
dir_add( name, laddr )
register char *name;
long laddr;
{
	register struct directory *dp;

	GETSTRUCT( dp, directory );
	dp->d_namep = strdup( name );
	dp->d_addr = laddr;
	dp->d_forw = DirHead;
	DirHead = dp;
	return( dp );
}

/*
 *			S T R D U P
 *
 * Given a string, allocate enough memory to hold it using malloc(),
 * duplicate the strings, returns a pointer to the new string.
 */
char *strdup( cp )
register char *cp;
{
	register char	*base;
	register char	*current;

	if( (base = malloc( strlen(cp)+1 )) == (char *)0 )
		bomb("strdup:  unable to allocate memory");

	current = base;
	do  {
		*current++ = *cp;
	}  while( *cp++ != '\0' );

	return(base);
}
