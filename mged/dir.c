/*
 *			D I R . C
 *
 * Functions -
 *	db_open		Open the database
 *	dir_build	Build directory of object file
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
 *	f_memprint	Debug, print memory & db free maps
 *	conversions	Builds conversion factors given a local unit
 *	dir_units	Changes the local unit
 *	dir_title	Change the target title
 *	dir_nref	Count number of times each db element referenced
 *	regexp_match	Does regular exp match given string?
 *	dir_summary	Summarize contents of directory by categories
 *	f_tops		Prints top level items in database
 *	cmd_glob	Does regular expression expansion on cmd_args[]
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A. Applin
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

#include	<fcntl.h>
#include	<stdio.h>
#include	<string.h>
#include	<signal.h>
#include "ged_types.h"
#include "../h/db.h"
#include "ged.h"
#include "solid.h"
#include "objdir.h"
#include "../h/vmath.h"
#include "dm.h"

extern int	read();
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

extern void color_addrec();
extern int numargs, maxargs;		/* defined in cmd.c */
extern char *cmd_args[];		/* defined in cmd.c */

/*
 *  			D B _ O P E N
 */
void
db_open( name )
char *name;
{
	if( (objfd = open( name, O_RDWR )) < 0 )  {
		if( (objfd = open( name, O_RDONLY )) < 0 )  {
			perror( name );
			exit(2);		/* NOT finish */
		}
		(void)printf("%s: READ ONLY\n", name);
		read_only = 1;
	}
	filename = name;
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
	(void)read( objfd, (char *)&record, sizeof record );
	if( record.u_id != ID_IDENT )  {
		(void)printf("Warning:  File is not a proper GED database\n");
		(void)printf("This database should be converted before further use.\n");
		localunit = 0;
		local2base = base2local = 1.0;
	} else {
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
	}

	if( (fp = fopen( filename, "r" )) == NULL )  {
		(void)printf("dir_build: fopen failed\n");
		return;
	}

	addr = 0L;		/* Addr of record read at top of loop */
	while(
	    fread( (char *)&record, sizeof(record), 1, fp ) == 1  &&
	    !feof(fp)
	)  switch( record.u_id )  {

	case ID_COMB:
		(void)dir_add( record.c.c_name, addr,
			record.c.c_flags == 'R' ?
				DIR_COMB|DIR_REGION : DIR_COMB,
			record.c.c_length+1 );
		addr += (long)(record.c.c_length+1) * (long)sizeof record;
		(void)fseek( fp, addr, 0 );
		continue;

	case ID_ARS_A:
		(void)dir_add( record.a.a_name, addr,
			DIR_SOLID, record.a.a_totlen+1 );
		addr += (long)(record.a.a_totlen+1) * (long)(sizeof record);
		(void)fseek( fp, addr, 0 );
		continue;

	case ID_B_SPL_HEAD:
		dir_add( record.d.d_name, addr,
			DIR_SOLID, record.d.d_totlen+1 );
		addr += (long)(record.d.d_totlen+1) * (long)(sizeof(record));
		(void)fseek( fp, addr, 0 );
		continue;

	case ID_B_SPL_CTL:
		(void)printf("Unattached control mesh record?\n");
		addr += (long)(sizeof(record));
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
		addr += sizeof(record);
		continue;

	case ID_FREE:
		/* Inform db manager of avail. space */
		memfree( &dbfreep, 1, addr/sizeof(union record) );
		addr += sizeof(record);
		continue;

	case ID_SOLID:
		(void)dir_add( record.s.s_name, addr, DIR_SOLID, 1 );
		addr += sizeof(record);
		continue;

	case ID_MATERIAL:
		color_addrec( &record, addr );
		addr += sizeof(record);
		continue;

	default:
		(void)printf( "dir_build:  unknown record %c (0%o) erased\n",
			record.u_id, record.u_id );
		if( !read_only )  {
			/* zap this record and put in free map */
			zapper.u_id = ID_FREE;	/* The rest will be zeros */
			(void)lseek( objfd, addr, 0 );
			(void)write(objfd, (char *)&zapper, sizeof(zapper));
		}
		memfree( &dbfreep, 1, addr/(sizeof(union record)) );
		addr += sizeof(record);
		continue;
	}

	/* Record current end of objects file */
	eof_addr = addr;
	(void)fclose(fp);
}

/*
 *			D I R _ P R I N T
 *
 * This routine lists the names of all the objects accessible
 * in the object file.
 */
void
dir_print()  {
	register struct directory	*dp;
	register int i;

	(void)signal( SIGINT, sig2 );	/* allow interupts */
	if( numargs > 1 ) {
		/* Just list specified names */
		for( i = 1; i < numargs; i++ )  {
			if( (dp = lookup( cmd_args[i], LOOKUP_NOISY )) == DIR_NULL )
				continue;
			col_item( dp->d_namep );
			if( dp->d_flags & DIR_COMB )
				col_putchar( '/' );
			if( dp->d_flags & DIR_REGION )
				col_putchar( 'R' );
		}
		col_eol();
		return;
	}
	/* Full table of contents */
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			col_item( dp->d_namep );
			if( dp->d_flags & DIR_COMB )
				col_putchar( '/' );
			if( dp->d_flags & DIR_REGION )
				col_putchar( 'R' );
		}
	}
	col_eol();
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

	for( dp = *dir_hash(str); dp != DIR_NULL; dp=dp->d_forw )  {
		if(
			str[0] == dp->d_namep[0]  &&	/* speed */
			str[1] == dp->d_namep[1]  &&	/* speed */
			strcmp( str, dp->d_namep ) == 0
		)
			return(dp);
	}

	if( noisy )
		(void)printf("dir_lookup:  could not find '%s'\n", str );
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

	GETSTRUCT( dp, directory );
	if( dp == DIR_NULL )
		return( DIR_NULL );
	dp->d_namep = strdup( name );
	dp->d_addr = laddr;
	dp->d_flags = flags;
	dp->d_len = len;
	headp = dir_hash( name );
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
		(void)printf("db_getrec(%s):  read error.  Wanted %d, got %d\n",
			dp->d_namep, sizeof(union record), i );
		where->u_id = '\0';	/* undefined id */
	}
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

	if( offset < 0 || offset+len >= dp->d_len )  {
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
 */
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
		return;
	}

	/* Easy case -- see if at end-of-file */
	extra_start = dp->d_addr + dp->d_len * sizeof(union record);
	if( extra_start == eof_addr )  {
		eof_addr += count * sizeof(union record);
		dp->d_len += count;
clean:
		(void)lseek( objfd, extra_start, 0 );
		zapper.u_id = ID_FREE;	/* The rest will be zeros */
		for( i = 0; i < count; i++ )
			(void)write( objfd, (char *)&zapper, sizeof(zapper) );
		return;
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
	(void)printf("  %5d regions, %d non-region combinations\n", reg, comb);
	(void)printf("  %5d branch names\n", br);
	(void)printf("  %5d total objects\n", sol+reg+comb );

	if( flag == 0 )
		return;
	/* Print all names matching the flags parameter */
	/* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( dp->d_flags & flag )
				col_item(dp->d_namep);
		}
	}
	col_eol();
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

	(void)signal( SIGINT, sig2 );	/* allow interupts */
	dir_nref();
	for( i = 0; i < NHASH; i++ )  {
		for( dp = DirHead[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( dp->d_nref > 0 )
				continue;
			/* Object is not a member of any combination */
			col_item(dp->d_namep);
		}
	}
	col_eol();
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
