/*
 *			C O N C A T . C
 *
 *  Functions -
 *	f_dup()		checks for dup names before cat'ing of two files
 *	f_concat()	routine to cat another GED file onto end of current file
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A. Applin
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 1990 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSconcat[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <pwd.h>
#include <signal.h>
#include <math.h>
#include <time.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "db.h"
#include "raytrace.h"
#include "externs.h"
#include "./ged.h"
#include "./sedit.h"

int			num_dups;
struct directory	**dup_dirp;

char	new_name[NAMESIZE];
char	prestr[NAMESIZE];
int	ncharadd;

/*
 *			M G E D _ D I R _ C H E C K
 *
 * Check a name against the global directory.
 */
int
mged_dir_check( input_dbip, name, laddr, len, flags )
register struct db_i	*input_dbip;
register char		*name;
long			laddr;
int			len;
int			flags;
{
	struct directory	*dupdp;
	char			local[NAMESIZE+2];

	if( input_dbip->dbi_magic != DBI_MAGIC )  bu_bomb("mged_dir_check:  bad dbip\n");

	/* Add the prefix, if any */
	if( ncharadd > 0 )  {
		(void)strncpy( local, prestr, ncharadd );
		(void)strncpy( local+ncharadd, name, NAMESIZE-ncharadd );
	} else {
		(void)strncpy( local, name, NAMESIZE );
	}
	local[NAMESIZE] = '\0';
		
	/* Look up this new name in the existing (main) database */
	if( (dupdp = db_lookup( dbip, local, LOOKUP_QUIET )) != DIR_NULL )  {
		/* Duplicate found, add it to the list */
		num_dups++;
		*dup_dirp++ = dupdp;
	}
	return 0;
}

/*
 *
 *			F _ D U P ( )
 *
 *  Check for duplicate names in preparation for cat'ing of files
 *
 *  Usage:  dup file.g [prefix]
 */
int
f_dup(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
  struct db_i		*newdbp = DBI_NULL;
  struct directory	**dirp0 = (struct directory **)NULL;
  int status = TCL_OK;
  struct bu_vls vls;

  if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
    return TCL_ERROR;

  bu_vls_init(&vls);
  if( setjmp( jmp_env ) == 0 )
    (void)signal( SIGINT, sig3);	/* allow interupts */
  else{
    bu_vls_free(&vls);

    if(dirp0)
      bu_free( (genptr_t)dirp0, "dir_getspace array" );

    if(newdbp && newdbp->dbi_magic == DBI_MAGIC)
      db_close( newdbp );

    return TCL_OK;
  }

  /* get any prefix */
  if( argc < 3 ) {
    prestr[0] = '\0';
  } else {
    (void)strcpy(prestr, argv[2]);
  }
  num_dups = 0;
  if( (ncharadd = strlen( prestr )) > 12 )  {
    ncharadd = 12;
    prestr[12] = '\0';
  }

  /* open the input file */
  if( (newdbp = db_open( argv[1], "r" )) == DBI_NULL )  {
    perror( argv[1] );
    Tcl_AppendResult(interp, "dup: Can't open ", argv[1], "\n", (char *)NULL);
    status = TCL_ERROR;
    goto end;
  }

  Tcl_AppendResult(interp, "\n*** Comparing ", dbip->dbi_filename,
		   "  with ", argv[1], " for duplicate names\n", (char *)NULL);
  if( ncharadd ) {
    Tcl_AppendResult(interp, "  For comparison, all names in ",
		     argv[1], " prefixed with:  ", prestr, "\n", (char *)NULL);
  }

  /* Get array to hold names of duplicates */
  if( (dup_dirp = dir_getspace(0)) == (struct directory **) 0) {
    Tcl_AppendResult(interp, "f_dup: unable to get memory\n", (char *)NULL);
    status = TCL_ERROR;
    db_close( newdbp );
    goto end;
  }
  dirp0 = dup_dirp;

  /* Scan new database for overlaps */
  if( db_scan( newdbp, mged_dir_check, 0 ) < 0 )  {
    Tcl_AppendResult(interp, "dup: db_scan failure\n", (char *)NULL);
    status = TCL_ERROR;
    bu_free( (genptr_t)dirp0, "dir_getspace array" );
    db_close( newdbp );
    goto end;
  }

  vls_col_pr4v(&vls, dirp0, (int)(dup_dirp - dirp0));
  bu_vls_printf(&vls, "\n -----  %d duplicate names found  -----\n",num_dups);
  Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
  bu_free( (genptr_t)dirp0, "dir_getspace array" );
  db_close( newdbp );

end:
  (void)signal( SIGINT, SIG_IGN );
  bu_vls_free(&vls);
  return status;
}

HIDDEN void
Do_update( dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3 )
struct db_i		*dbip;
struct rt_comb_internal *comb;
union tree		*comb_leaf;
genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	char	mref[NAMESIZE+2];
	char	*prestr;
	int	*ncharadd;

	RT_CK_DBI( dbip );
	RT_CK_TREE( comb_leaf );

	ncharadd = (int *)user_ptr1;
	prestr = (char *)user_ptr2;

	(void)strncpy( mref, prestr, *ncharadd );
	(void)strncpy( mref+(*ncharadd),
		comb_leaf->tr_l.tl_name,
		NAMESIZE-1-(*ncharadd) );
	bu_free( comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name" );
	comb_leaf->tr_l.tl_name = bu_strdup( mref );
}

/*
 *			M G E D _ D I R _ A D D
 *
 *  Add a solid or conbination from an auxillary database
 *  into the primary database.
 */
int
mged_dir_add( input_dbip, name, laddr, len, flags )
register struct db_i	*input_dbip;
register char		*name;
long			laddr;
int			len;
int			flags;
{
	register struct directory *input_dp;
	register struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	char			local[NAMESIZE+2+2];

	if( input_dbip->dbi_magic != DBI_MAGIC )  bu_bomb("mged_dir_add:  bad dbip\n");

	/* Add the prefix, if any */
	if( ncharadd > 0 )  {
		(void)strncpy( local, prestr, ncharadd );
		(void)strncpy( local+ncharadd, name, NAMESIZE-1-ncharadd );
	} else {
		(void)strncpy( local, name, NAMESIZE );
	}
	local[NAMESIZE-1] = '\0';
		
	/* Look up this new name in the existing (main) database */
	if( (dp = db_lookup( dbip, local, LOOKUP_QUIET )) != DIR_NULL )  {
		register int	c;
		char		loc2[NAMESIZE+2+2];

		/* This object already exists under the (prefixed) name */
		/* Protect the database against duplicate names! */
		/* Change object names, but NOT any references made by combinations. */
		(void)strncpy( loc2, local, NAMESIZE );
		/* Shift name right two characters, and further prefix */
		strncpy( local+2, loc2, NAMESIZE-2 );
		local[1] = '_';			/* distinctive separater */
		local[NAMESIZE-1] = '\0';	/* ensure null termination */

		for( c = 'A'; c <= 'Z'; c++ )  {
			local[0] = c;
			if( (dp = db_lookup( dbip, local, LOOKUP_QUIET )) == DIR_NULL )
				break;
		}
		if( c > 'Z' )  {
			bu_log("mged_dir_add: Duplicate of name '%s', ignored\n",
				local );
			return 0;
		}
		bu_log("mged_dir_add: Duplicate of '%s' given new name '%s'\nYou should have used the 'dup' command to detect this,\nand then specified a prefix for the 'concat' command.\n",
			loc2, local );
	}

	/* First, register this object in input database */
	if( (input_dp = db_diradd( input_dbip, name, laddr, len, flags)) == DIR_NULL )
		return(-1);

	/* Then, register a new object in the main database */
	if( (dp = db_diradd( dbip, local, -1L, len, flags)) == DIR_NULL )
		return(-1);
	if( db_alloc( dbip, dp, len ) < 0 )
		return(-1);

	if( rt_db_get_internal( &intern, input_dp, input_dbip, (mat_t *)NULL ) < 0 )
	{
		READ_ERR;
		if( db_delete( dbip, dp ) < 0 ||
		    db_dirdelete( dbip, dp ) < 0 )  {
		    	DELETE_ERR(local);
		}
	    	/* Abort processing on first error */
		return -1;
	}

	/* Update the name, and any references */
	if( flags & DIR_SOLID )
	{
		bu_log("adding solid '%s'\n", local );
		if ((ncharadd + strlen(name)) >= (unsigned)NAMESIZE)
			bu_log("WARNING: solid name \"%s%s\" truncated to \"%s\"\n",
				prestr,name, local);

		NAMEMOVE( local, dp->d_namep );
	}
	else
	{
		register int i;
		char	mref[NAMESIZE+2];

		bu_log("adding  comb '%s'\n", local );
		NAMEMOVE( local, dp->d_namep );

		/* Update all the member records */
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		if( ncharadd && comb->tree )
		{
			db_tree_funcleaf( dbip, comb, comb->tree, Do_update,
				(genptr_t)&ncharadd, (genptr_t)prestr, (genptr_t)NULL );
		}
	}

	if( rt_db_put_internal( dp, dbip, &intern ) < 0 )
	{
		bu_log( "Failed writing %s to database\n", dp->d_namep );
		return( -1 );
	}

	return 0;
}


/*
 *			F _ C O N C A T
 *
 *  Concatenate another GED file into the current file.
 *  Interrupts are not permitted during this function.
 *
 *  Usage:  dup file.g [prefix]
 *
 *  NOTE:  If a prefix is not given on the command line,
 *  then the users insist that they be prompted for the prefix,
 *  to prevent inadvertently sucking in a non-prefixed file.
 */
int
f_concat(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	struct db_i		*newdbp;
	int bad = 0;

	if(mged_cmd_arg_check(argc, argv, (struct funtab *)NULL))
	  return TCL_ERROR;

	/* get any prefix */
	if( argc < 3 )  {
	  Tcl_AppendResult(interp, MORE_ARGS_STR,
			   "concat: Enter prefix string or / for no prefix: ",
			   (char *)NULL);
	  return TCL_ERROR;
	}

	if( strcmp( argv[2], "/" ) == 0 )  {
		/* No prefix desired */
		(void)strcpy(prestr, "\0");
	} else {
		(void)strcpy(prestr, argv[2]);
	}

	if( (ncharadd = strlen( prestr )) > 12 )  {
		ncharadd = 12;
		prestr[12] = '\0';
	}

	/* open the input file */
	if( (newdbp = db_open( argv[1], "r" )) == DBI_NULL )  {
		perror( argv[1] );
		Tcl_AppendResult(interp, "concat: Can't open ",
				 argv[1], "\n", (char *)NULL);
		return TCL_ERROR;
	}

	/* Scan new database, adding everything encountered. */
	if( db_scan( newdbp, mged_dir_add, 1 ) < 0 )  {
	  Tcl_AppendResult(interp, "concat: db_scan failure\n", (char *)NULL);
	  bad = 1;	
	  /* Fall through, to close off database */
	}

	/* Free all the directory entries, and close the input database */
	db_close( newdbp );

	sync();		/* just in case... */

	return bad ? TCL_ERROR : TCL_OK;
}
