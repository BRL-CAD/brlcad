/*
 *			D I R . C
 *
 * Functions -
 *	dir_getspace	Allocate memory for table of directory entry pointers
 *	dir_print	Print table-of-contents of object file
 *	f_memprint	Debug, print memory & db free maps
 *	dir_nref	Count number of times each db element referenced
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

  if(argc < 1 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help memprint");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  Tcl_AppendResult(interp, "Display manager free map:\n", (char *)NULL);
  rt_memprint( &(dmp->dm_map) );
  Tcl_AppendResult(interp, "Database free granule map:\n", (char *)NULL);
  rt_memprint( &(dbip->dbi_freep) );

  return TCL_OK;
}

HIDDEN void
Count_refs( dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal *comb;
union tree		*comb_leaf;
genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	struct directory *dp;

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	if( (dp = db_lookup( dbip, comb_leaf->tr_l.tl_name, LOOKUP_QUIET)) != DIR_NULL )
		dp->d_nref++;
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
	register int		i,j;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;

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

			if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
				continue;
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			db_tree_funcleaf( dbip, comb, comb->tree, Count_refs, (genptr_t)NULL, (genptr_t)NULL, (genptr_t)NULL );
			rt_comb_ifree( &intern );
		}
	}
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

	if(argc < 1 || 1 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help tops");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
			if( !db_regexp_match( word, dp->d_namep ) )
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

    if(argc < 1 || MAXARGS < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help expand");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

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
	        if( !db_regexp_match( pattern, dp->d_namep ) )
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

HIDDEN void
Find_ref( dbip, comb, comb_leaf, object, comb_name_ptr, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal	*comb;
union tree		*comb_leaf;
genptr_t		object;
genptr_t		comb_name_ptr;
genptr_t		user_ptr3;
{
	char *obj_name;
	char *comb_name;

	RT_CK_TREE( comb_leaf );

	obj_name = (char *)object;
	if( strcmp( comb_leaf->tr_l.tl_name, obj_name ) )
		return;

	comb_name = (char *)comb_name_ptr;

	Tcl_AppendResult(interp, obj_name, ":  member of ", comb_name, "\n", (char *)NULL );
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
  struct rt_db_internal intern;
  register struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;

  if(argc < 1 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help find");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    if( comb )
  	rt_comb_ifree( &intern );
    return TCL_OK;
  }

  /* Examine all COMB nodes */
  for( i = 0; i < RT_DBNHASH; i++ )  {
    for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
      if( !(dp->d_flags & DIR_COMB) )
	continue;

    	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
	{
		(void)signal( SIGINT, SIG_IGN );
		TCL_READ_ERR_return;
	}
    	comb = (struct rt_comb_internal *)intern.idb_ptr;
	for( k=0; k<argc; k++ )
	    	db_tree_funcleaf( dbip, comb, comb->tree, Find_ref, (genptr_t)argv[k], (genptr_t)dp->d_namep, (genptr_t)NULL );

    	rt_comb_ifree( &intern );
    }
  }

  (void)signal( SIGINT, SIG_IGN );
  return TCL_OK;
}

HIDDEN void
Do_prefix( dbip, comb, comb_leaf, prefix_ptr, obj_ptr, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal *comb;
union tree		*comb_leaf;
genptr_t		prefix_ptr, obj_ptr, user_ptr3;
{
	char *prefix,*obj;
	char tempstring[NAMESIZE+2];

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	prefix = (char *)prefix_ptr;
	obj = (char *)obj_ptr;

	if( strncmp( comb_leaf->tr_l.tl_name, obj, NAMESIZE ) )
		return;

	(void)strcpy( tempstring, prefix);
	(void)strcat( tempstring, obj);
	bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
	comb_leaf->tr_l.tl_name = bu_strdup( tempstring );
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
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	char		tempstring[NAMESIZE+2];

	CHECK_READ_ONLY;

	if(argc < 3 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help prefix");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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

			if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
				TCL_READ_ERR_return;
			comb = (struct rt_comb_internal *)intern.idb_ptr;

			for( k=2; k<argc; k++ )
				db_tree_funcleaf( dbip, comb, comb->tree, Do_prefix,
					(genptr_t)argv[1], (genptr_t)argv[k], (genptr_t)NULL );
			if( rt_db_put_internal( dp, dbip, &intern ) )
				TCL_WRITE_ERR_return;
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
	struct rt_db_internal	intern;
	int			want;

	if( dp->d_nref++ > 0 )
		return;		/* already written */

	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
		READ_ERR_return;
	if( mk_export_fwrite( keepfp, dp->d_namep, intern.idb_ptr, intern.idb_type ) )
		WRITE_ERR_return;
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

	if(argc < 3 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help keep");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
	
	switch (localunit)
	{
	    case ID_NO_UNIT:
		bu_vls_strcat( &units, "none"); break;
	    case ID_UM_UNIT:
		bu_vls_strcat( &units, "um"); break;
	    case ID_MM_UNIT:
		bu_vls_strcat( &units, "mm"); break;
	    case ID_CM_UNIT:
		bu_vls_strcat( &units, "cm"); break;
	    case ID_M_UNIT:
		bu_vls_strcat( &units, "m"); break;
	    case ID_KM_UNIT:
		bu_vls_strcat( &units, "km"); break;
	    case ID_IN_UNIT:
		bu_vls_strcat( &units, "in"); break;
	    case ID_FT_UNIT:
		bu_vls_strcat( &units, "ft"); break;
	    case ID_YD_UNIT:
		bu_vls_strcat( &units, "yd"); break;
	    case ID_MI_UNIT:
		bu_vls_strcat( &units, "mi"); break;
	}
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

  if(argc < 2 || MAXARGS < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help tree");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

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
  register int	i;
  register struct directory *nextdp;
  struct rt_db_internal intern;
  struct rt_comb_internal *comb;

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

  if( !(dp->d_flags & DIR_COMB) )
    return;

  /*
   *  This node is a combination (eg, a directory).
   *  Process all the arcs (eg, directory members).
   */

  if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
	READ_ERR_return;
  comb = (struct rt_comb_internal *)intern.idb_ptr;

  if( comb->tree )
  {
  	int node_count;
  	int actual_count;
  	struct rt_tree_array *rt_tree_array;

	if( comb->tree && db_ck_v4gift_tree( comb->tree ) < 0 )
	{
		db_non_union_push( comb->tree );
		if( db_ck_v4gift_tree( comb->tree ) < 0 )
		{
			Tcl_AppendResult(interp, "Cannot flatten tree for listing\n", (char *)NULL );
			return;
		}
	}
	node_count = db_tree_nleaves( comb->tree );
	if( node_count > 0 )
	{
		rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
			sizeof( struct rt_tree_array ), "tree list" );
		actual_count = (struct rt_tree_array *)db_flatten_tree( rt_tree_array, comb->tree, OP_UNION ) - rt_tree_array;
		if( actual_count > node_count )  bu_bomb("rt_comb_v4_export() array overflow!");
		if( actual_count < node_count )  bu_log("WARNING rt_comb_v4_export() array underflow! %d < %d", actual_count, node_count);
	}

  	for( i=0 ; i<actual_count ; i++ )
  	{
  		char op;

		switch( rt_tree_array[i].tl_op )
		{
			case OP_UNION:
				op = 'u';
				break;
			case OP_INTERSECT:
				op = '+';
				break;
			case OP_SUBTRACT:
				op = '-';
				break;
			default:
				op = '?';
				break;
		}

  		if( (nextdp = db_lookup( dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY )) == DIR_NULL )
  		{
  			int j;
			struct bu_vls tmp_vls;
  			
			for( j=0; j<pathpos+1; j++) 
				Tcl_AppendResult(interp, "\t", (char *)NULL);

			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls, "%c ", op);
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);

			Tcl_AppendResult(interp, rt_tree_array[i].tl_tree->tr_l.tl_name, "\n", (char *)NULL);
		}
  		else
			printnode( nextdp, pathpos+1, op );
  	}
	bu_free( (char *)rt_tree_array, "printnode: rt_tree_array" );
  }
  rt_comb_ifree( &intern );
}


HIDDEN void
Change_name( dbip, comb, comb_leaf, old_ptr, new_ptr )
struct db_i		*dbip;
struct rt_comb_internal *comb;
union tree		*comb_leaf;
genptr_t		old_ptr, new_ptr;
{
	char	*old_name, *new_name;

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	old_name = (char *)old_ptr;
	new_name = (char *)new_ptr;

	if( strncmp( comb_leaf->tr_l.tl_name, old_name, NAMESIZE ) )
		return;

	bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
	comb_leaf->tr_l.tl_name = bu_strdup( new_name );
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
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	struct bu_ptbl		stack;

	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help mvall");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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
	if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
		TCL_READ_ERR_return;
	if( rt_db_put_internal( dp, dbip, &intern ) < 0 ) {
		TCL_WRITE_ERR_return;
	}

	bu_ptbl_init( &stack, 64, "combination stack for f_mvall" );

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			union tree	*comb_leaf;
			int		done=0;
			int		changed=0;

			if( !(dp->d_flags & DIR_COMB) )
				continue;

			if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )
				continue;
			comb = (struct rt_comb_internal *)intern.idb_ptr;

			bu_ptbl_reset( &stack );
			/* visit each leaf in the combination */
			comb_leaf = comb->tree;
			while( !done )
			{
				while( comb_leaf->tr_op != OP_DB_LEAF )
				{
					bu_ptbl_ins( &stack, (long *)comb_leaf );
					comb_leaf = comb_leaf->tr_b.tb_left;
				}
				if( !strncmp( comb_leaf->tr_l.tl_name, argv[1], NAMESIZE ) )
				{
					bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
					comb_leaf->tr_l.tl_name = bu_strdup( argv[2] );
					changed = 1;
				}

				if( BU_PTBL_END( &stack ) < 1 )
				{
					done = 1;
					break;
				}
				comb_leaf = (union tree *)BU_PTBL_GET( &stack, BU_PTBL_END( &stack )-1 );
				if( comb_leaf->tr_op != OP_DB_LEAF )
				{
					bu_ptbl_rm( &stack, (long *)comb_leaf );
					comb_leaf = comb_leaf->tr_b.tb_right;
				}
			}

			if( changed )
			{
				if( rt_db_put_internal( dp, dbip, &intern ) )
				{
					bu_ptbl_free( &stack );
					rt_comb_ifree( &intern );
					TCL_WRITE_ERR_return;
				}
			}
			else
				rt_comb_ifree( &intern );
		}
	}
	bu_ptbl_free( &stack );
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
	register int	i,k;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	CHECK_READ_ONLY;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help killall");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( setjmp( jmp_env ) == 0 )
		(void)signal( SIGINT, sig3);  /* allow interupts */
	else{
		/* Free intern? */
		return TCL_OK;
	}

	ret = TCL_OK;

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;

			if( rt_db_get_internal( &intern, dp, dbip, (mat_t *)NULL ) < 0 )  {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
					") failure", (char *)NULL );
				ret = TCL_ERROR;
				continue;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB(comb);

			for( k=1; k<argc; k++ )  {
				int	code;

				code = db_tree_del_dbleaf( &(comb->tree), argv[k] );
				if( code == -1 )  continue;	/* not found */
				if( code == -2 )  continue;	/* empty tree */
				if( code < 0 )  {
					Tcl_AppendResult(interp, "  ERROR_deleting ",
						dp->d_namep, "/", argv[k],
						"\n", (char *)NULL);
					ret = TCL_ERROR;
				} else {
					Tcl_AppendResult(interp, "deleted ",
						dp->d_namep, "/", argv[k],
						"\n", (char *)NULL);
				}
			}

			if( rt_db_put_internal( dp, dbip, &intern ) < 0 )  {
				Tcl_AppendResult(interp, "ERROR: Unable to write new combination into database.\n", (char *)NULL);
				ret = TCL_ERROR;
				continue;
			}
		}
	}

	if( ret != TCL_OK )  {
		Tcl_AppendResult(interp, "KILL skipped because of earlier errors.\n", (char *)NULL);
		return ret;
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

	CHECK_READ_ONLY;

	if(argc < 2 || MAXARGS < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help killtree");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

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

	eraseobjall( dp );

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
  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help debugdir");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  db_pr_dir( dbip );
  return TCL_OK;
}
