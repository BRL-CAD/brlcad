/*
 *			D I R . C
 *
 * Functions -
 *	dir_getspace	Allocate memory for table of directory entry pointers
 *	dir_print	Print table-of-contents of object file
 *	f_memprint	Debug, print memory & db free maps
 *	dir_nref	Count number of times each db element referenced
 *	regexp_match	Does regular exp match given string?
 *	dir_summary	Summarize contents of directory by categories
 *	f_tops		Prints top level items in database
 *	cmd_glob	Does regular expression expansion
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

#include "conf.h"

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "rtstring.h"
#include "raytrace.h"
#include "./ged.h"
#include "externs.h"
#include "./solid.h"
#include "./dm.h"

#define BAD_EOF	(-1L)			/* eof_addr not set yet */

void	killtree();

static void printnode();

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
		rt_log( "dir_getspace: was passed %d, used 0\n",
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
	dir_basep = (struct directory **) rt_malloc(
		(num_entries+1) * sizeof(struct directory *),
		"dir_getspace *dir[]" );
	return(dir_basep);
}

/*
 *			D I R _ P R I N T
 *
 * This routine lists the names of all the objects accessible
 * in the object file.
 */
int
dir_print(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2);	/* allow interupts */

	if( argc > 1) {
		/* Just list specified names */
		dirp = dir_getspace( argc-1 );
		dirp0 = dirp;
		/*
		 * Verify the names, and add pointers to them to the array.
		 */
		for( i = 1; i < argc; i++ )  {
			if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) ==
			  DIR_NULL )
				continue;
			*dirp++ = dp;
		}
	} else {
		/* Full table of contents */
		dirp = dir_getspace(0);		/* Enough for all */
		dirp0 = dirp;
		/*
		 * Walk the directory list adding pointers (to the directory
		 * entries) to the array.
		 */
		for( i = 0; i < RT_DBNHASH; i++)
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
				*dirp++ = dp;
	}
	col_pr4v( dirp0, (int)(dirp - dirp0));
	rt_free( (char *)dirp0, "dir_getspace dp[]" );

	return CMD_OK;
}

/*
 *			F _ M E M P R I N T
 *  
 *  Debugging aid:  dump memory maps
 */
int
f_memprint(argc, argv)
int	argc;
char	**argv;
{
	rt_log("Display manager free map:\n");
	memprint( &(dmp->dmr_map) );
	rt_log("Database free granule map:\n");
	memprint( &(dbip->dbi_freep) );

	return CMD_OK;
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
				READ_ERR_return;
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
	rt_log("Summary:\n");
	rt_log("  %5d solids\n", sol);
	rt_log("  %5d region; %d non-region combinations\n", reg, comb);
	rt_log("  %5d total objects\n\n", sol+reg+comb );

	if( flag == 0 )
		return;
	/* Print all names matching the flags parameter */
	/* THIS MIGHT WANT TO BE SEPARATED OUT BY CATEGORY */
	
	dirp = dir_getspace(0);
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
	rt_free( (char *)dirp0, "dir_getspace" );
}

/*
 *  			F _ T O P S
 *  
 *  Find all top level objects.
 *  TODO:  Perhaps print all objects, sorted by use count, as an option?
 */
int
f_tops(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;
	struct directory **dirp, **dirp0;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	dir_nref();
	/*
	 * Find number of possible entries and allocate memory
	 */
	dirp = dir_getspace(0);
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
	rt_free( (char *)dirp0, "dir_getspace" );

	return CMD_OK;
}

/*
 *			C M D _ G L O B
 *  
 *  Assist routine for command processor.  If the current word in
 *  the argv[] array contains "*", "?", "[", or "\" then this word
 *  is potentially a regular expression, and we will tromp through the
 *  entire in-core directory searching for a match. If no match is
 *  found, the original word remains untouched and this routine was an
 *  expensive no-op.  If any match is found, it replaces the original
 *  word. Escape processing is also done done in this module.  If there
 *  are no matches, but there are escapes, the current word is modified.
 *  All matches are sought for, up to the limit of the argv[] array.
 *
 *  Returns 0 if no expansion happened, !0 if we matched something.
 */
int
cmd_glob(argcp, argv, maxargs)
int  *argcp;
char *argv[];
int   maxargs;
{
	static char word[64];
	register char *pattern;
	register struct directory	*dp;
	register int i;
	int escaped = 0;
	int orig_numargs = *argcp;

	strncpy( word, argv[*argcp], sizeof(word)-1 );
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
	 * argv, as it will be overwritten by the expansions.
	 * If any matches are found, we do not have to worry about
	 * '\' escapes since the match coming from dp->d_namep is placed
	 * into argv.  Only in the case of no matches do we have
	 * to do escape crunching.
	 */

	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !regexp_match( word, dp->d_namep ) )
				continue;
			/* Successful match */
			/* See if already over the limit */
			if( *argcp >= maxargs )  {
				rt_log("%s: expansion stopped after %d matches (%d args)\n",
					word, *argcp-orig_numargs, maxargs);
				break;
			}
			argv[(*argcp)++] = dp->d_namep;
		}
	}
	/* If one or matches occurred, decrement final argc,
	 * otherwise, do escape processing if needed.
	 */

	if( *argcp > orig_numargs )  {
		(*argcp)--;
		return(1);
	} else if(escaped) {
		char *temp;
		temp = pattern = argv[*argcp];
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
		if(*(argv[*argcp]) == '\0')
			(*argcp)--;
	}
	return(0);		/* found nothing */
}

/*
 *  			F _ F I N D
 *  
 *  Find all references to the named objects.
 */
int
f_find(argc, argv)
int	argc;
char	**argv;
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
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
				READ_ERR;
				return CMD_BAD;
			}
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=0; k<argc; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    argv[k], NAMESIZE) != 0 )
						continue;
					rt_log("%s:  member of %s\n",
						rp[j].M.m_instname,
						rp[0].c.c_name );
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
	return CMD_OK;
}

/*
 *			F _ P R E F I X
 *
 *  Prefix each occurence of a specified object name, both
 *  when defining the object, and when referencing it.
 */
int
f_prefix(argc, argv)
int	argc;
char	**argv;
{
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;
	char		tempstring[NAMESIZE+2];

	/* First, check validity, and change node names */
	for( i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL) {
			argv[i] = "";
			continue;
		}

		if( (int)(strlen(argv[1]) + strlen(argv[i])) > NAMESIZE) {
			rt_log("'%s%s' too long, must be less than %d characters.\n",
				argv[1], argv[i],
				NAMESIZE);
			argv[i] = "";
			continue;
		}

		(void) strcpy( tempstring, argv[1]);
		(void) strcat( tempstring, argv[i]);

		if( db_lookup( dbip, tempstring, LOOKUP_QUIET ) != DIR_NULL ) {
			aexists( tempstring );
			argv[i] = "";
			continue;
		}
		/*  Change object name in the directory. */
		if( db_rename( dbip, dp, tempstring ) < 0 )  {
			rt_log("error in rename to %s, aborting\n", tempstring );
			ERROR_RECOVERY_SUGGESTION;
			return CMD_BAD;
		}
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
				READ_ERR;
				return CMD_BAD;
			}
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=2; k<argc; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    argv[k], NAMESIZE) != 0 )
						continue;
					(void)strcpy( tempstring, argv[1]);
					(void)strcat( tempstring, argv[k]);
					(void)strncpy(rp[j].M.m_instname,
						tempstring, NAMESIZE);
					if( db_put( dbip, dp, rp, 0, dp->d_len ) < 0 ) {
						WRITE_ERR;
						return CMD_BAD;
					}
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
	return CMD_OK;
}

/*
 *			F _ K E E P
 *
 *  	Save named objects in specified file.
 *	Good for pulling parts out of one model for use elsewhere.
 */
static FILE	*keepfp;

void
node_write( dbip, dp )
struct db_i	*dbip;
register struct directory *dp;
{
	register union record	*rp;
	int			want;

	if( dp->d_nref++ > 0 )
		return;		/* already written */

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		READ_ERR_return;
	want = dp->d_len*sizeof(union record);
	if( fwrite( (char *)rp, want, 1, keepfp ) != 1 )
		perror("keep fwrite");
	rt_free( (char *)rp, "keep rec[]" );
}

int
f_keep(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	struct rt_vls		title;
	struct rt_vls		units;
	register int		i;

	/* First, clear any existing counts */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_nref = 0;
	}

	/* Alert user if named file already exists */
	if( (keepfp = fopen( argv[1], "r" ) ) != NULL )  {
		rt_log("keep:  appending to '%s'\n", argv[1] );
		fclose(keepfp);
	}

	if( (keepfp = fopen( argv[1], "a" ) ) == NULL )  {
		perror( argv[1] );
		return CMD_BAD;
	}
	
	/* ident record */
	rt_vls_init( &title );
	rt_vls_strcat( &title, "Parts of: " );
	rt_vls_strcat( &title, dbip->dbi_title );

	rt_vls_init( &units);
	
	if( localunit == ID_NO_UNIT)
		rt_vls_strcat( &units, "none");

	if( localunit == ID_MM_UNIT)
		rt_vls_strcat( &units, "mm");

	if( localunit == ID_CM_UNIT)
		rt_vls_strcat( &units, "cm");

	if( localunit == ID_M_UNIT)
		rt_vls_strcat( &units, "m");

	if( localunit == ID_IN_UNIT)
		rt_vls_strcat( &units, "in");

	if( localunit == ID_FT_UNIT)
		rt_vls_strcat( &units, "ft");

	if( mk_id_units( keepfp, rt_vls_addr(&title), rt_vls_addr(&units) ) < 0 )  {
		perror("fwrite");
		rt_log("mk_id_units() failed\n");
		fclose(keepfp);
		rt_vls_free( &title );
		rt_vls_free( &units );
		return CMD_BAD;
	}

	for(i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		db_functree( dbip, dp, node_write, node_write );
	}

	fclose(keepfp);
	rt_vls_free( &title );
	rt_vls_free( &units );

	return CMD_OK;
}

#ifdef OLD
/*
 *			F _ T R E E
 *
 *	Print out a list of all members and submembers of an object.
 */
void
f_tree(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int j;

	(void) signal( SIGINT, sig2);  /* Allow interrupts */

	for ( j = 1; j < argc; j++) {
		if( j > 1 )
			rt_log( "\n" );
		if( (dp = db_lookup( dbip, argv[j], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		printnode(dp, 0, 0);
	}
}

/*
 *			P R I N T N O D E
 */
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
		READ_ERR_return;

	if( !cont ) {
		for( i=0; i<(pathpos*(NAMESIZE+2)); i++) 
			rt_log(" ");
		cont = 1;
	}
	rt_log("| %s", dp->d_namep);
	if( !(dp->d_flags & DIR_COMB) )  {
		rt_log( "\n" );
		return;
	}


	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	i = NAMESIZE - strlen(dp->d_namep);
	while( i-- > 0 )
		rt_log("_");

	if( dp->d_len <= 1 )
		rt_log("\n");		/* empty combination */

	for( i=1; i < dp->d_len; i++ )  {
		if( (nextdp = db_lookup( dbip, rp[i].M.m_instname, LOOKUP_NOISY ))
		    == DIR_NULL )
			continue;

		printnode ( nextdp, pathpos+1, cont );
		cont = 0;
	}
	rt_free( (char *)rp, "printnode recs");
}
#else
/*
 *			F _ T R E E
 *
 *	Print out a list of all members and submembers of an object.
 */
int
f_tree(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int j;

	(void) signal( SIGINT, sig2);  /* Allow interrupts */

	for ( j = 1; j < argc; j++) {
		if( j > 1 )
			rt_log( "\n" );
		if( (dp = db_lookup( dbip, argv[j], LOOKUP_NOISY )) == DIR_NULL )
			continue;
		printnode(dp, 0, 0);
	}

	return CMD_OK;
}

/*
 *			P R I N T N O D E
 */
static void
printnode( dp, pathpos, prefix )
register struct directory *dp;
int pathpos;
char prefix;
{	
	union record	*rp;
	register int	i;
	register struct directory *nextdp;

	if( (rp = db_getmrec( dbip, dp )) == (union record *)0 )
		READ_ERR_return;

	for( i=0; i<pathpos; i++) 
		rt_log("\t");
	if( prefix ) {
		rt_log("%c", prefix);
		rt_log(" ");
	}

	rt_log("%s", dp->d_namep);
	/* Output Comb and Region flags (-F?) */
	if( dp->d_flags & DIR_COMB )
		rt_log("/");
	if( dp->d_flags & DIR_REGION )
		rt_log("R");
	rt_log("\n");

	if( !(dp->d_flags & DIR_COMB) )  {
		return;
	}

	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */
	for( i=1; i < dp->d_len; i++ )  {
		if( (nextdp = db_lookup( dbip, rp[i].M.m_instname, LOOKUP_NOISY ))
		    == DIR_NULL )
			continue;

		prefix = rp[i].M.m_relation;
		printnode ( nextdp, pathpos+1, prefix );
	}
	rt_free( (char *)rp, "printnode recs");
}
#endif

/*	F _ M V A L L
 *
 *	rename all occurences of an object
 *	format:	mvall oldname newname
 *
 */
int
f_mvall(argc, argv)
int	argc;
char	**argv;
{
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;
	union record	record;

	if( (int)strlen(argv[2]) > NAMESIZE ) {
		rt_log("ERROR: name length limited to %d characters\n",
				NAMESIZE);
		return CMD_BAD;
	}

	/* rename the record itself */
	if( (dp = db_lookup( dbip, argv[1], LOOKUP_NOISY )) == DIR_NULL)
		return CMD_BAD;
	if( db_lookup( dbip, argv[2], LOOKUP_QUIET ) != DIR_NULL ) {
		aexists( argv[2]);
		return CMD_BAD;
	}
	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, argv[2] ) < 0 )  {
		rt_log("error in rename to %s, aborting\n", argv[2] );
		ERROR_RECOVERY_SUGGESTION;
		return CMD_BAD;
	}

	/* Change name in the file */
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
		READ_ERR;
		return CMD_BAD;
	}
	NAMEMOVE( argv[2], record.c.c_name );
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
		WRITE_ERR;
		return CMD_BAD;
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
				READ_ERR;
				return CMD_BAD;
			}
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=2; k<argc; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    argv[1], NAMESIZE) != 0 )
						continue;
					(void)strncpy(rp[j].M.m_instname,
						argv[2], NAMESIZE);
					if( db_put( dbip, dp, rp, 0, dp->d_len ) < 0 ) {
						WRITE_ERR;
						return CMD_BAD;
					}
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}
	return CMD_OK;
}

/*	F _ K I L L A L L
 *
 *	kill object[s] and
 *	remove all references to the object[s]
 *	format:	killall obj1 ... objn
 *
 */
int
f_killall(argc, argv)
int	argc;
char	**argv;
{
	register int	i,j,k;
	register union record *rp;
	register struct directory *dp;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
again:
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
				READ_ERR;
				return CMD_BAD;
			}
			/* [0] is COMB, [1..n] are MEMBERs */
			for( j=1; j < dp->d_len; j++ )  {
				if( rp[j].M.m_instname[0] == '\0' )
					continue;
				for( k=1; k<argc; k++ )  {
					if( strncmp( rp[j].M.m_instname,
					    argv[k], NAMESIZE) != 0 )
						continue;

					/* Remove this reference */
					if( db_delrec( dbip, dp, j ) < 0 )  {
						rt_log("error in killing reference to '%s', exit MGED and retry\n",
							argv[k]);
						ERROR_RECOVERY_SUGGESTION;
						return CMD_BAD;
					}
					rt_free( (char *)rp, "dir_nref recs" );
					goto again;
				}
			}
			rt_free( (char *)rp, "dir_nref recs" );
		}
	}

	/* ALL references removed...now KILL the object[s] */
	/* reuse argv[] */
	return f_kill( argc, argv );
}


/*		F _ K I L L T R E E ( )
 *
 *	Kill ALL paths belonging to an object
 *
 */
int
f_killtree(argc, argv)
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	(void)signal( SIGINT, sig2 );	/* allow interupts */

	for(i=1; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY) ) == DIR_NULL )
			continue;
		db_functree( dbip, dp, killtree, killtree );
	}

	return CMD_OK;
}

/*
 *			K I L L T R E E
 */
void
killtree( dbip, dp )
struct db_i	*dbip;
register struct directory *dp;
{
	rt_log("KILL %s:  %s\n",
		(dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
		dp->d_namep );
	eraseobj( dp );
	if( db_delete( dbip, dp) < 0 || db_dirdelete( dbip, dp ) < 0 )
		DELETE_ERR_return("");
}

int
f_debugdir( argc, argv )
int	argc;
char	**argv;
{
	db_pr_dir( dbip );
	return CMD_OK;
}
