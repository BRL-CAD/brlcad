/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file dir.c
 *
 * Functions -
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
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif


#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"


#define BAD_EOF	(-1L)			/* eof_addr not set yet */

void	killtree(struct db_i *dbip, struct directory *dp, genptr_t ptr);


/*
 *			F _ M E M P R I N T
 *
 *  Debugging aid:  dump memory maps
 */
int
f_memprint(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
cmd_glob(int *argcp, char **argv, int maxargs)
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

	FOR_ALL_DIRECTORY_START(dp, dbip) {
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
	} FOR_ALL_DIRECTORY_END;

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

HIDDEN void
Do_prefix(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t prefix_ptr, genptr_t obj_ptr, genptr_t user_ptr3)
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
f_prefix(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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
	FOR_ALL_DIRECTORY_START(dp, dbip) {
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
	} FOR_ALL_DIRECTORY_END;
	return TCL_OK;
}

HIDDEN void
Change_name(struct db_i *dbip, struct rt_comb_internal *comb, union tree *comb_leaf, genptr_t old_ptr, genptr_t new_ptr)
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

/*	F _ K I L L A L L
 *
 *	kill object[s] and
 *	remove all references to the object[s]
 *	format:	killall obj1 ... objn
 *
 */
int
cmd_killall(ClientData	clientData,
	    Tcl_Interp	*interp,
	    int		argc,
	    char	**argv)
{
#if 0
	CHECK_DBI_NULL;

	return wdb_killall_cmd(wdbp, interp, argc, argv);
#else
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
	FOR_ALL_DIRECTORY_START(dp, dbip) {
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
	} FOR_ALL_DIRECTORY_END;

	if( ret != TCL_OK )  {
		Tcl_AppendResult(interp, "KILL skipped because of earlier errors.\n", (char *)NULL);
		return ret;
	}

	/* ALL references removed...now KILL the object[s] */
	/* reuse argv[] */
	argv[0] = "kill";
	(void)signal( SIGINT, SIG_IGN );
	return cmd_kill( clientData, interp, argc, argv );
#endif
}


/*		F _ K I L L T R E E ( )
 *
 *	Kill ALL paths belonging to an object
 *
 */
int
cmd_killtree(ClientData	clientData,
	     Tcl_Interp *interp,
	     int	argc,
	     char	**argv)
{
#if 0
	CHECK_DBI_NULL;

	return wdb_kill_cmd(wdbp, interp, argc, argv);
#else
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
#endif
}

/*
 *			K I L L T R E E
 */
void
killtree(struct db_i *dbip, struct directory *dp, genptr_t ptr)
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
f_debugdir(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
