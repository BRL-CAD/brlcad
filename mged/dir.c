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
static const char RCSid[] = "@(#)$Header$ (BRL)";
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
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./mgedtcl.h"

#define BAD_EOF	(-1L)			/* eof_addr not set yet */

void	killtree();

/*
 *			D I R _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 *  a) all of the entries if called with an argument of 0, or
 *  b) the number of entries specified by the argument if > 0.
 */
struct directory **
dir_getspace(num_entries)
register int num_entries;
{
	register struct directory *dp;
	register int i;
	register struct directory **dir_basep;

	if(dbip == DBI_NULL)
	  return (struct directory **) 0;

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
  int c;
  int aflag = 0;		/* print all objects without formatting */
  int cflag = 0;		/* print combinations */
  int rflag = 0;		/* print regions */
  int sflag = 0;		/* print solids */
  struct directory **dirp;
  struct directory **dirp0 = (struct directory **)NULL;
  struct bu_vls vls;

  CHECK_DBI_NULL;

  if(argc < 1){
    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  bu_optind = 1;	/* re-init bu_getopt() */
  while ((c = bu_getopt(argc, argv, "acgrs")) != EOF) {
    switch (c) {
    case 'a':
      aflag = 1;
      break;
    case 'c':
      cflag = 1;
      break;
    case 'r':
      rflag = 1;
      break;
    case 's':
      sflag = 1;
      break;
    default:
      bu_vls_init(&vls);
      bu_vls_printf(&vls, "Unrecognized option - %c\n", c);
      Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
      bu_vls_free(&vls);
      return TCL_ERROR;
    }
  }
  argc -= (bu_optind - 1);
  argv += (bu_optind - 1);

  bu_vls_init(&vls);
  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    if(dirp0)
      bu_free( (genptr_t)dirp0, "dir_getspace dp[]" );
    bu_vls_free(&vls);

    return TCL_OK;
  }

  if (argc > 1) {
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

  if (aflag || cflag || rflag || sflag)
    vls_line_dpp(&vls, dirp0, (int)(dirp - dirp0),
		 aflag, cflag, rflag, sflag);
  else
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
  CHECK_DBI_NULL;

  if(argc < 1 || 1 < argc){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help memprint");
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

#if 0
  Tcl_AppendResult(interp, "Display manager free map:\n", (char *)NULL);
  rt_memprint( &(dmp->dm_map) );
#endif

  bu_log("Database free-storage map:\n");
  rt_memprint( &(dbip->dbi_freep) );

  return TCL_OK;
}

/*
 *  			D I R _ S U M M A R Y
 *
 * Summarize the contents of the directory by categories
 * (solid, comb, region).  If flag is != 0, it is interpreted
 * as a request to print all the names in that category (eg, DIR_SOLID).
 */
void
dir_summary(int flag)
{
	register struct directory *dp;
	register int i;
	static int sol, comb, reg;
	struct directory **dirp;
	struct directory **dirp0 = (struct directory **)NULL;
	struct bu_vls vls;

	if(dbip == DBI_NULL)
	  return;

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
			if( dp->d_flags & DIR_COMB )  {
				if( dp->d_flags & DIR_REGION )
					reg++;
				else
					comb++;
			}
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

	CHECK_DBI_NULL;

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

	db_update_nref( dbip, &rt_uniresource );
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

	if(dbip == DBI_NULL)
	  return 0;

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

    CHECK_DBI_NULL;

    if(argc < 1){
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
	register int sflag = (int)(((long)user_ptr3) & 0xFFFF);	/* only need lsb */

	RT_CK_TREE( comb_leaf );

	obj_name = (char *)object;
	if( strcmp( comb_leaf->tr_l.tl_name, obj_name ) )
		return;

	comb_name = (char *)comb_name_ptr;

	if (sflag)
	  Tcl_AppendElement(interp, comb_name);
	else
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
  register int	i,k;
  register int sflag = 0;
  register struct directory *dp;
  struct rt_db_internal intern;
  register struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;

  CHECK_DBI_NULL;

  if(argc < 2){
    struct bu_vls vls;

    bu_vls_init(&vls);
    bu_vls_printf(&vls, "help %s", argv[0]);
    Tcl_Eval(interp, bu_vls_addr(&vls));
    bu_vls_free(&vls);
    return TCL_ERROR;
  }

  if (strcmp(argv[1], "-s") == 0) {
    --argc;
    ++argv;

    if(argc < 2){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help find");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    sflag = 1;
  }

  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    if( comb )
  	rt_comb_ifree( &intern, &rt_uniresource );
    return TCL_OK;
  }

  /* Examine all COMB nodes */
  for( i = 0; i < RT_DBNHASH; i++ )  {
    for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
      if( !(dp->d_flags & DIR_COMB) )
	continue;

    	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
	{
		(void)signal( SIGINT, SIG_IGN );
		TCL_READ_ERR_return;
	}
    	comb = (struct rt_comb_internal *)intern.idb_ptr;
	for( k=0; k<argc; k++ )
	    	db_tree_funcleaf( dbip, comb, comb->tree, Find_ref, (genptr_t)argv[k], (genptr_t)dp->d_namep, (genptr_t)sflag );

    	rt_comb_ifree( &intern, &rt_uniresource );
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
	char tempstring_v4[NAMESIZE+2];

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	prefix = (char *)prefix_ptr;
	obj = (char *)obj_ptr;

	if( strcmp( comb_leaf->tr_l.tl_name, obj ) )
		return;

	bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
	if( dbip->dbi_version < 5 ) {
		(void)strcpy( tempstring_v4, prefix);
		(void)strcat( tempstring_v4, obj);
		comb_leaf->tr_l.tl_name = bu_strdup( tempstring_v4 );
	} else {
		comb_leaf->tr_l.tl_name = (char *)bu_malloc( strlen( prefix ) + strlen( obj ) + 1,
							     "Adding prefix" );
		(void)strcpy( comb_leaf->tr_l.tl_name , prefix);
		(void)strcat( comb_leaf->tr_l.tl_name , obj );
	}
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
	register int	i,k;	
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	char tempstring_v4[NAMESIZE+2];
	struct bu_vls tempstring_v5;
	char *tempstring;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help prefix");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	bu_vls_init( &tempstring_v5 );

	/* First, check validity, and change node names */
	for( i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY )) == DIR_NULL) {
			argv[i] = "";
			continue;
		}

		if( dbip->dbi_version < 5 && (int)(strlen(argv[1]) + strlen(argv[i])) > NAMESIZE) {
		  struct bu_vls tmp_vls;

		  bu_vls_init(&tmp_vls);
		  bu_vls_printf(&tmp_vls, "'%s%s' too long, must be less than %d characters.\n",
				argv[1], argv[i], NAMESIZE);
		  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		  bu_vls_free(&tmp_vls);

		  argv[i] = "";
		  continue;
		}

		if( dbip->dbi_version < 5 ) {
			(void) strcpy( tempstring_v4, argv[1]);
			(void) strcat( tempstring_v4, argv[i]);
			tempstring = tempstring_v4;
		} else {
			bu_vls_trunc( &tempstring_v5, 0 );
			bu_vls_strcpy( &tempstring_v5, argv[1] );
			bu_vls_strcat( &tempstring_v5, argv[i] );
			tempstring = bu_vls_addr( &tempstring_v5 );
		}

		if( db_lookup( dbip, tempstring, LOOKUP_QUIET ) != DIR_NULL ) {
			aexists( tempstring );
			argv[i] = "";
			continue;
		}
		/*  Change object name in the directory. */
		if( db_rename( dbip, dp, tempstring ) < 0 )  {
			bu_vls_free( &tempstring_v5 );
		  Tcl_AppendResult(interp, "error in rename to ", tempstring,
				   ", aborting\n", (char *)NULL);
		  TCL_ERROR_RECOVERY_SUGGESTION;
		  return TCL_ERROR;
		}
	}

	bu_vls_free( &tempstring_v5 );

	/* Examine all COMB nodes */
	for( i = 0; i < RT_DBNHASH; i++ )  {
		for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )  {
			if( !(dp->d_flags & DIR_COMB) )
				continue;

			if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
				TCL_READ_ERR_return;
			comb = (struct rt_comb_internal *)intern.idb_ptr;

			for( k=2; k<argc; k++ )
				db_tree_funcleaf( dbip, comb, comb->tree, Do_prefix,
					(genptr_t)argv[1], (genptr_t)argv[k], (genptr_t)NULL );
			if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) )
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
BU_EXTERN(void node_write, (struct db_i *dbip, struct directory *dp, genptr_t ptr));

void
node_write( dbip, dp, ptr )
struct db_i	*dbip;
register struct directory *dp;
genptr_t	ptr;
{
	struct rt_wdb	*keepfp = (struct rt_wdb *)ptr;
	struct bu_external	ext;

	RT_CK_WDB(keepfp);

	if( dp->d_nref++ > 0 )
		return;		/* already written */

	if( db_get_external( &ext, dp, dbip ) < 0 )
		READ_ERR_return;
	if( wdb_export_external( keepfp, &ext, dp->d_namep, dp->d_flags ) < 0 )
		WRITE_ERR_return;
}

int
f_keep(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct rt_wdb		*keepfp;
	register struct directory *dp;
	struct bu_vls		title;
	register int		i;
	struct db_i		*new_dbip;

	CHECK_DBI_NULL;

	if(argc < 3){
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
	if( (new_dbip = db_open( argv[1], "w" )) !=  DBI_NULL  &&
	    (keepfp = wdb_dbopen( new_dbip, RT_WDB_TYPE_DB_DISK ) ) != NULL )  {
		Tcl_AppendResult(interp, "keep:  appending to '", argv[1],
			   "'\n", (char *)NULL);
	} else {
		/* Create a new database */
		if( (keepfp = wdb_fopen( argv[1] ) ) == NULL )  {
			perror( argv[1] );
			return TCL_ERROR;
		}
	}
	
	/* ident record */
	bu_vls_init( &title );
	bu_vls_strcat( &title, "Parts of: " );
	bu_vls_strcat( &title, dbip->dbi_title );

	if( db_update_ident( keepfp->dbip, bu_vls_addr(&title), dbip->dbi_local2base ) < 0 )  {
		perror("fwrite");
		Tcl_AppendResult(interp, "db_update_ident() failed\n", (char *)NULL);
		wdb_close(keepfp);
		bu_vls_free( &title );
		return TCL_ERROR;
	}
	bu_vls_free( &title );

	for(i = 2; i < argc; i++) {
		if( (dp = db_lookup( dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL )
			continue;
		db_functree( dbip, dp, node_write, node_write, &rt_uniresource, (genptr_t)keepfp );
	}

	wdb_close(keepfp);

	return TCL_OK;
}


/*
 *			F _ T R E E
 *
 *	Print out a list of all members and submembers of an object.
 */
int
f_tree(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
{
	int		ret;
	struct bu_vls	vls;

	CHECK_DBI_NULL;
	bu_vls_init(&vls);

	/*
	 * The tree command is wrapped by tclscripts/tree.tcl and calls this
	 * routine with the name _mged_tree. So, we put back the original name.
	 */ 
	argv[0] = "tree";
	bu_build_cmd_vls(&vls, MGED_DB_NAME, argc, argv);

	if (setjmp(jmp_env) == 0)
		(void)signal( SIGINT, sig3);  /* allow interupts */
	else {
		bu_vls_free(&vls);
		return TCL_OK;
	}

	ret = Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);

	(void)signal(SIGINT, SIG_IGN);
	return ret;
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

	if( strcmp( comb_leaf->tr_l.tl_name, old_name ) )
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
	register int	i;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	struct bu_ptbl		stack;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help mvall");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( dbip->dbi_version < 5 && (int)strlen(argv[2]) > NAMESIZE ) {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "ERROR: name length limited to %d characters in v4 databases\n", NAMESIZE);
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
	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
		TCL_READ_ERR_return;
	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 ) {
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

			if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
				continue;
			comb = (struct rt_comb_internal *)intern.idb_ptr;

			bu_ptbl_reset( &stack );
			/* visit each leaf in the combination */
			comb_leaf = comb->tree;
			if( comb_leaf )
			{
				while( !done )
				{
					while( comb_leaf->tr_op != OP_DB_LEAF )
					{
						bu_ptbl_ins( &stack, (long *)comb_leaf );
						comb_leaf = comb_leaf->tr_b.tb_left;
					}
					if( !strcmp( comb_leaf->tr_l.tl_name, argv[1] ) )
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
			}

			if( changed )
			{
				if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) )
				{
					bu_ptbl_free( &stack );
					rt_comb_ifree( &intern, &rt_uniresource );
					TCL_WRITE_ERR_return;
				}
			}
			else
				rt_comb_ifree( &intern, &rt_uniresource );
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
f_killall(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register int	i,k;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2){
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

			if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
					") failure", (char *)NULL );
				ret = TCL_ERROR;
				continue;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB(comb);

			for( k=1; k<argc; k++ )  {
				int	code;

				code = db_tree_del_dbleaf( &(comb->tree), argv[k], &rt_uniresource );
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

			if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
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
f_killtree(
	ClientData clientData,
	Tcl_Interp *interp,
	int	argc,
	char	**argv)
{
	register struct directory *dp;
	register int i;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2){
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
		db_functree( dbip, dp, killtree, killtree, &rt_uniresource, (genptr_t)interp );
	}

	(void)signal( SIGINT, SIG_IGN );
	solid_list_callback();
	return TCL_OK;
}

/*
 *			K I L L T R E E
 */
void
killtree(dbip, dp, ptr)
struct db_i	*dbip;
struct directory *dp;
genptr_t	ptr;
{
	Tcl_Interp		*interp = (Tcl_Interp *)ptr;
	struct directory	*dpp[2] = {DIR_NULL, DIR_NULL};

	if (dbip == DBI_NULL)
		return;

	Tcl_AppendResult(interp, "KILL ", (dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
			 ":  ", dp->d_namep, "\n", (char *)NULL);

	dpp[0] = dp;
	eraseobjall(dpp);

	if (db_delete(dbip, dp) < 0 || db_dirdelete(dbip, dp) < 0) {
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
  CHECK_DBI_NULL;

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
