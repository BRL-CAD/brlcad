/*
 *			D I R . C
 *
 * Functions -
 *	db_open		Open the database
 *	dir_build	Build directory of object file
 *	dir_getspace	Allocate memory for table of directory entry pointers
 *	dir_print	Print table-of-contents of object file
 *	lookup		Convert an object name into directory pointer
 *	dir_add		Add entry to the directory
 *	strdup		Duplicate a string in dynamic memory
 *	dir_delete	Delete entry from directory
 *	db_delete	Delete entry from database
 *	db_getrec	Get a record from database
 *	db_getmany	Retrieve several records from database
 *	db_putrec	Put record to database
 *	db_alloc	Find a contiguous block of database storage
 *	db_grow		Increase size of existing database entry
 *	db_trunc	Decrease size of existing entry, from it's end
 *	db_delrec	Delete a specific record from database entry
 *	f_memprint	Debug, print memory & db free maps
 *	conversions	Builds conversion factors given a local unit
 *	dir_units	Changes the local unit
 *	dir_title	Change the target title
 *	dir_nref	Count number of times each db element referenced
 *	regexp_match	Does regular exp match given string?
 *	dir_summary	Summarize contents of directory by categories
 *	f_tops		Prints top level items in database
 *	cmd_glob	Does regular expression expansion on cmd_args[]
 *	f_prefix	Prefix each occurence of a specified object name
 *	f_keep		Save named objects in specified file
 *	f_tree		Print out a tree of all members of an object
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A. Applin
 *	Richard Romanelli
 *	Robert Jon Reschly Jr.
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#ifdef BSD
#include <strings.h>
#else
#include <string.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "./ged.h"
#include "./solid.h"
#include "./objdir.h"
#include "./dm.h"

extern int	read(), strcmp();
extern long	lseek();
extern char	*malloc();
extern void	exit(), free(), perror();

/*
 *  The directory is organized as forward linked lists hanging off of
 *  one of NHASH headers.  The function dir_hash() returns a pointer
 *  to the correct header.
 */
#define	NHASH		128	/* size of hash table */

#if	((NHASH)&((NHASH)-1)) != 0
#define	HASH(sum)	((unsigned)(sum) % (NHASH))
#else
#define	HASH(sum)	((unsigned)(sum) & ((NHASH)-1))
#endif

static struct directory *DirHead[NHASH];

static struct mem_map *dbfreep = MAP_NULL; /* map of free granules */

#define BAD_EOF	(-1L)			/* eof_addr not set yet */
static long	eof_addr = BAD_EOF;	/* End+1 position of object file */
int		objfd;			/* FD of object file */

union record	record;
static union record zapper;		/* Zeros, for erasing records */

double	local2base, base2local;		/* unit conversion factors */
int	localunit;			/* local unit currently in effect */
static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

char	cur_title[128];			/* current target title */

char	*filename;			/* Name of database file */
int	read_only = 0;			/* non-zero when read-only */
void	conversions();
void	rm_membs();
void	killtree();

static void file_put();
static void printnode();

extern void color_addrec();
extern int numargs, maxargs;		/* defined in cmd.c */
extern char *cmd_args[];		/* defined in cmd.c */

/*
 *  			D B _ O P E N
 *
 *  Returns:
 *	-1	error
 *	+1	success
 */
int
db_open( name )
char *name;
{
	if( (objfd = open( name, O_RDWR )) < 0 )  {
		if( (objfd = open( name, O_RDONLY )) < 0 )  {
			perror( name );
			return(-1);
		}  else  {
			(void)printf("%s: READ ONLY\n", name);
			read_only = 1;
		}
	}
	filename = name;
	return(1);
}

/*
 *			D B _ C R E A T E
 *
 *  Create a new database containing just an IDENT record
 *  Returns:
 *	-1 on error,
 *	+1 on success.
 */
int
db_create( name )
char *name;
{
	union record new;

	if( (objfd = creat(name, 0644)) < 0 )  {
		perror(name);
		return(-1);
	}
	bzero( (char *)&new, sizeof(new) );
	new.i.i_id = ID_IDENT;
	new.i.i_units = ID_MM_UNIT;
	strncpy( new.i.i_version, ID_VERSION, sizeof(new.i.i_version) );
	strcpy( new.i.i_title, "Untitled MGED Database" );
	(void)write( objfd, (char *)&new, sizeof(new) );
	(void)close(objfd);

	if( db_open( name ) < 0 )
		return(-1);
	return(1);
}

/*
 *			D I R _ H A S H
 *  
 *  Internal function to return pointer to head of hash chain
 *  corresponding to the given string.
 */
static
struct directory **
dir_hash(str)
char *str;
{
	register unsigned char *s = (unsigned char *)str;
	register long sum;
	register int i;

	sum = 0;
	/* namei hashing starts i=0, discarding first char.  ??? */
	for( i=1; *s; )
		sum += *s++ * i++;

	return( &DirHead[HASH(sum)] );
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
	register FILE *fp;
	register long addr;

	(void)lseek( objfd, 0L, 0 );
	if( read( objfd, (char *)&record, sizeof record ) != sizeof record ) {
		(void)printf("dir_build:  database header read error\n");
		mged_finish(5);
		return;
	}
	if( record.u_id != ID_IDENT )  {
		(void)printf("ERROR:  %s looks nothing like a GED database\n",
			filename);
		mged_finish(6);
		return;
	}
	if( strcmp( record.i.i_version, ID_VERSION) != 0 )  {
		(void)printf("File is Version %s, Program is version %s\n",
			record.i.i_version, ID_VERSION );
		(void)printf("This database should be converted before further use.\n");
		localunit = 0;
		local2base = base2local = 1.0;
	} else {
		/* get the unit conversion factors */
		localunit = record.i.i_units;
		conversions( record.i.i_units );
	}
	/* save the title */
	cur_title[0] = '\0';
	(void)strcat(cur_title, record.i.i_title);

	if( (fp = fopen( filename, "r" )) == NULL )  {
		(void)printf("dir_build: fopen failed\n");
		return;
	}

	addr = -1;
	while(1)  {
		if( (addr = ftell(fp)) == EOF )
			printf("dir_build:  ftell() failure\n");
		if( fread( (char *)&record, sizeof record, 1, fp ) != 1
		    || feof(fp) )
			break;

	switch( record.u_id )  {

	case ID_COMB:
		(void)dir_add( record.c.c_name, addr,
			record.c.c_flags == 'R' ?
				DIR_COMB|DIR_REGION : DIR_COMB,
			record.c.c_length+1 );
		continue;

	case ID_MEMB:
		continue;

	case ID_ARS_A:
		(void)dir_add( record.a.a_name, addr,
			DIR_SOLID, record.a.a_totlen+1 );
		continue;

	case ID_ARS_B:
		continue;

	case ID_BSOLID:
		{
			static union record rec;
			register long start_addr;

			start_addr = addr;
			addr += (long)(sizeof(record));
			while( fread((char *)&rec, sizeof(rec), 1, fp) == 1 &&
			    !feof(fp)  &&
			    rec.u_id == ID_BSURF )  {
				addr += (rec.d.d_nknots+rec.d.d_nctls+1) *
					(long)(sizeof(record));
				(void)fseek( fp, addr, 0 );
			}
			(void)dir_add( record.B.B_name, start_addr,
				DIR_SOLID, (addr-start_addr)/sizeof(record) );
			(void)fseek( fp, addr, 0 );	/* to reread */
			continue;
		}

	case ID_BSURF:
		(void)printf("Unattached B-spline surface record?\n");
		/* Need to skip over knots & mesh which follows! */
		addr += (record.d.d_nknots + record.d.d_nctls + 1) *
			(long)(sizeof(record));
		(void)fseek( fp, addr, 0 );
		continue;

	case ID_P_HEAD:
		{
			union record rec;
			register int nrec;

			nrec = 1;
			while( fread((char *)&rec, sizeof(rec), 1, fp) == 1 &&
			    !feof(fp)  &&
			    rec.u_id == ID_P_DATA )
				nrec++;
			(void)dir_add( record.p.p_name, addr, DIR_SOLID, nrec );
			addr += (long)nrec * (long)sizeof(record);
			(void)fseek( fp, addr, 0 );
			continue;
		}

	case ID_IDENT:
		(void)printf("%s (units=%s)\n",
			record.i.i_title,
			units_str[record.i.i_units] );
		continue;

	case ID_FREE:
		/* Inform db manager of avail. space */
		memfree( &dbfreep, 1, addr/sizeof(union record) );
		continue;

	case ID_SOLID:
		(void)dir_add( record.s.s_name, addr, DIR_SOLID, 1 );
		continue;

	case ID_MATERIAL:
		color_addrec( &record, addr );
		continue;

	default:
		(void)printf( "dir_build:  unknown record %d=%c (0%o) addr=x%x\n",
			addr/sizeof(union record),
			record.u_id, record.u_id,
			addr );
#ifdef CLEANUP_DB
		if( !read_only )  {
			/* zap this record and put in free map */
			zapper.u_id = ID_FREE;	/* The rest will be zeros */
			(void)lseek( objfd, addr, 0 );
			(void)write(objfd, (char *)&zapper, sizeof(zapper));
		}
		memfree( &dbfreep, 1, addr/(sizeof(union record)) );
#endif
		continue;
	}
	}

	/* Record current end of objects file */
	eof_addr = addr;
	(void)fclose(fp);
}

/*
 *			D I R _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 *  a) all of the entries if called with an argument of 0, or
 *  b) the number of entries specified by the argument if > 0.
 */
struct directory **
dir_getspace( num_entries)
register int num_entries;
{
	register struct directory *dp;
	register int i;
	register struct directory **dir_basep;

	if( num_entries < 0) {
		(void) printf( "dir_getspace: was passed %d, used 0\n",
		  num_entries);
		num_entries = 0;
	}
	if( num_entries == 0)  {
		/* Set num_entries to the number of entries */
		for( i = 0; i < NHASH; i++)
			for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw)
				num_entries++;
	}

	/* Allocate and cast num_entries worth of pointers */
	if( (dir_basep = (struct directory **) malloc( num_entries *
	  sizeof(dp))) == (struct directory **) 0) {
	  	(void) printf( "dir_getspace:  unable to allocate memory");
	}
	return(dir_basep);
}

/*
 *			D I R _ P R I N T
 *
 * This routine lists the names of all the objects accessible
 * in the object file.
 */
void
dir_print() {
	register struct directory *dp;
	register int i;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2);	/* allow interupts */

	/* Get some memory */
	if( (dirp = dir_getspace( numargs - 1)) == (struct directory **) 0) {
	  	(void) printf( "dir_print:  unable to get memory");
	  	return;
	}
	dirp0 = dirp;

	if( numargs > 1) {
		/* Just list specified names */
		/*
		 * Verify the names, and add pointers to them to the array.
		 */
		for( i = 1; i < numargs; i++ )  {
			if( (dp = lookup( cmd_args[i], LOOKUP_NOISY)) ==
			  DIR_NULL )
				continue;
			*dirp++ = dp;
		}
	} else {
		/* Full table of contents */
		/*
		 * Walk the directory list adding pointers (to the directory
		 * entries) to the array.
		 */
		for( i = 0; i < NHASH; i++)
			for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw)
				*dirp++ = dp;
	}
	col_pr4v( dirp0, (int)(dirp - dirp0));
	free( dirp0);
}

/*
 *			D I R _ L O O K U P
 *
 * This routine takes a name, trims to NAMESIZE, and looks it up in the
 * directory table.  If the name is present, a pointer to
 * the directory struct element is returned, otherwise
 * NULL is returned.
 *
 * If noisy is non-zero, a print occurs, else only
 * the return code indicates failure.
 */
struct directory *
lookup( name, noisy )
register char *name;
{
	register struct directory *dp;
	static char local[NAMESIZE+2];
	register int i;

	if( (i=strlen(name)) > NAMESIZE )  {
		(void)strncpy( local, name, NAMESIZE );	/* Trim the name */
		local[NAMESIZE] = '\0';			/* ensure null termination */
		name = local;
	}
	for( dp = *dir_hash(name); dp != DIR_NULL; dp=dp->d_forw )  {
		if(
			name[0] == dp->d_namep[0]  &&	/* speed */
			name[1] == dp->d_namep[1]  &&	/* speed */
			strcmp( name, dp->d_namep ) == 0
		)
			return(dp);
	}

	if( noisy )
		(void)printf("dir_lookup:  could not find '%s'\n", name );
	return( DIR_NULL );
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
	register struct directory **headp;
	register struct directory *dp;
	char local[NAMESIZE+2];

	GETSTRUCT( dp, directory );
	if( dp == DIR_NULL )
		return( DIR_NULL );
	(void)strncpy( local, name, NAMESIZE );	/* Trim the name */
	local[NAMESIZE] = '\0';			/* ensure null termination */
	dp->d_namep = strdup( local );
	dp->d_addr = laddr;
	dp->d_flags = flags;
	dp->d_len = len;
	headp = dir_hash( local );
	dp->d_forw = *headp;
	*headp = dp;
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

	if( (base = malloc( (unsigned)(strlen(cp)+1) )) == (char *)0 )  {
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
	register struct directory **headp;

	headp = dir_hash( dp->d_namep );
	if( *headp == dp )  {
		free( dp->d_namep );
		*headp = dp->d_forw;
		free( dp );
		return;
	}
	for( findp = *headp; findp != DIR_NULL; findp = findp->d_forw )  {
		if( findp->d_forw != dp )
			continue;
		free( dp->d_namep );
		findp->d_forw = dp->d_forw;
		free( dp );
		return;
	}
	(void)printf("dir_delete:  unable to find %s\n", dp->d_namep );
}

/*
 *  			D B _ D E L E T E
 *  
 *  Delete the indicated database record(s).
 *  Mark all records with ID_FREE.
 */
void
db_delete( dp )
struct directory *dp;
{
	register int i;

	zapper.u_id = ID_FREE;	/* The rest will be zeros */

	for( i=0; i < dp->d_len; i++ )
		db_putrec( dp, &zapper, i );
	memfree( &dbfreep, (unsigned)dp->d_len, dp->d_addr/(sizeof(union record)) );
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
	(void)lseek( objfd, (long)(dp->d_addr + offset * sizeof(union record)), 0 );
	i = read( objfd, (char *)where, sizeof(union record) );
	if( i != sizeof(union record) )  {
		perror("db_getrec");
		(void)printf("db_getrec(%s,%d):  read error.  Wanted %d, got %d\n",
			dp->d_namep, offset, sizeof(union record), i );
		where->u_id = '\0';	/* undefined id */
	}
}

/*
 *  			D B _ G E T M R E C
 *
 *  Retrieve all records in the database pertaining to an object,
 *  and place them in malloc()'ed storage, which the caller is
 *  responsible for free()'ing.
 */
union record *
db_getmrec( dp )
struct directory *dp;
{
	union record *where;
	int	want;
	int	got;

	want = dp->d_len * sizeof(union record);
	if( (where = (union record *)malloc(want)) == (union record *)0 )  {
		perror("db_getmrec malloc");
		return( (union record *)0 );	/* VERY BAD */
	}
	(void)lseek( objfd, (long)(dp->d_addr), 0 );
	got = read( objfd, (char *)where, want );
	if( got != want )  {
		perror("db_getmrec");
		(void)printf("db_getmrec(%s):  read error.  Wanted %d, got %d bytes\n",
			dp->d_namep, want, got );
		free( (char *)where );
		return( (union record *)0 );	/* VERY BAD */
	}
	return( where );
}

/*
 *  			D B _ G E T M A N Y
 *
 *  Retrieve several records from the database,
 *  "offset" granules into this entry.
 */
void
db_getmany( dp, where, offset, len )
struct directory *dp;
union record *where;
{
	register int i;

	if( offset < 0 || offset+len > dp->d_len )  {
		(void)printf("db_getmany(%s):  xfer %d..%x exceeds 0..%d\n",
			dp->d_namep, offset, offset+len, dp->d_len );
		where->u_id = '\0';	/* undefined id */
		return;
	}
	(void)lseek( objfd, (long)(dp->d_addr + offset * sizeof(union record)), 0 );
	i = read( objfd, (char *)where, len * sizeof(union record) );
	if( i != len * sizeof(union record) )  {
		perror("db_getmany");
		(void)printf("db_getmany(%s):  read error.  Wanted %d, got %d\n",
			dp->d_namep, len * sizeof(union record), i );
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

	if( read_only )  {
		(void)printf("db_putrec on READ-ONLY file\n");
		return;
	}
	if( offset < 0 || offset >= dp->d_len )  {
		(void)printf("db_putrec(%s):  offset %d exceeds %d\n",
			dp->d_namep, offset, dp->d_len );
		return;
	}
	(void)lseek( objfd, (long)(dp->d_addr + offset * sizeof(union record)), 0 );
	i = write( objfd, (char *)where, sizeof(union record) );
	if( i != sizeof(union record) )  {
		perror("db_putrec");
		(void)printf("db_putrec(%s):  write error\n", dp->d_namep );
	}
}

/*
 *  			D B _ P U T M A N Y
 *
 *  Store several records to the database,
 *  "offset" granules into this entry.
 */
void
db_putmany( dp, where, offset, len )
struct directory *dp;
union record *where;
{
	register int i;

	if( read_only )  {
		(void)printf("db_putrec on READ-ONLY file\n");
		return;
	}
	if( offset < 0 || offset+len > dp->d_len )  {
		(void)printf("db_putmany(%s):  xfer %d..%x exceeds 0..%d\n",
			dp->d_namep, offset, offset+len, dp->d_len );
		return;
	}
	(void)lseek( objfd, (long)(dp->d_addr + offset * sizeof(union record)), 0 );
	i = write( objfd, (char *)where, len * sizeof(union record) );
	if( i != len * sizeof(union record) )  {
		perror("db_putmany");
		(void)printf("db_putmany(%s):  write error.  Wanted %d, got %d\n",
			dp->d_namep, len * sizeof(union record), i );
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
	register int i;
	unsigned long addr;
	union record rec;

	if( read_only )  {
		(void)printf("db_alloc on READ-ONLY file\n");
		return;
	}
	if( count <= 0 )  {
		(void)printf("db_alloc(0)\n");
		return;
	}
top:
	if( (addr = memalloc( &dbfreep, (unsigned)count )) == 0L )  {
		/* No contiguous free block, append to file */
		if( (dp->d_addr = eof_addr) == BAD_EOF )  {
			(void)printf("db_alloc while reading database?\n");
			return;
		}
		dp->d_len = count;
		eof_addr += count * sizeof(union record);

		/* Clear out the granules, for safety */
		zapper.u_id = ID_FREE;	/* The rest will be zeros */
		for( i=0; i < dp->d_len; i++ )
			db_putrec( dp, &zapper, i );
		return;
	}
	dp->d_addr = addr * sizeof(union record);
	dp->d_len = count;
	db_getrec( dp, &rec, 0 );
	if( rec.u_id != ID_FREE )  {
		(void)printf("db_alloc():  addr %ld non-FREE (id %d), skipping\n",
			addr, rec.u_id );
		goto top;
	}

	/* Clear out the granules, for safety */
	zapper.u_id = ID_FREE;	/* The rest will be zeros */
	for( i=0; i < dp->d_len; i++ )
		db_putrec( dp, &zapper, i );
}

/*
 *  			D B _ G R O W
 *  
 *  Increase the database size of an object by "count",
 *  by duplicating in a new area if necessary.
 *  Returns:
 *	-1	on error
 *	0	on success
 */
int
db_grow( dp, count )
register struct directory *dp;
int count;
{
	register int i;
	union record rec;
	struct directory olddir;
	int extra_start;

	if( read_only )  {
		(void)printf("db_grow on READ-ONLY file\n");
		return(-1);
	}

	/* Easy case -- see if at end-of-file */
	extra_start = dp->d_addr + dp->d_len * sizeof(union record);
	if( extra_start == eof_addr )  {
		eof_addr += count * sizeof(union record);
		dp->d_len += count;
clean:
		(void)lseek( objfd, extra_start, 0 );
		zapper.u_id = ID_FREE;	/* The rest will be zeros */
		for( i = 0; i < count; i++ )  {
			if( write( objfd, (char *)&zapper, sizeof(zapper) ) != sizeof(zapper) )  {
				perror("db_grow: write");
				return(-1);
			}
		}
		return(0);
	}

	/* Try to extend into free space immediately following current obj */
	if( memget( &dbfreep, (unsigned)count, (unsigned long)(dp->d_addr/sizeof(union record)) ) == 0L )
		goto hard;

	/* Check to see if granules are all really availible (sanity check) */
	for( i=0; i < count; i++ )  {
		(void)lseek( objfd, (long)(dp->d_addr +
			((dp->d_len + i) * sizeof(union record))), 0 );
		(void)read( objfd, (char *)&rec, sizeof(union record) );
		if( rec.u_id != ID_FREE )  {
			(void)printf("db_grow:  FREE record wasn't?! (id%d)\n",
				rec.u_id);
			goto hard;
		}
	}
	dp->d_len += count;
	goto clean;
hard:
	/* Sigh, got to duplicate it in some new space */
	olddir = *dp;				/* struct copy */
	db_alloc( dp, dp->d_len + count );	/* fixes addr & len */
	/* TODO:  malloc, db_getmany, db_putmany, free.  Whack. */
	for( i=0; i < olddir.d_len; i++ )  {
		db_getrec( &olddir, &rec, i );
		db_putrec( dp, &rec, i );
	}
	/* Release space that original copy consumed */
	db_delete( &olddir );
	return(0);
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

	if( read_only )  {
		(void)printf("db_trunc on READ-ONLY file\n");
		return;
	}
	zapper.u_id = ID_FREE;	/* The rest will be zeros */

	for( i = 0; i < count; i++ )
		db_putrec( dp, &zapper, (dp->d_len - 1) - i );
	dp->d_len -= count;
}

/*
 *			D B _ D E L R E C
 *
 *  Delete a specific record from database entry
 */
void
db_delrec( dp, recnum )
register struct directory *dp;
int recnum;
{
	register int i;
	auto union record rec;

	if( read_only )  {
		(void)printf("db_delrec on READ-ONLY file\n");
		return;
	}
	/* If deleting last member, just truncate */
	if( recnum == dp->d_len-1 )  {
		db_trunc( dp, 1 );
		return;
	}

	/* "Ripple up" the rest of the entry */
	for( i = recnum+1; i < dp->d_len; i++ )  {
		db_getrec( dp, &rec, i );
		db_putrec( dp, &rec, i-1 );
	}
	db_trunc( dp, 1 );
}

/*
 *			F _ M E M P R I N T
 *  
 *  Debugging aid:  dump memory maps
 */
void
f_memprint()
{
	(void)printf("Display manager free map:\n");
	memprint( &(dmp->dmr_map) );
	(void)printf("Database free granule map:\n");
	memprint( &dbfreep );
}



/*	builds conversion factors given the local unit
 */
void
conversions( local )
int local;
{

	/* Base unit is MM */
	switch( local ) {

	case ID_NO_UNIT:
		/* no local unit specified ... use the base unit */
		localunit = record.i.i_units = ID_MM_UNIT;
		local2base = 1.0;
		break;

	case ID_MM_UNIT:
		/* local unit is mm */
		local2base = 1.0;
		break;

	case ID_CM_UNIT:
		/* local unit is cm */
		local2base = 10.0;		/* CM to MM */
		break;

	case ID_M_UNIT:
		/* local unit is meters */
		local2base = 1000.0;		/* M to MM */
		break;

	case ID_IN_UNIT:
		/* local unit is inches */
		local2base = 25.4;		/* IN to MM */
		break;

	case ID_FT_UNIT:
		/* local unit is feet */
		local2base = 304.8;		/* FT to MM */
		break;

	default:
		local2base = 1.0;
		localunit = 6;
		break;
	}
	base2local = 1.0 / local2base;
}

/* change the local unit of the description */
void
dir_units( new_unit )
int new_unit;
{
	conversions( new_unit );	/* Change local unit first */

	if( read_only ) {
		(void)printf("Read only file\n");
		return;
	}

	(void)lseek(objfd, 0L, 0);
	(void)read(objfd, (char *)&record, sizeof record);

	if(record.u_id != ID_IDENT) {
		(void)printf("NOT a proper GED file\n");
		return;
	}

	(void)lseek(objfd, 0L, 0);
	record.i.i_units = new_unit;
	(void)write(objfd, (char *)&record, sizeof record);
}

/* change the title of the description */
void
dir_title( )
{

	if( read_only ) {
		(void)printf("Read only file\n");
		return;
	}

	(void)lseek(objfd, 0L, 0);
	(void)read(objfd, (char *)&record, sizeof record);

	if(record.u_id != ID_IDENT) {
		(void)printf("NOT a proper GED file\n");
		return;
	}

	(void)lseek(objfd, 0L, 0);

	record.i.i_title[0] = '\0';

	(void)strcat(record.i.i_title, cur_title);

	(void)write(objfd, (char *)&record, sizeof record);
}

/*
 *			D I R _ N R E F
 *
 * Count the number of time each directory member is referenced
 * by a COMBination record.
 */
void
dir_nref( )
{
	register struct directory *dp;
	register FILE *fp;
	register int i;

	/* First, clear any existing counts */
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_nref = 0;
	}

	/* Read through the whole database, looking only at MEMBER records */
	if( (fp = fopen( filename, "r" )) == NULL )  {
		(void)printf("dir_nref: fopen failed\n");
		return;
	}
	while(fread( (char *)&record, sizeof(record), 1, fp ) == 1 &&
	     !feof(fp))  {
		if( record.u_id != ID_MEMB )
			continue;
		if( record.M.m_instname[0] != '\0' &&
		    (dp = lookup(record.M.m_instname, LOOKUP_QUIET)) != DIR_NULL )
			dp->d_nref++;
	}
	(void)fclose(fp);
}

/*
 *			R E G E X P _ M A T C H
 *
 *	If string matches pattern, return 1, else return 0
 *
 *	special characters:
 *		*	Matches any string including the null string.
 *		?	Matches any single character.
 *		[...]	Matches any one of the characters enclosed.
 *		-	May be used inside brackets to specify range
 *			(i.e. str[1-58] matches str1, str2, ... str5, str8)
 *		\	Escapes special characters.
 */
int
regexp_match(	 pattern,  string )
register char	*pattern, *string;
{
	do {
		switch( *pattern ) {
		case '*': /*
			   * match any string including null string
			   */
			++pattern;
			do {
				if( regexp_match( pattern, string ) )
					 return( 1 );
			} while( *string++ != '\0' );
			return( 0 );
		case '?': /*
			   * match any character
			   */
			if( *string == '\0' )	return( 0 );
			break;
		case '[': /*
			   * try to match one of the characters in brackets
			   */
			++pattern;
			while( *pattern != *string ) {
				if(	pattern[ 0] == '-'
				    &&	pattern[-1] != '\\'
				)	if(	pattern[-1] <= *string
					    &&	pattern[-1] != '['
					    &&	pattern[ 1] >= *string
					    &&	pattern[ 1] != ']'
					)	break;
				if( *++pattern == ']' )	return( 0 );
			}

			/* skip to next character after closing bracket
			 */
			while( *++pattern != ']' );
			break;
		case '\\': /*
			    * escape special character
			    */
			++pattern;
			/* WARNING: falls through to default case */
		default:  /*
			   * compare characters
			   */
			if( *pattern != *string )	return( 0 );
			break;
		}
		++string;
	} while( *pattern++ != '\0' );
	return( 1 );
}

/*
 *  			D I R _ S U M M A R Y
 *
 * Summarize the contents of the directory by categories
 * (solid, comb, region).  If flag is != 0, it is interpreted
 * as a request to print all the names in that category (eg, DIR_SOLID).
 */
void
dir_summary(flag)
{
	register struct directory *dp;
	register int i;
	static int sol, comb, reg, br;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	sol = comb = reg = br = 0;
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( dp->d_flags & DIR_SOLID )
				sol++;
			if( dp->d_flags & DIR_COMB )
				if( dp->d_flags & DIR_REGION )
					reg++;
				else
					comb++;
			if( dp->d_flags & DIR_BRANCH )
				br++;
		}
	}
	(void)printf("Summary:\n");
	(void)printf("  %5d solids\n", sol);
	(void)printf("  %5d region; %d non-region combinations\n", reg, comb);
	(void)printf("  %5d branch names\n", br);
	(void)printf("  %5d total objects\n\n", sol+reg+comb );

	if( flag == 0 )
		return;
	/* Print all names matching the flags parameter */
	/* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */
	
	if( (dirp = dir_getspace(0)) == (struct directory **) 0) {
	  	(void) printf( "dir_summary:  unable to get memory");
	  	return;
	}
	dirp0 = dirp;
	/*
	 * Walk the directory list adding pointers (to the directory entries
	 * of interest) to the array
	 */
	for( i = 0; i < NHASH; i++)
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw)
			if( dp->d_flags & flag )
				*dirp++ = dp;
	col_pr4v( dirp0, (int)(dirp - dirp0));
	free( dirp0);
}

/*
 *  			F _ T O P S
 *  
 *  Find all top level objects.
 *  TODO:  Perhaps print all objects, sorted by use count, as an option?
 */
void
f_tops()
{
	register struct directory *dp;
	register int i;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	dir_nref();
	/*
	 * Find number of possible entries and allocate memory
	 */
	if( (dirp = dir_getspace(0)) == (struct directory **) 0) {
	  	(void) printf( "f_tops:  unable to get memory");
	  	return;
	}
	dirp0 = dirp;
	/*
	 * Walk the directory list adding pointers (to the directory entries
	 * which are the tops of their respective trees) to the array
	 */
	for( i = 0; i < NHASH; i++)
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw)
			if( dp->d_nref > 0) {
				/* Object not member of any combination */
				continue;
			} else {
				*dirp++ = dp;
			}
	col_pr4v( dirp0, (int)(dirp - dirp0));
	free( dirp0);
}

/*
 *			C M D _ G L O B
 *  
 *  Assist routine for command processor.  If the current word
 *  in the cmd_args[] array contains "*", "?" or "[", then this
 *  word is potentially a regular expression, and we will tromp
 *  through the entire in-core directory searching for a match.
 *  If no match is found, the original word remains untouched
 *  and this routine was an expensive no-op.  If any match is
 *  found, it replaces the original word.  All matches are sought
 *  for, up to the limit of the cmd_args[] array.
 *
 *  Returns 0 if no expansion happened, !0 if we matched something.
 */
int
cmd_glob()
{
	static char word[64];
	register char *pattern;
	register struct directory	*dp;
	register int i;
	int orig_numargs = numargs;

	strncpy( word, cmd_args[numargs-1], sizeof(word)-1 );
	/* If * ? or [ are present, this is a regular expression */
	pattern = word;
	while( *pattern )  {
		if( *pattern == '\n' ||
		    *pattern == ' '  ||
		    *pattern == '\t' )
			return(0);		/* nothing to do */
		if( *pattern == '*' ||
		    *pattern == '?' ||
		    *pattern++ == '[' )
			goto hard;
	}
	return(0);				/* nothing to do */
hard:
	/* First, null terminate (sigh) */
	pattern = &word[-1];
	while( *++pattern )  {
		if( *pattern == '\n' ||
		    *pattern == ' '  ||
		    *pattern == '\t' )  {
			*pattern = '\0';
			break;
		    }
	}
			
	/* Search for pattern matches.
	 * First, save the pattern, and remove it from cmd_args,
	 * as it will be overwritten by the expansions.
	 */
	numargs--;
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !regexp_match( word, dp->d_namep ) )
				continue;
			/* Successful match */
			cmd_args[numargs++] = dp->d_namep;
			if( numargs >= maxargs )  {
				(void)printf("%s: expansion stopped after %d matches\n", word, maxargs);
				return(1);	/* limited success */
			}
		}
	}
	/* If we failed to do any expansion, leave the word untouched */
	if( numargs == orig_numargs-1 )  {
		(void)printf("%s: no match\n", word);
		numargs++;
		return(0);		/* found nothing */
	}
	return(1);			/* success */
}

/*
 *  			F _ F I N D
 *  
 *  Find all references to the named objects.
 */
void
f_find()
{
	register FILE *fp;
	register int i;
	char lastname[NAMESIZE];

	(void)signal( SIGINT, sig2 );	/* allow interupts */
	/* Read whole database, looking only at MEMBER + COMB records */
	if( (fp = fopen( filename, "r" )) == NULL )  {
		(void)printf("f_find: fopen failed\n");
		return;
	}
	while(fread( (char *)&record, sizeof(record), 1, fp ) == 1 &&
	     !feof(fp))  {
		if( record.u_id == ID_COMB )  {
			strncpy( lastname, record.c.c_name, NAMESIZE );
			continue;
		}
		if( record.u_id != ID_MEMB )
			continue;

		if( record.M.m_instname[0] == '\0' )
			continue;
		for( i=1; i < numargs; i++ )  {
			if( strncmp( record.M.m_instname, cmd_args[i], NAMESIZE ) != 0 )
				continue;
			(void)printf("%s:  member of %s\n",
				record.M.m_instname, lastname );
		}
	}
	(void)fclose(fp);
}

/*
 *			F _ P R E F I X
 *
 *  Prefix each occurence of a specified object name, both
 *  when defining the object, and when referencing it.
 */
void
f_prefix()
{
	register struct directory *dp;
	register FILE *fp;
	long	seekptr = 0;
	long	laddr;
	char	tempstring[NAMESIZE+2];
	int 	flags, len, i;

	if( (fp = fopen(filename, "r+")) == NULL) {
		(void) printf("f_prefix: fopen failed\n");
		return;
	}

	for( i = 2; i < numargs; i++) {
		if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) == DIR_NULL) 
			return;

		if( strlen(cmd_args[1]) + strlen(cmd_args[i]) > NAMESIZE) {
			printf("Prefix too long, names must be less than %d characters.\n",
				NAMESIZE);
			return;
		}

		(void) strcpy( tempstring, cmd_args[1]);
		(void) strcat( tempstring, cmd_args[i]);

		if( lookup( tempstring, LOOKUP_QUIET ) != DIR_NULL ) {
			aexists( tempstring );
			return;
		}
		/*  Change object name in the directory.
		    Due to hashing, need to delete and add it back. */
		laddr = dp->d_addr;
		flags = dp->d_flags;
		len = dp->d_len;

		dir_delete( dp );
		dp = dir_add( tempstring, laddr, flags, len );

		/* Read whole database, looking at each object name. */
		for( ; fread( (char*)&record, sizeof(record), 1, fp ) == 1 &&
		     ! feof(fp);
		    seekptr += sizeof(record) )  {

			switch( record.u_id ) {
			case ID_COMB:
				if( strcmp(cmd_args[i],record.c.c_name) != 0 )
					continue;
				(void) strcpy(record.c.c_name,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
	 			break;

			case ID_BSOLID:
				if( strcmp(cmd_args[i],record.B.B_name) != 0 )
					continue;
				(void) strcpy(record.B.B_name,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
		 		break;

			case ID_BSURF:
				/* No names here, just lots of granules to
				 * skip with no ID fields...
				 */
				seekptr += (record.d.d_nknots+record.d.d_nctls) *
					sizeof(record);
				(void) fseek( fp, seekptr+sizeof(record), 0);
		 		break;

			case ID_ARS_A:
				if( strcmp(cmd_args[i],record.a.a_name) != 0 )
					continue;
				(void) strcpy(record.a.a_name,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
	 			break;

			case ID_P_HEAD:
				if( strcmp(cmd_args[i],record.p.p_name) != 0 )
					continue;
				(void) strcpy(record.p.p_name,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
	 			break;

			case ID_SOLID:
				if( strcmp(cmd_args[i],record.s.s_name) != 0 )
					continue;
				(void) strcpy(record.s.s_name,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
	 			break;

			case ID_MEMB:
				if( strcmp(cmd_args[i],record.M.m_instname) != 0 )
					continue;
				(void) strcpy(record.M.m_instname,tempstring);
				(void) fseek( fp, seekptr, 0);
				fwrite((char*)&record, sizeof(record), 1, fp);
				(void) fseek( fp, seekptr+sizeof(record), 0);
 				break;

			default:
				;
			}
		}
		seekptr = 0;
		fseek(fp,0,0);  /* Rewind the file for next pass.*/
	}  
	(void) fclose(fp);
}

/*
 *			F _ K E E P
 *
 *  	Saves named objects in specified file.
 *	Good for pulling parts out of a description.
 */

#define MAX_KEEPCOUNT	  2000
static char	*keep_names[MAX_KEEPCOUNT];
static int	keepfd;
static int	keep_count;

void
f_keep() {
	register struct directory *dp;
	int i;

	if( (keepfd = creat( cmd_args[1], 0644 )) < 0 )  {
		perror( cmd_args[1] );
		return;
	}
	
	/* ident record */
	(void)lseek(keepfd, 0L, 0);
	record.i.i_id = ID_IDENT;
	record.i.i_units = localunit;
	strcpy(record.i.i_version, ID_VERSION);
	sprintf(record.i.i_title, "Parts of: %s", cur_title);
	(void)write(keepfd, (char *)&record, sizeof record);

	keep_count = 0;
	for(i = 2; i < numargs; i++) {
		if( (dp = lookup(cmd_args[i], LOOKUP_NOISY)) != DIR_NULL )
			file_put(dp);
		if( keep_count >= MAX_KEEPCOUNT ) {
			(void)printf("ERROR: exceeded MAX objects to keep, %d objects kept\n",MAX_KEEPCOUNT);
			break;
		}
	}
	(void) close(keepfd);
}

/*
 *  Saves all objects in hierarchy of an object.
 */
static void
file_put( dp )
register struct directory *dp;
{
	register struct directory *nextdp;
	register int i;

	/* If this object already sent to keep file, just return */
	for( i=0; i<keep_count; i++) {
		if(keep_names[i] == dp->d_namep)
			return;
	}

	/* write this record to the keep file if new object */
	keep_names[keep_count++] = dp->d_namep;

	if( keep_count >= MAX_KEEPCOUNT ) 
		return;

	db_getrec (dp, (char *)&record, 0);
	(void)write(keepfd, (char *)&record, sizeof record);

	if(record.u_id == ID_COMB) {
		/* write out all member records */
		for( i=1; i<dp->d_len; i++ )  {
			db_getrec( dp, &record, i );
			(void)write(keepfd, (char *)&record, sizeof record);
		}
		/* recurse on all member records */
		for( i=1; i<dp->d_len; i++ )  {
			db_getrec( dp, &record, i );
			nextdp = lookup(record.M.m_instname,LOOKUP_NOISY);
			if( nextdp == DIR_NULL )
				continue;
			file_put( nextdp );
		}
		return;
	}

	/* Whatever it is, we must write all granules */
	for( i=1; i<dp->d_len; i++ )  {
		db_getrec( dp, &record, i );
		(void)write(keepfd, (char *)&record, sizeof record);
	}
}

/*
 *			F _ T R E E
 *
 *	Print out a list of all members and submembers of an object.
 */
void
f_tree() {
	register struct directory *dp;
	register int j;

	(void) signal( SIGINT, sig2);  /* Allow interrupts */

	for ( j = 1; j < numargs; j++) {
		if( (dp = lookup( cmd_args[j], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		printnode(dp, 0, 0);
		putchar( '\n' );
	}
}

static void
printnode( dp, pathpos, cont )
register struct directory *dp;
int pathpos;
int cont;		/* non-zero when continuing partly printed line */
{	
	union record rec;
	register int	i;

	/*
	 * Load the record into local record buffer
	 */
	db_getrec( dp, &rec, 0 );

	if( !cont ) {
		for( i=0; i<(pathpos*(NAMESIZE+2)); i++) 
			putchar(' ');
		cont = 1;
	}
	printf("| %s", dp->d_namep);

	if( rec.u_id != ID_COMB )  {
		putchar( '\n' );
		return;
	}
	i = NAMESIZE - strlen(dp->d_namep);
	while( i-- > 0 )
		putchar('_');

	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	for( i=1; i < dp->d_len; i++ )  {
		register struct directory *nextdp;	/* temporary */

		db_getrec( dp, &rec, i );
		if( (nextdp = lookup( rec.M.m_instname, LOOKUP_NOISY )) == DIR_NULL )
			continue;

		printnode ( nextdp, pathpos+1, cont );
		cont = 0;
	}
}



/*	F _ M V A L L
 *
 *	rename all occurences of an object
 *	format:	mvall oldname newname
 *
 */
void
f_mvall()
{
	register FILE *fp;
	long seekptr;

	if( strlen(cmd_args[2]) > NAMESIZE ) {
		(void)printf("ERROR: name length limited to %d characters\n",
				NAMESIZE);
		return;
	}

	if( (fp = fopen(filename, "r+")) == NULL ) {
		(void)printf("f_mvall: fopen failed\n");
		return;
	}

	/* no interupts */
	(void)signal( SIGINT, SIG_IGN );

	/* rename the record itself */
	f_name();

	/* rename all member records with this name */
	seekptr = 0;
	for( ; fread( (char*)&record, sizeof(record), 1, fp ) == 1 &&
	     ! feof(fp);
	    seekptr += sizeof(record) )  {

		if( record.u_id == ID_MEMB ) {
			if( strcmp( cmd_args[1], record.M.m_instname ) == 0 ) {
				/* match -- change this name */
				(void)strcpy(record.M.m_instname, cmd_args[2]);
				(void)fseek( fp, seekptr, 0 );
				fwrite( (char *)&record.M, sizeof(record), 1, fp );
				(void)fseek(fp, seekptr+sizeof(record), 0);
			}
		}
	}

	(void)fclose(fp);
}

/*	F _ K I L L A L L
 *
 *	kill object[s] and
 *	remove all occurences of object[s]
 *	format:	killall obj1 ... objn
 *
 */
void
f_killall()
																		{
	register FILE *fp;
	char combname[NAMESIZE+2];
	int len;

	/* no interupts */
	(void)signal( SIGINT, SIG_IGN );

	if( (fp = fopen(filename, "r")) == NULL ) {
		(void)printf("f_killall: fopen failed\n");
		return;
	}

	/* hunt for all combinations with matching member records */
	while( fread( (char*)&record, sizeof(record), 1, fp ) == 1 &&
	     ! feof(fp) )  {
		if( record.u_id != ID_COMB )
	     		continue;
		if( (len = record.c.c_length) == 0)
			continue;
		/* save the combination name */
	     	NAMEMOVE( record.c.c_name, combname );
		while( len-- ) {	/* each member */
			register int i;
			fread( (char *)&record, sizeof(record), 1, fp );
			if( record.u_id != ID_MEMB )
				continue;
			for(i=1; i<numargs; i++) {
				if(strcmp(cmd_args[i], record.M.m_instname) != 0)
					continue;
				/* match ... must remove at least one member */
				rm_membs( combname );
				break;
			}
		}
	}
	(void)fclose(fp);
	/* ALL references removed...now KILL the object[s] */
	/* reuse cmd_args[] */
	f_kill();
}

/*
 *			R M _ M E M B S
 *
 *  Hack:  re-uses cmd_args[]
 *  Note that the buffering of fread() may occasionally cause
 *  odd interactions...
 */
void
rm_membs( name )
char *name;
{
	register struct directory *dp;
	register int i, rec, num_deleted;
	union record record;

	if( (dp = lookup( name, LOOKUP_QUIET )) == DIR_NULL )
		return;
	(void)printf("%s: ",name);

	/* Examine all the Member records, one at a time */
	num_deleted = 0;
top:
	for( rec = 1; rec < dp->d_len; rec++ )  {
		db_getrec( dp, &record, rec );
		/* Compare this member to each command arg */
		for( i = 1; i < numargs; i++ )  {
			if( strcmp( cmd_args[i], record.M.m_instname ) != 0 )
				continue;
			(void)printf("deleting member %s\n", cmd_args[i] );
			num_deleted++;
			db_delrec( dp, rec );
			goto top;
		}
	}
	/* go back and undate the header record */
	if( num_deleted ) {
		db_getrec(dp, &record, 0);
		record.c.c_length -= num_deleted;
		db_putrec(dp, &record, 0);
	}
}




/*		F _ K I L L T R E E ( )
 *
 *	Kill ALL paths belonging to an object
 *
 */
void
f_killtree()
{
	register struct directory *dp;
	register int i;

	/* no interupts */
	(void)signal( SIGINT, SIG_IGN );
	
	for(i=1; i<numargs; i++) {
		if( (dp = lookup(cmd_args[i], LOOKUP_NOISY) ) == DIR_NULL )
			continue;
		killtree( dp );
	}
}

void
killtree( dp )
register struct directory *dp;
{

	struct directory *nextdp;
	int nparts, i;

	db_getrec(dp, (char *)&record, 0);

	if( record.u_id == ID_COMB ) {
		nparts = record.c.c_length;

		for(i=1; i<=nparts; i++) {
			/* get ith member */
			db_getrec(dp, (char *)&record, i);

			if( (nextdp = lookup(record.M.m_instname, LOOKUP_QUIET)) == DIR_NULL )
				continue;
			killtree( nextdp );
		}
		/* finished killing all members....kill this comb */
		(void)printf("KILL COMB :  %s\n",dp->d_namep);
		eraseobj( dp );
		db_delete( dp);
		dir_delete( dp );
		return;
	}

	/* NOT a comb, if solid, ars, spline, or polygon -> kill */
	if( record.u_id == ID_SOLID ||
		record.u_id == ID_ARS_A ||
		record.u_id == ID_P_HEAD ||
		record.u_id == ID_BSOLID ) {

		(void)printf("KILL SOLID:  %s\n",dp->d_namep);
		eraseobj( dp );
		db_delete( dp );
		dir_delete( dp );
	}
}



#ifdef SYSV
#undef bzero
bzero( str, n )
{
	memset( str, '\0', n );
}
#endif
