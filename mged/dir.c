/*
 *			D I R . C
 *
 * Functions -
 *	dir_build	Build directory of object file
 *	dir_getspace	Allocate memory for table of directory entry pointers
 *	dir_print	Print table-of-contents of object file
 *	f_memprint	Debug, print memory & db free maps
 *	conversions	Builds conversion factors given a local unit
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
#include "raytrace.h"
#include "./ged.h"
#include "./solid.h"
#include "./dm.h"

extern int	read(), strcmp();
extern long	lseek();
extern char	*malloc();
extern void	exit(), free(), perror();

#define BAD_EOF	(-1L)			/* eof_addr not set yet */

union record	record;
static union record zapper;		/* Zeros, for erasing records */

static char *units_str[] = {
	"none",
	"mm",
	"cm",
	"meters",
	"inches",
	"feet",
	"extra"
};

void	conversions();
void	killtree();

static void file_put();
static void printnode();

extern int numargs, maxargs;		/* defined in cmd.c */
extern char *cmd_args[];		/* defined in cmd.c */

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
		for( i = 0; i < RT_DBNHASH; i++)
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
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
			if( (dp = db_lookup( dbip, cmd_args[i], LOOKUP_NOISY)) ==
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
		for( i = 0; i < RT_DBNHASH; i++)
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
				*dirp++ = dp;
	}
	col_pr4v( dirp0, (int)(dirp - dirp0));
	free( dirp0);
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
	memprint( &(dbip->dbi_freep) );
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

/*
 *			D I R _ N R E F
 *
 * Count the number of time each directory member is referenced
 * by a COMBination record.
 */
void
dir_nref( )
{
	register int		j;
	register union record	*rp;
	register struct directory *dp;
	register struct directory *newdp;
	register int		i;

	/* First, clear any existing counts */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_nref = 0;
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				continue;
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				if( (newdp = db_lookup( dbip, rp[j].M.m_instname,
				    LOOKUP_QUIET)) != DIR_NULL )
					newdp->d_nref++;
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
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
regexp_match(	 pattern, string )
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
	static int sol, comb, reg;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	sol = comb = reg = 0;
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( dp->d_flags & DIR_SOLID )
				sol++;
			if( dp->d_flags & DIR_COMB )
				if( dp->d_flags & DIR_REGION )
					reg++;
				else
					comb++;
		}
	}
	(void)printf("Summary:\n");
	(void)printf("  %5d solids\n", sol);
	(void)printf("  %5d region; %d non-region combinations\n", reg, comb);
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
	for( i = 0; i < RT_DBNHASH; i++)
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
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
	for( i = 0; i < RT_DBNHASH; i++)
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
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
 *  Assist routine for command processor.  If the current word in
 *  the cmd_args[] array contains "*", "?", "[", or "\" then this word
 *  is potentially a regular expression, and we will tromp through the
 *  entire in-core directory searching for a match. If no match is
 *  found, the original word remains untouched and this routine was an
 *  expensive no-op.  If any match is found, it replaces the original
 *  word. Escape processing is also done done in this module.  If there
 *  are no matches, but there are escapes, the current word is modified.
 *  All matches are sought for, up to the limit of the cmd_args[] array.
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
	int escaped = 0;
	int orig_numargs = numargs;

	strncpy( word, cmd_args[numargs], sizeof(word)-1 );
	/* If * ? [ or \ are present, this is a regular expression */
	pattern = word;
	do {
		if( *pattern == '\0' )
			return(0);		/* nothing to do */
		if( *pattern == '*' ||
		    *pattern == '?' ||
		    *pattern == '[' ||
		    *pattern == '\\' )
			break;
	} while( *pattern++);

	/* Note if there are any escapes */
	for( pattern = word; *pattern; pattern++)
		if( *pattern == '\\') {
			escaped++;
			break;
		}

	/* Search for pattern matches.
	 * First, save the pattern (in word), and remove it from
	 * cmd_args, as it will be overwritten by the expansions.
	 * If any matches are found, we do not have to worry about
	 * '\' escapes since the match coming from dp->d_namep is placed
	 * into cmd_args.  Only in the case of no matches do we have
	 * to do escape crunching.
	 */

	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !regexp_match( word, dp->d_namep ) )
				continue;
			/* Successful match */
			cmd_args[numargs++] = dp->d_namep;
			if( numargs >= maxargs )  {
				(void)printf("%s: expansion stopped after %d matches\n", word, maxargs);
				break;
			}
		}
	}
	/* If one or matches occurred, decrement final numargs,
	 * otherwise, do escape processing if needed.
	 */

	if( numargs > orig_numargs )  {
		numargs--;
		return(1);
	} else if(escaped) {
		char *temp;
		temp = pattern = cmd_args[numargs];
		do {
			if(*pattern != '\\') {
				*temp = *pattern;
				temp++;
			} else if(*(pattern + 1) == '\\') {
				*temp = *pattern;
				pattern++;
				temp++;
			}
		} while(*pattern++);

		/* Elide the rare pattern which becomes null ("\<NULL>") */
		if(*(cmd_args[numargs]) == '\0')
			numargs--;
	}
	return(0);		/* found nothing */
}

/*
 *  			F _ F I N D
 *  
 *  Find all references to the named objects.
 */
void
f_find()
{
	register int	i,j,k;
	register struct directory *dp;
	register union record	*rp;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				continue;
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=0; k<numargs; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    cmd_args[k], NAMESIZE) != 0 )
						continue;
					(void)printf("%s:  member of %s\n",
						rp[j].M.m_instname,
						rp[0].c.c_name );
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
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
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;
	char		tempstring[NAMESIZE+2];

	/* First, check validity, and change node names */
	for( i = 2; i < numargs; i++) {
		if( (dp = db_lookup( dbip, cmd_args[i], LOOKUP_NOISY )) == DIR_NULL) {
			cmd_args[i] = "";
			continue;
		}

		if( strlen(cmd_args[1]) + strlen(cmd_args[i]) > NAMESIZE) {
			printf("'%s%s' too long, must be less than %d characters.\n",
				cmd_args[1], cmd_args[i],
				NAMESIZE);
			cmd_args[i] = "";
			continue;
		}

		(void) strcpy( tempstring, cmd_args[1]);
		(void) strcat( tempstring, cmd_args[i]);

		if( db_lookup( dbip, tempstring, LOOKUP_QUIET ) != DIR_NULL ) {
			aexists( tempstring );
			cmd_args[i] = "";
			continue;
		}
		/*  Change object name in the directory. */
		if( db_rename( dbip, dp, tempstring ) < 0 )
			printf("error in rename to %s\n", tempstring );
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				continue;
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=2; k<numargs; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    cmd_args[k], NAMESIZE) != 0 )
						continue;
					(void)strcpy( tempstring, cmd_args[1]);
					(void)strcat( tempstring, cmd_args[k]);
					(void)strncpy(rp[j].M.m_instname,
						tempstring, NAMESIZE);
					(void)db_put( dbip, dp, rp, 0, dp->d_len );
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
}

/*
 *			F _ K E E P
 *
 *  	Save named objects in specified file.
 *	Good for pulling parts out of one model for use elsewhere.
 */
static int	keepfd;

void
node_write( dbip, dp )
struct db_i	*dbip;
register struct directory *dp;
{
	register union record	*rp;
	int			want;

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return;
	want = dp->d_len*sizeof(union record);
	if( write( keepfd, (char *)rp, want ) != want )
		perror("keep write");
}

void
f_keep() {
	register struct directory *dp;
	register int		i;

	/* First, clear any existing counts */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_nref = 0;
	}

	if( (keepfd = creat( cmd_args[1], 0644 )) < 0 )  {
		perror( cmd_args[1] );
		return;
	}
	
	/* ident record */
	(void)lseek(keepfd, 0L, 0);
	record.i.i_id = ID_IDENT;
	record.i.i_units = localunit;
	strcpy(record.i.i_version, ID_VERSION);
	sprintf(record.i.i_title, "Parts of: %s", cur_title);	/* XXX len */
	(void)write(keepfd, (char *)&record, sizeof record);

	for(i = 2; i < numargs; i++) {
		if( (dp = db_lookup( dbip, cmd_args[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		db_functree( dbip, dp, node_write, node_write );
	}
	(void) close(keepfd);
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
		if( (dp = db_lookup( dbip, cmd_args[j], LOOKUP_NOISY )) == DIR_NULL )
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
	union record	*rp;
	register int	i;
	register struct directory *nextdp;

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		return;

	if( !cont ) {
		for( i=0; i<(pathpos*(NAMESIZE+2)); i++) 
			putchar(' ');
		cont = 1;
	}
	printf("| %s", dp->d_namep);
	if( !(dp->d_flags & DIR_COMB) )  {
		putchar( '\n' );
		return;
	}


	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	i = NAMESIZE - strlen(dp->d_namep);
	while( i-- > 0 )
		putchar('_');

	for( i=1; i < dp->d_len; i++ )  {
		if( (nextdp = db_lookup( dbip, rp[i].M.m_instname, LOOKUP_NOISY ))
		    == DIR_NULL )
			continue;

		printnode ( nextdp, pathpos+1, cont );
		cont = 0;
	}
	rt_free( (char *)rp, "printnode recs");
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
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;

	if( strlen(cmd_args[2]) > NAMESIZE ) {
		(void)printf("ERROR: name length limited to %d characters\n",
				NAMESIZE);
		return;
	}

	/* rename the record itself */
	if( (dp = db_lookup( dbip, cmd_args[1], LOOKUP_NOISY )) == DIR_NULL)
		return;
	if( db_lookup( dbip, cmd_args[2], LOOKUP_QUIET ) != DIR_NULL ) {
		aexists( cmd_args[2]);
		return;
	}
	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, cmd_args[2] ) < 0 )
		printf("error in rename to %s\n", cmd_args[2] );

	/* Change name in the file */
	(void)db_get( dbip,  dp, &record, 0 , 1);
	NAMEMOVE( cmd_args[2], record.c.c_name );
	(void)db_put( dbip, dp, &record, 0, 1 );

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				continue;
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=2; k<numargs; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    cmd_args[1], NAMESIZE) != 0 )
						continue;
					(void)strncpy(rp[j].M.m_instname,
						cmd_args[2], NAMESIZE);
					(void)db_put( dbip, dp, rp, 0, dp->d_len );
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
}

/*	F _ K I L L A L L
 *
 *	kill object[s] and
 *	remove all references to the object[s]
 *	format:	killall obj1 ... objn
 *
 */
void
f_killall()
{
	register int	i,j,k;
	register union record *rp;
	register struct directory *dp;
	char combname[NAMESIZE+2];
	int len;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
again:
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
				continue;
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=1; k<numargs; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    cmd_args[k], NAMESIZE) != 0 )
						continue;

					/* Remove this reference */
					(void)db_delrec( dbip, dp, j );
					rt_free( (char *)rp, "dir_nref recs" );
					goto again;
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}

	/* ALL references removed...now KILL the object[s] */
	/* reuse cmd_args[] */
	f_kill();
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

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	for(i=1; i<numargs; i++) {
		if( (dp = db_lookup( dbip, cmd_args[i], LOOKUP_NOISY) ) == DIR_NULL )
			continue;
		db_functree( dbip, dp, killtree, killtree );
	}
}

void
killtree( dbip, dp )
struct db_i	*dbip;
register struct directory *dp;
{
	(void)printf("KILL %s:  %s\n",
		(dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
		dp->d_namep );
	eraseobj( dp );
	db_delete( dbip, dp);
	db_dirdelete( dbip, dp );
}
