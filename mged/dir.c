/*
 *			D I R . C
 *
 * Functions -
 *	dir_build	Build directory of object file
 *	toc		Print table-of-contents of object file
 *	eraseobj	Drop an object from the visible list
 *	lookup		Convert an object name into directory pointer
 *	pr_solids	Print info about visible list
 *	dir_add		Add entry to the directory
 *	strdup		Duplicate a string in dynamic memory
 *	dir_delete	Delete entry from directory
 *	db_delete	Delete entry from database
 *	db_getrec	Get a record from database
 *	db_putrec	Put record to database
 */

#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include "ged_types.h"
#include "3d.h"
#include "ged.h"
#include "solid.h"
#include "dir.h"
#include "vmath.h"
#include "dm.h"

extern int	read();
extern long	lseek();
extern char	*malloc();

static struct directory *DirHead = DIR_NULL;

static long	objfdend;		/* End+1 position of object file */
int		objfd;			/* FD of object file */

union record	record;

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};
char	*filename;			/* Name of database file */

/*
 *  			D B _ O P E N
 */
db_open( name )
char *name;
{
	if( (objfd = open( name, O_RDWR )) < 0 )  {
		if( (objfd = open( name, O_RDONLY )) < 0 )  {
			perror( name );
			exit(2);		/* NOT finish */
		}
		(void)printf("%s: READ ONLY\n", name);
	}
	filename = name;
}

/*
 *			D I R _ B U I L D
 *
 * This routine reads through the 3d object file and
 * builds a directory of the object names, to allow rapid
 * named access to objects.
 */
void
dir_build()  {
	static long	addr;

	(void)lseek( objfd, 0L, 0 );
	(void)read( objfd, (char *)&record, sizeof record );
	if( record.u_id != ID_IDENT )  {
		(void)printf("Warning:  File is not a proper GED database\n");
		(void)printf("This database should be converted before further use.\n");
	}
	(void)lseek( objfd, 0L, 0 );
	while(1)  {
		addr = lseek( objfd, 0L, 1 );
		if( (unsigned)read( objfd, (char *)&record, sizeof record )
				!= sizeof record )
			break;

		if( record.u_id == ID_IDENT )  {
			if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
				(void)printf("File is Version %s, Program is version %s\n",
					record.i.i_version, ID_VERSION );
				finish(8);
			}
			(void)printf("%s (units=%s)\n",
				record.i.i_title,
				units_str[record.i.i_units] );
			continue;
		}
		if( record.u_id == ID_FREE )  {
			/* Ought to inform db manager of avail. space */
			continue;
		}
		if( record.u_id == ID_ARS_A )  {
			dir_add( record.a.a_name, addr, DIR_SOLID, record.a.a_totlen+1 );

			/* Skip remaining B type records.	*/
			(void)lseek( objfd,
				(long)(record.a.a_totlen) *
				(long)(sizeof record),
				1 );
			continue;
		}

		if( record.u_id == ID_SOLID )  {
			dir_add( record.s.s_name, addr, DIR_SOLID, 1 );
			continue;
		}
		if( record.u_id == ID_P_HEAD )  {
			union record rec;
			register int nrec;
			register int j;
			nrec = 1;
			while(1) {
				j = read( objfd, (char *)&rec, sizeof(rec) );
				if( j != sizeof(rec) )
					break;
				if( rec.u_id != ID_P_DATA )  {
					lseek( objfd, -(sizeof(rec)), 1 );
					break;
				}
				nrec++;
			}
			dir_add( record.p.p_name, addr, DIR_SOLID, nrec );
			continue;
		}
		if( record.u_id != ID_COMB )  {
			(void)printf( "dir_build:  unknown record %c (0%o)\n",
				record.u_id, record.u_id );
			/* skip this record */
			continue;
		}

		dir_add( record.c.c_name,
			addr,
			record.c.c_flags == 'R' ?
				DIR_COMB|DIR_REGION : DIR_COMB,
			record.c.c_length+1 );
		/* Skip over member records */
		(void)lseek( objfd,
			(long)record.c.c_length * (long)sizeof record,
			1 );
	}

	/* Record current end of objects file */
	objfdend = lseek( objfd, 0L, 1 );
}

/*
 *			D I R _ P R I N T
 *
 * This routine lists the names of all the objects accessible
 * in the object file.
 */
void
dir_print()  {
	struct directory	*dp;
	register char	*cp;		/* -> name char to output */
	register int	count;		/* names listed on current line */
	register int	len;		/* length of previous name */

#define	COLUMNS	((80 + NAMESIZE - 1) / NAMESIZE)

	count = 0;
	len = 0;
	for( dp = DirHead; dp != DIR_NULL; dp = dp->d_forw )  {
		if ( (cp = dp->d_namep)[0] == '\0' )
			continue;	/* empty slot */

		/* Tab to start column. */
		if ( count != 0 )
			do
				(void)putchar( '\t' );
			while ( (len += 8) < NAMESIZE );

		/* Output name and save length for next tab. */
		len = 0;
		do {
			(void)putchar( *cp++ );
			++len;
		}  while ( *cp != '\0' );
		if( dp->d_flags & DIR_COMB )  {
			(void)putchar( '/' );
			++len;
		}
		if( dp->d_flags & DIR_REGION )  {
			(void)putchar( 'R' );
			++len;
		}

		/* Output newline if last column printed. */
		if ( ++count == COLUMNS )  {	/* line now full */
			(void)putchar( '\n' );
			count = 0;
		}
	}
	/* No more names. */
	if ( count != 0 )		/* partial line */
		(void)putchar( '\n' );
#undef	COLUMNS
}

/*
 *			E R A S E O B J
 *
 * This routine goes through the solid table and deletes all displays
 * which contain the specified object in their 'path'
 */
void
eraseobj( dp )
register struct directory *dp;
{
	register struct solid *sp;
	static struct solid *nsp;
	register int i;

	sp=HeadSolid.s_forw;
	while( sp != &HeadSolid )  {
		nsp = sp->s_forw;
		for( i=0; i<=sp->s_last; i++ )  {
			if( sp->s_path[i] == dp )  {
				memfree( &(dmp->dmr_map), sp->s_addr, sp->s_bytes );
				DEQUEUE_SOLID( sp );
				FREE_SOLID( sp );
				break;
			}
		}
		sp = nsp;
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
lookup( str, noisy )
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
 *			P R _ S O L I D S
 *
 *  Given a pointer to a member of the circularly linked list of solids
 *  (typically the head), chase the list and print out the information
 *  about each solid structure.
 */
void
pr_solids( startp )
struct solid *startp;
{
	register struct solid *sp;
	register int i;

	sp = startp->s_forw;
	while( sp != startp )  {
		for( i=0; i <= sp->s_last; i++ )
			(void)printf("/%s", sp->s_path[i]->d_namep);
		(void)printf("  %s", sp->s_flag == UP ? "VIEW":"-NO-" );
		if( sp->s_iflag == UP )
			(void)printf(" ILL");
		(void)printf(" [%f,%f,%f] size %f",
			sp->s_center[X], sp->s_center[Y], sp->s_center[Z],
			sp->s_size);
		(void)putchar('\n');
		sp = sp->s_forw;
	}
}

/*
 *			D I R _ A D D
 *
 * Add an entry to the directory
 */
struct directory *
dir_add( name, laddr, flags, len )
register char *name;
long laddr;
{
	register struct directory *dp;

	GETSTRUCT( dp, directory );
	if( dp == DIR_NULL )
		return( DIR_NULL );
	dp->d_namep = strdup( name );
	dp->d_addr = laddr;
	dp->d_flags = flags;
	dp->d_len = len;
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
char *
strdup( cp )
register char *cp;
{
	register char	*base;
	register char	*current;

	if( (base = malloc( strlen(cp)+1 )) == (char *)0 )  {
		(void)printf("strdup:  unable to allocate memory");
		return( (char *) 0);
	}

	current = base;
	do  {
		*current++ = *cp;
	}  while( *cp++ != '\0' );

	return(base);
}

/*
 *  			D I R _ D E L E T E
 *
 *  Given a pointer to a directory entry, remove it from the
 *  linked list, and free the associated memory.
 */
void
dir_delete( dp )
register struct directory *dp;
{
	register struct directory *findp;

printf("dir_delete(%s)\n", dp->d_namep);
	if( DirHead == dp )  {
		free( dp->d_namep );
		DirHead = dp->d_forw;
		free( dp );
		return;
	}
	for( findp = DirHead; findp != DIR_NULL; findp = findp->d_forw )  {
		if( findp->d_forw != dp )
			continue;
		free( dp->d_namep );
		findp->d_forw = dp->d_forw;
		free( dp );
		return;
	}
	printf("dir_delete:  unable to find %s\n", dp->d_namep );
}

/*
 *  			D B _ D E L E T E
 *  
 *  Delete the indicated database record(s).
 *  Mark all records with ID_FREE.
 */
static union record zapper;

void
db_delete( dp )
struct directory *dp;
{
	register int i;

printf("db_delete(%s) len=%d\n", dp->d_namep, dp->d_len);
	zapper.u_id = ID_FREE;	/* The rest will be zeros */

	for( i=0; i < dp->d_len; i++ )
		db_putrec( dp, &zapper, i );
	dp->d_len = 0;
	dp->d_addr = -1;
}

/*
 *  			D B _ G E T R E C
 *
 *  Retrieve a record from the database,
 *  "offset" granules into this entry.
 */
void
db_getrec( dp, where, offset )
struct directory *dp;
union record *where;
{
	register int i;

	if( offset < 0 || offset >= dp->d_len )  {
		(void)printf("db_getrec(%s):  offset %d exceeds %d\n",
			dp->d_namep, offset, dp->d_len );
		where->u_id = '\0';	/* undefined id */
		return;
	}
	(void)lseek( objfd, dp->d_addr + offset * sizeof(union record), 0 );
	i = read( objfd, (char *)where, sizeof(union record) );
	if( i != sizeof(union record) )  {
		perror("db_getrec");
		printf("db_getrec(%s):  read error.  Wanted %d, got %d\n",
			dp->d_namep, sizeof(union record), i );
		where->u_id = '\0';	/* undefined id */
	}
}

/*
 *  			D B _ P U T R E C
 *
 *  Store a single record in the database,
 *  "offset" granules into this entry.
 */
void
db_putrec( dp, where, offset )
register struct directory *dp;
union record *where;
int offset;
{
	register int i;

	if( offset < 0 || offset >= dp->d_len )  {
		(void)printf("db_putrec(%s):  offset %d exceeds %d\n",
			dp->d_namep, offset, dp->d_len );
		return;
	}
	(void)lseek( objfd, dp->d_addr + offset * sizeof(union record), 0 );
	i = write( objfd, (char *)where, sizeof(union record) );
	if( i != sizeof(union record) )  {
		perror("db_putrec");
		printf("db_putrec(%s):  write error\n", dp->d_namep );
	}
}

/*
 *  			D B _ A L L O C
 *  
 *  Find a block of database storage of "count" granules.
 */
void
db_alloc( dp, count )
register struct directory *dp;
int count;
{
	/* For now, a dumb algorithm. */
	dp->d_addr = objfdend;
	dp->d_len = count;
	objfdend += count * sizeof(union record);
}

/*
 *  			D B _ G R O W
 *  
 *  Increase the database size of an object by "count",
 *  by duplicating in a new area if necessary.
 */
db_grow( dp, count )
register struct directory *dp;
int count;
{
	register int i;
	union record rec;
	struct directory olddir;

	/* Easy case -- see if at end-of-file */
	if( dp->d_addr + dp->d_len * sizeof(union record) == objfdend )  {
		objfdend += count * sizeof(union record);
		dp->d_len += count;
		return;
	}
printf("db_grow(%s, %d) ", dp->d_namep, count );
	/* Next, check to see if granules are all availible */
	for( i=0; i < count; i++ )  {
		(void)lseek( objfd, dp->d_addr +
			((dp->d_len + i) * sizeof(union record)), 0 );
		(void)read( objfd, (char *)&rec, sizeof(union record) );
		if( rec.u_id != ID_FREE )
			goto hard;
	}
printf("easy -- following records were free\n");
	dp->d_len += count;
	return;
hard:
	/* Sigh, got to duplicate it in some new space */
	olddir = *dp;				/* struct copy */
printf("hard -- make copy\n");
	db_alloc( dp, dp->d_len + count );	/* fixes addr & len */
	for( i=0; i < olddir.d_len; i++ )  {
		db_getrec( &olddir, &rec, i );
		db_putrec( dp, &rec, i );
	}
	/* Release space that original copy consumed */
	db_delete( &olddir );
}

/*
 *  			D B _ T R U N C
 *  
 *  Remove "count" granules from the indicated database entry.
 *  Stomp on them with ID_FREE's.
 *  Later, we will add them to a freelist.
 */
void
db_trunc( dp, count )
register struct directory *dp;
int count;
{
	register int i;

printf("db_trunc(%s, %d)\n", dp->d_namep, count );
	zapper.u_id = ID_FREE;	/* The rest will be zeros */

	for( i = 0; i < count; i++ )
		db_putrec( dp, &zapper, (dp->d_len - 1) - i );
	dp->d_len -= count;
}
