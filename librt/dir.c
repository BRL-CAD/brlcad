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

#include <stdio.h>
#include "vmath.h"
#include "db.h"
#include "rtdir.h"
#include "debug.h"

static struct directory *DirHead = DIR_NULL;

int	ged_fd = -1;		/* FD of object file */
extern char *malloc();

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

/*
 *			D I R _ B U I L D
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 *
 * Returns -
 *	0	Success
 *	-1	Fatal Error
 */
int
dir_build(filename)
char *filename;
{
	static union record	record;
	static long	addr;

	if( (ged_fd = open(filename, 0)) < 0 )  {
		perror(filename);
		return(-1);
	}

	(void)lseek( ged_fd, 0L, 0 );
	(void)read( ged_fd, (char *)&record, sizeof record );
	if( record.u_id != ID_IDENT )  {
		(void)fprintf(stderr,"Warning:  File is not a proper GED database\n");
		(void)fprintf(stderr,"This database should be converted before further use.\n");
	}
	(void)lseek( ged_fd, 0L, 0 );
	while(1)  {
		addr = lseek( ged_fd, 0L, 1 );
		if( (unsigned)read( ged_fd, (char *)&record, sizeof record )
				!= sizeof record )
			break;

		if( record.u_id == ID_IDENT )  {
			if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
				(void)fprintf(stderr,"File is Version %s, Program is version %s\n",
					record.i.i_version, ID_VERSION );
			}
			(void)fprintf(stderr,"%s (units=%s)\n",
				record.i.i_title,
				units_str[record.i.i_units] );
			continue;
		}
		if( record.u_id == ID_FREE )  {
			continue;
		}
		if( record.u_id == ID_ARS_A )  {
			dir_add( record.a.a_name, addr );

			/* Skip remaining B type records.	*/
			(void)lseek( ged_fd,
				(long)(record.a.a_totlen) *
				(long)(sizeof record),
				1 );
			continue;
		}

		if( record.u_id == ID_SOLID )  {
			dir_add( record.s.s_name, addr );
			continue;
		}
		if( record.u_id == ID_P_HEAD )  {
			union record rec;
			register int nrec;
			register int j;
			nrec = 1;
			while(1) {
				j = read( ged_fd, (char *)&rec, sizeof(rec) );
				if( j != sizeof(rec) )
					break;
				if( rec.u_id != ID_P_DATA )  {
					lseek( ged_fd, -(sizeof(rec)), 1 );
					break;
				}
				nrec++;
			}
			dir_add( record.p.p_name, addr );
			continue;
		}
		if( record.u_id != ID_COMB )  {
			(void)fprintf(stderr, "dir_build:  unknown record %c (0%o)\n",
				record.u_id, record.u_id );
			/* skip this record */
			continue;
		}

		dir_add( record.c.c_name, addr );
		/* Skip over member records */
		(void)lseek( ged_fd,
			(long)record.c.c_length * (long)sizeof record,
			1 );
	}
	return(0);	/* OK */
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
		fprintf(stderr, "dir_lookup:  could not find '%s'\n", str );
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
		rtbomb("strdup:  unable to allocate memory");

	current = base;
	do  {
		*current++ = *cp;
	}  while( *cp++ != '\0' );

	return(base);
}
