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

#include "tcl.h"
#include "tk.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./mgedtcl.h"

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
		bu_log( "dir_getspace: was passed %d, used 0\n",
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
	dir_basep = (struct directory **) bu_malloc(
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
dir_print(clientData, interp, argc, argv)
ClientData	clientData;
Tcl_Interp	*interp;
int	argc;
char	**argv;
{
  register struct directory *dp;
  register int i;
  struct directory **dirp;
  struct directory **dirp0 = (struct directory **)NULL;
  struct bu_vls vls;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  bu_vls_init(&vls);
  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    if(dirp0)
      bu_free( (genptr_t)dirp0, "dir_getspace dp[]" );
    bu_vls_free(&vls);

    return TCL_OK;
  }

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

  vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0));
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  (void)signal( SIGINT, SIG_IGN );
  bu_vls_free(&vls);
  bu_free( (genptr_t)dirp0, "dir_getspace dp[]" );
  return TCL_OK;
}

/*
 *			F _ M E M P R I N T
 *  
 *  Debugging aid:  dump memory maps
 */
int
f_memprint(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  Tcl_AppendResult(interp, "Display manager free map:\n", (char *)NULL);
  rt_memprint( &(dmp->dm_map) );
  Tcl_AppendResult(interp, "Database free granule map:\n", (char *)NULL);
  rt_memprint( &(dbip->dbi_freep) );

  return TCL_OK;
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
			bu_free( (genptr_t)rp, "dir_nref recs" );
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
regexp_match( pattern, string )
register char *pattern, *string;
{
    do {
	switch( *pattern ) {
	case '*':
	    /* match any string including null string */
	    ++pattern;
	    do {
		if( regexp_match( pattern, string ) )
		    return( 1 );
	    } while( *string++ != '\0' );
	    return( 0 );
	case '?':
	    /* match any character  */
	    if( *string == '\0' )
		return( 0 );
	    break;
	case '[':
	    /* try to match one of the characters in brackets */
	    ++pattern;
	    if( *pattern == '\0' )
		return( 0 );
	    while( *pattern != *string ) {
		if( pattern[0] == '-' && pattern[-1] != '\\')
		    if(	pattern[-1] <= *string &&
		        pattern[-1] != '[' &&
		       	pattern[ 1] >= *string &&
		        pattern[ 1] != ']' )
			break;
		++pattern;
		if( *pattern == '\0' || *pattern == ']' )
		    return( 0 );
	    }
	    /* skip to next character after closing bracket */
	    while( *pattern != '\0' && *pattern != ']' )
		++pattern;
	    break;
	case '\\':
	    /* escape special character */
	    ++pattern;
	    /* compare characters */
	    if( *pattern != *string )
		return( 0 );
	    break;
	default:
	    /* compare characters */
	    if( *pattern != *string )
		return( 0 );
	}
	++string;
    } while( *pattern++ != '\0' );
    return( 1 );
}

/*
 *			R E G E X P _ M A T C H _ A L L
 *
 * Appends a list of all database matches to the given vls, or the pattern
 * itself if no matches are found.
 * Returns the number of matches.
 *
 */
 
int
regexp_match_all( dest, pattern )
struct bu_vls *dest;
char *pattern;
{
    register int i, num;
    register struct directory *dp;

    for( i = num = 0; i < RT_DBNHASH; i++ )  {
	for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw ){
	    if( !regexp_match( pattern, dp->d_namep ) )
		continue;
	    if( num == 0 )
		bu_vls_strcat( dest, dp->d_namep );
	    else {
		bu_vls_strcat( dest, " " );
		bu_vls_strcat( dest, dp->d_namep );
	    }
	    ++num;
	}
    }

    return num;
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
	struct directory **dirp;
	struct directory **dirp0 = (struct directory **)NULL;
	struct bu_vls vls;

	bu_vls_init(&vls);
	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);	/* allow interupts */
	else{
	  if(dirp0)
	    bu_free( (genptr_t)dirp0, "dir_getspace" );
	  bu_vls_free(&vls);

	  return;
	}	  

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
	bu_log("Summary:\n");
	bu_log("  %5d solids\n", sol);
	bu_log("  %5d region; %d non-region combinations\n", reg, comb);
	bu_log("  %5d total objects\n\n", sol+reg+comb );

	if( flag == 0 ){
	  (void)signal( SIGINT, SIG_IGN );
	  return;
	}

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

	vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	(void)signal( SIGINT, SIG_IGN );
	bu_vls_free(&vls);
	bu_free( (genptr_t)dirp0, "dir_getspace" );
}

/*
 *  			F _ T O P S
 *  
 *  Find all top level objects.
 *  TODO:  Perhaps print all objects, sorted by use count, as an option?
 */
int
f_tops(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;
	struct directory **dirp;
	struct directory **dirp0 = (struct directory **)NULL;
	struct bu_vls vls;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	bu_vls_init(&vls);
	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);	/* allow interupts */
	else{
	  if(dirp0)
	    bu_free( (genptr_t)dirp0, "dir_getspace" );
	  bu_vls_free(&vls);

	  return TCL_OK;
	}

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

	vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0));
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	(void)signal( SIGINT, SIG_IGN );
	bu_vls_free(&vls);
	bu_free( (genptr_t)dirp0, "dir_getspace" );
	return TCL_OK;
}

/*
 *			C M D _ G L O B
 *  
 *  Assist routine for command processor.  If the current word in
 *  the argv[] array contains '*', '?', '[', or '\' then this word
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
				bu_log("%s: expansion stopped after %d matches (%d args)\n",
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

void
scrape_escapes_AppendResult( interp, str )
Tcl_Interp *interp;
char *str;
{
    char buf[2];
    buf[1] = '\0';
    
    while ( *str ) {
	buf[0] = *str;
	if ( *str != '\\' ) {
	    Tcl_AppendResult( interp, buf, NULL );
	} else if( *(str+1) == '\\' ) {
	    Tcl_AppendResult( interp, buf, NULL );
	    ++str;
	}
	if( *str == '\0' )
	    break;
	++str;
    }
}

/*
 *                C M D _ E X P A N D
 *
 * Performs wildcard expansion (matched to the database elements)
 * on its given arguments.  The result is returned in interp->result.
 */

int
cmd_expand( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    register char *pattern;
    register struct directory *dp;
    register int i, whicharg;
    int regexp, nummatch, thismatch, backslashed;

    if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
      return TCL_ERROR;

    nummatch = 0;
    backslashed = 0;
    for ( whicharg = 1; whicharg < argc; whicharg++ ) {
	/* If * ? or [ are present, this is a regular expression */
	pattern = argv[whicharg];
	regexp = 0;
	do {
	    if( (*pattern == '*' || *pattern == '?' || *pattern == '[') &&
	        !backslashed ) {
		regexp = 1;
		break;
	    }
	    if( *pattern == '\\' && !backslashed )
		backslashed = 1;
	    else
		backslashed = 0;
	} while( *pattern++ );

	/* If it isn't a regexp, copy directly and continue */
	if( regexp == 0 ) {
	    if( nummatch > 0 )
		Tcl_AppendResult( interp, " ", NULL );
	    scrape_escapes_AppendResult( interp, argv[whicharg] );
	    ++nummatch;
	    continue;
	}
	
	/* Search for pattern matches.
	 * If any matches are found, we do not have to worry about
	 * '\' escapes since the match coming from dp->d_namep will be
	 * clean. In the case of no matches, just copy the argument
	 * directly.
	 */

	pattern = argv[whicharg];
	thismatch = 0;
	for( i = 0; i < RT_DBNHASH; i++ )  {
	    for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
	        if( !regexp_match( pattern, dp->d_namep ) )
		    continue;
		/* Successful match */
		if( nummatch == 0 )
		    Tcl_AppendResult( interp, dp->d_namep, NULL );
		else 
		    Tcl_AppendResult( interp, " ", dp->d_namep, NULL );
		++nummatch;
		++thismatch;
	    }
	}
	if( thismatch == 0 ) {
	    if( nummatch > 0 )
		Tcl_AppendResult( interp, " ", NULL );
	    scrape_escapes_AppendResult( interp, argv[whicharg] );
	}
    }

    return TCL_OK;
}

/*
 *  			F _ F I N D
 *  
 *  Find all references to the named objects.
 */
int
f_find(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register int	i,j,k;
  register struct directory *dp;
  register union record	*rp = (union record *)NULL;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    if(rp)
      bu_free( (genptr_t)rp, "dir_nref recs" );

    return TCL_OK;
  }

  /* Examine all COMB nodes */
  for( i = 0; i < RT_DBNHASH; i++ )  {
    for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
      if( !(dp->d_flags & DIR_COMB) )
	continue;
      if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
	(void)signal( SIGINT, SIG_IGN );
	TCL_READ_ERR_return;
      }
      /* [0] is COMB, [1..n] are MEMBERs */
      for( j=1; j < dp->d_len; j++ )  {
	if( rp[j].M.m_instname[0] == '\0' )
	  continue;
	for( k=0; k<argc; k++ )  {
	  if( strncmp( rp[j].M.m_instname,
		       argv[k], NAMESIZE) != 0 )
	    continue;
	  Tcl_AppendResult(interp, rp[j].M.m_instname,
			   ":  member of ", rp[0].c.c_name,
			   "\n", (char *)NULL);
	}
      }
      bu_free( (genptr_t)rp, "dir_nref recs" );
    }
  }

  (void)signal( SIGINT, SIG_IGN );
  return TCL_OK;
}

/*
 *			F _ P R E F I X
 *
 *  Prefix each occurence of a specified object name, both
 *  when defining the object, and when referencing it.
 */
int
f_prefix(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;
	char		tempstring[NAMESIZE+2];

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* First, check validity, and change node names */
	for( i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL) {
			argv[i] = "";
			continue;
		}

		if( (int)(strlen(argv[1]) + strlen(argv[i])) > NAMESIZE) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "'%s%s' too long, must be less than %d characters.\n",
				argv[1], argv[i], NAMESIZE);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);

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
		  Tcl_AppendResult(interp, "error in rename to ", tempstring,
				   ", aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return TCL_ERROR;
		}
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
			  TCL_READ_ERR_return;
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
					  TCL_WRITE_ERR_return;
					}
				}
			}
			bu_free( (genptr_t)rp, "dir_nref recs" );
		}
	}
	return TCL_OK;
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
	bu_free( (genptr_t)rp, "keep rec[]" );
}

int
f_keep(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	struct bu_vls		title;
	struct bu_vls		units;
	register int		i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* First, clear any existing counts */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			dp->d_nref = 0;
	}

	/* Alert user if named file already exists */
	if( (keepfp = fopen( argv[1], "r" ) ) != NULL )  {
	  Tcl_AppendResult(interp, "keep:  appending to '", argv[1],
			   "'\n", (char *)NULL);
	  fclose(keepfp);
	}

	if( (keepfp = fopen( argv[1], "a" ) ) == NULL )  {
		perror( argv[1] );
		return TCL_ERROR;
	}
	
	/* ident record */
	bu_vls_init( &title );
	bu_vls_strcat( &title, "Parts of: " );
	bu_vls_strcat( &title, dbip->dbi_title );

	bu_vls_init( &units);
	
	if( localunit == ID_NO_UNIT)
		bu_vls_strcat( &units, "none");

	if( localunit == ID_MM_UNIT)
		bu_vls_strcat( &units, "mm");

	if( localunit == ID_CM_UNIT)
		bu_vls_strcat( &units, "cm");

	if( localunit == ID_M_UNIT)
		bu_vls_strcat( &units, "m");

	if( localunit == ID_IN_UNIT)
		bu_vls_strcat( &units, "in");

	if( localunit == ID_FT_UNIT)
		bu_vls_strcat( &units, "ft");

	if( mk_id_units( keepfp, bu_vls_addr(&title), bu_vls_addr(&units) ) < 0 )  {
		perror("fwrite");
		Tcl_AppendResult(interp, "mk_id_units() failed\n", (char *)NULL);
		fclose(keepfp);
		bu_vls_free( &title );
		bu_vls_free( &units );
		return TCL_ERROR;
	}

	for(i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		db_functree( dbip, dp, node_write, node_write );
	}

	fclose(keepfp);
	bu_vls_free( &title );
	bu_vls_free( &units );

	return TCL_OK;
}


/*
 *			F _ T R E E
 *
 *	Print out a list of all members and submembers of an object.
 */
int
f_tree(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register struct directory *dp;
  register int j;

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);  /* allow interupts */
  else
    return TCL_OK;

  for ( j = 1; j < argc; j++) {
    if( j > 1 )
      Tcl_AppendResult(interp, "\n", (char *)NULL);
    if( (dp = db_lookup( dbip, argv[j], LOOKUP_NOISY )) == DIR_NULL )
      continue;
    printnode(dp, 0, 0);
  }

  (void)signal( SIGINT, SIG_IGN );
  return TCL_OK;
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

  if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ){
    TCL_READ_ERR;
    return;
  }

  for( i=0; i<pathpos; i++) 
    Tcl_AppendResult(interp, "\t", (char *)NULL);

  if( prefix ) {
    struct bu_vls tmp_vls;

    bu_vls_init(&tmp_vls);
    bu_vls_printf(&tmp_vls, "%c ", prefix);
    Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    bu_vls_free(&tmp_vls);
  }

  Tcl_AppendResult(interp, dp->d_namep, (char *)NULL);
  /* Output Comb and Region flags (-F?) */
  if( dp->d_flags & DIR_COMB )
    Tcl_AppendResult(interp, "/", (char *)NULL);
  if( dp->d_flags & DIR_REGION )
    Tcl_AppendResult(interp, "R", (char *)NULL);

  Tcl_AppendResult(interp, "\n", (char *)NULL);

  if( !(dp->d_flags & DIR_COMB) )  {
    bu_free( (genptr_t)rp, "printnode recs");
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
  bu_free( (genptr_t)rp, "printnode recs");
}


/*	F _ M V A L L
 *
 *	rename all occurences of an object
 *	format:	mvall oldname newname
 *
 */
int
f_mvall(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register int	i,j,k;	
	register union record *rp;
	register struct directory *dp;
	union record	record;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( (int)strlen(argv[2]) > NAMESIZE ) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "ERROR: name length limited to %d characters\n", NAMESIZE);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	  return TCL_ERROR;
	}

	/* rename the record itself */
	if( (dp = db_lookup( dbip, argv[1], LOOKUP_NOISY )) == DIR_NULL)
	  return TCL_ERROR;
	if( db_lookup( dbip, argv[2], LOOKUP_QUIET ) != DIR_NULL ) {
	  aexists( argv[2]);
	  return TCL_ERROR;
	}
	/*  Change object name in the directory. */
	if( db_rename( dbip, dp, argv[2] ) < 0 )  {
	  Tcl_AppendResult(interp, "error in rename to ", argv[2],
			   ", aborting\n", (char *)NULL);
	  TCL_ERROR_RECOVERY_SUGGESTION;
	  return TCL_ERROR;
	}

	/* Change name in the file */
	if( db_get( dbip,  dp, &record, 0 , 1) < 0 ) {
	  TCL_READ_ERR_return;
	}
	NAMEMOVE( argv[2], record.c.c_name );
	if( db_put( dbip, dp, &record, 0, 1 ) < 0 ) {
	  TCL_WRITE_ERR_return;
	}

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;
			if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
			  TCL_READ_ERR_return;
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
					  TCL_WRITE_ERR_return;
					}
				}
			}
			bu_free( (genptr_t)rp, "dir_nref recs" );
		}
	}
	return TCL_OK;
}

/*	F _ K I L L A L L
 *
 *	kill object[s] and
 *	remove all references to the object[s]
 *	format:	killall obj1 ... objn
 *
 */
int
f_killall(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  register int	i,j,k;
  register union record *rp = (union record *)NULL;
  register struct directory *dp;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);  /* allow interupts */
  else{
    if(rp)
      bu_free( (genptr_t)rp, "dir_nref recs" );

    return TCL_OK;
  }

  /* Examine all COMB nodes */
  for( i = 0; i < RT_DBNHASH; i++ )  {
    for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
      if( !(dp->d_flags & DIR_COMB) )
	continue;
again:
      if( (rp = db_getmrec( dbip, dp )) == (union record *)0 ) {
	(void)signal( SIGINT, SIG_IGN );
	TCL_READ_ERR_return;
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
	    Tcl_AppendResult(interp, "error in killing reference to '",
			     argv[k], "', exit MGED and retry\n", (char *)NULL);
	    TCL_ERROR_RECOVERY_SUGGESTION;
	    (void)signal( SIGINT, SIG_IGN );
	    bu_free( (genptr_t)rp, "dir_nref recs" );
	    return TCL_ERROR;
	  }
	  bu_free( (genptr_t)rp, "dir_nref recs" );
	  goto again;
	}
      }
      bu_free( (genptr_t)rp, "dir_nref recs" );
    }
  }

  /* ALL references removed...now KILL the object[s] */
  /* reuse argv[] */
  argv[0] = "kill";
  (void)signal( SIGINT, SIG_IGN );
  return f_kill( clientData, interp, argc, argv );
}


/*		F _ K I L L T R E E ( )
 *
 *	Kill ALL paths belonging to an object
 *
 */
int
f_killtree(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	register struct directory *dp;
	register int i;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else
	  return TCL_OK;

	for(i=1; i<argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY) ) == DIR_NULL )
			continue;
		db_functree( dbip, dp, killtree, killtree );
	}

	(void)signal( SIGINT, SIG_IGN );
	return TCL_OK;
}

/*
 *			K I L L T R E E
 */
void
killtree( dbip, dp )
struct db_i	*dbip;
register struct directory *dp;
{
  Tcl_AppendResult(interp, "KILL ", (dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
		   ":  ", dp->d_namep, "\n", (char *)NULL);

  eraseobj( dp );

  if( db_delete( dbip, dp) < 0 || db_dirdelete( dbip, dp ) < 0 ){
    TCL_DELETE_ERR("");
  }
}

int
f_debugdir(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  db_pr_dir( dbip );
  return TCL_OK;
}
