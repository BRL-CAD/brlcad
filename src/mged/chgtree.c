/*                       C H G T R E E . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file chgtree.c
 *
 * This module contains functions which change the tree structure
 * of the model, and delete solids or combinations or combination elements.
 *
 * Functions -
 *	f_name		rename an object
 *	f_copy		copy a solid
 *	f_instance	create an instance of something
 *	f_region	add solids to a region or create the region
 *	f_kill		remove an object or several from the description
 *	f_group		"grouping" command
 *	f_rm		delete members of a combination
 *	f_copy_inv	copy cyl and position at "end" of original cyl
 *
 *  Authors -
 *	Michael John Muuss
 *	Keith A Applin
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

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "rtgeom.h"

#include "./sedit.h"
#include "./ged.h"
#include "./mged_solid.h"
#include "./mged_dm.h"
#include "./cmd.h"


extern void solid_list_callback(void); /* chgview.c */
extern struct db_tree_state	mged_initial_tree_state;	/* dodraw.c */
extern struct bn_tol		mged_tol;	/* from ged.c */
extern struct rt_tess_tol	mged_ttol;	/* from ged.c */
extern int			classic_mged;

static char *really_delete="tk_messageBox -icon question -title {Are you sure?}\
 -type yesno -message {If you delete the \"_GLOBAL\" object you will be losing some important information\
 such as your preferred units and the title of the database. Do you really want to do this?}";

void	aexists(char *name);


/* Remove an object or several from the description */
/* Format: kill [-f] object1 object2 .... objectn	*/
int
cmd_kill(ClientData	clientData,
	 Tcl_Interp	*interp,
	 int		argc,
	 char		**argv)
{
#if 0
	int ret;

	CHECK_DBI_NULL;

	ret = wdb_kill_cmd(wdbp, interp, argc, argv);
	solid_list_callback();

	return ret;
#else
	register int		i;
	struct directory	*dp;
	struct directory	*dpp[2] = {DIR_NULL, DIR_NULL};
	int			is_phony;
	int			verbose = LOOKUP_NOISY;
	int			force=0;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help kill");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( argc > 1 && strcmp( argv[1], "-f" ) == 0 )  {
		verbose = LOOKUP_QUIET;
		force = 1;
		argc--;
		argv++;
	}

	for( i = 1; i < argc; i++ )  {
		if( (dp = db_lookup( dbip,  argv[i], verbose )) != DIR_NULL )  {
			if( !force && dp->d_major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY && dp->d_minor_type == 0 ) {
				/* kill the _GLOBAL object?? */
				if( classic_mged || tkwin == NULL ) {
					/* Tk is not available */
					bu_log( "You attempted to delete the _GLOBAL object.\n" );
					bu_log( "\tIf you delete the \"_GLOBAL\" object you will be losing some important information\n" );
					bu_log( "\tsuch as your preferred units and the title of the database.\n" );
					bu_log( "\tUse the \"-f\" option, if you really want to do this.\n" );
					continue;
				} else {
					/* Use tk_messageBox to question user */
					Tcl_ResetResult( interp );
					if( Tcl_Eval( interp, really_delete ) != TCL_OK ) {
						bu_bomb( "Tcl_Eval() failed!!!\n" );
					}
					if( strcmp( Tcl_GetStringResult( interp ), "yes" ) ) {
						Tcl_ResetResult( interp );
						continue;
					}
					Tcl_ResetResult( interp );
				}
			}
			is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);
			dpp[0] = dp;
			eraseobjall(dpp);
			/* eraseobjall() does db_dirdelete() on phony entries, don't re-do. */
			if( is_phony )  continue;

			if( db_delete( dbip, dp ) < 0 ||
			    db_dirdelete( dbip, dp ) < 0 )  {
			  /* Abort kill processing on first error */
			  TCL_DELETE_ERR_return(argv[i]);
			}
		}
	}

	solid_list_callback();
	return TCL_OK;
#endif
}

/* Copy a cylinder and position at end of original cylinder

 *	Used in making "wires"
 *
 * Format: cpi old new
 */
int
f_copy_inv(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	register struct directory *proto;
	register struct directory *dp;
	struct rt_db_internal internal;
	struct rt_tgc_internal *tgc_ip;
	int id;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help cpi");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( (proto = db_lookup( dbip,  argv[1], LOOKUP_NOISY )) == DIR_NULL )
	  return TCL_ERROR;

	if( db_lookup( dbip,  argv[2], LOOKUP_QUIET ) != DIR_NULL )  {
	  aexists( argv[2] );
	  return TCL_ERROR;
	}

	if( (id = rt_db_get_internal( &internal, proto, dbip, (fastf_t *)NULL, &rt_uniresource )) < 0 )  {
		TCL_READ_ERR_return;
	}
	/* make sure it is a TGC */
	if( id != ID_TGC )
	{
	  Tcl_AppendResult(interp, "f_copy_inv: ", argv[1],
			   " is not a cylinder\n", (char *)NULL);
		rt_db_free_internal( &internal, &rt_uniresource );
		return TCL_ERROR;
	}
	tgc_ip = (struct rt_tgc_internal *)internal.idb_ptr;

	/* translate to end of "original" cylinder */
	VADD2( tgc_ip->v , tgc_ip->v , tgc_ip->h );

	/* no interuprts */
	(void)signal( SIGINT, SIG_IGN );

	if( (dp = db_diradd( dbip, argv[2], -1L, 0, proto->d_flags, &proto->d_minor_type)) == DIR_NULL )  {
		TCL_ALLOC_ERR_return;
	}

	if( rt_db_put_internal( dp, dbip, &internal, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR_return;
	}

	{
	  char *av[3];

	  av[0] = "e";
	  av[1] = argv[2]; /* depends on solid name being in argv[2] */
	  av[2] = NULL;

	  /* draw the new solid */
	  (void)cmd_draw( clientData, interp, 2, av );
	}

	if(state == ST_VIEW) {
	  char *av[3];

	  av[0] = "sed";
	  av[1] = argv[2];  /* new name in argv[2] */
	  av[2] = NULL;

	  /* solid edit this new cylinder */
	  (void)f_sed( clientData, interp, 2, av );
	}

	return TCL_OK;
}

/*
 *			F _ A R C E D
 *
 *  Allow editing of the matrix, etc., along an arc in the database
 *  from the command line, when NOT in an edit state.
 *  Used mostly to facilitate shell script writing.
 *
 *  Syntax:  arced a/b ("anim" command)
 *	arced a/b matrix rmul translate dx dy dz
 *
 *  Extensions should be formulated along the lines of the "anim"
 *  commands from the script language.
 */
int
f_arced(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct animate		*anp;
	struct directory	*dp;
	mat_t			stack;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	union tree		*tp;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help arced");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Arc edit" ) )  return TCL_ERROR;

	if( !strchr( argv[1], '/' ) )  {
	  Tcl_AppendResult(interp, "arced: bad path specification '", argv[1],
			   "'\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( !(anp = db_parse_1anim( dbip, argc, (const char **)argv ) ) )  {
	  Tcl_AppendResult(interp, "arced: unable to parse command\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( anp->an_path.fp_len < 2 )  {
		Tcl_AppendResult(interp, "arced: path spec has insufficient elements\n", (char *)NULL);
		return TCL_ERROR;
	}
#if 0
	if( anp->an_path.fp_len != 2 )  {
		char	*thepath = db_path_to_string( &(anp->an_path) );
		bu_log("arced: '%s' is not a 2-element path spec\n");
		bu_free( thepath, "path" );
		db_free_1anim( anp );
		return TCL_ERROR;
	}
#endif

#if 0
	db_write_anim(stdout, anp);	/* XXX temp */
#endif

	/* Only the matrix rarc, lmul, and rmul directives are useful here */
	MAT_IDN( stack );

	/* Load the combination into memory */
	dp = anp->an_path.fp_names[anp->an_path.fp_len-2];
	RT_CK_DIR(dp);
	if( (dp->d_flags & DIR_COMB) == 0 )  {
	  Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
	  return TCL_ERROR;
	}
	if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )  {
		db_free_1anim( anp );
		TCL_READ_ERR_return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);
	if( !comb->tree )  {
		Tcl_AppendResult(interp, dp->d_namep, ": empty combination\n", (char *)NULL);
		goto fail;
	}

	/* Search for first mention of arc */
	if( (tp = db_find_named_leaf( comb->tree, anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep )) == TREE_NULL )  {
		Tcl_AppendResult(interp, "Unable to find instance of '",
			anp->an_path.fp_names[anp->an_path.fp_len-1]->d_namep,
			"' in combination '",
			anp->an_path.fp_names[anp->an_path.fp_len-2]->d_namep,
			"', error\n", (char *)NULL);
		goto fail;
	}

	/* Found match.  Update tl_mat in place. */
	if( !tp->tr_l.tl_mat )
		tp->tr_l.tl_mat = bn_mat_dup( bn_mat_identity );

	if( db_do_anim( anp, stack, tp->tr_l.tl_mat, NULL ) < 0 )  {
		goto fail;
	}

	if( bn_mat_is_identity( tp->tr_l.tl_mat ) )  {
		bu_free( (genptr_t)tp->tr_l.tl_mat, "tl_mat" );
		tp->tr_l.tl_mat = (matp_t)NULL;
	}

	if( rt_db_put_internal( dp, dbip, &intern, &rt_uniresource ) < 0 )  {
		TCL_WRITE_ERR;
		goto fail;
	}
	db_free_1anim( anp );
	return TCL_OK;

fail:
	rt_db_free_internal( &intern, &rt_uniresource );
	db_free_1anim( anp );
	return TCL_ERROR;
}

/*
 *			C M D _ P A T H L I S T
 *
 *  Given the name(s) of a database node(s), return a TCL list of all
 *  paths from there to each leaf below it.
 *
 *  Similar to the "tree" command, only suitable for programs, not humans.
 */
int
cmd_pathlist(ClientData	clientData,
	     Tcl_Interp	*interp,
	     int	argc,
	     char	**argv)
{
  CHECK_DBI_NULL;

  return wdb_pathlist_cmd(wdbp, interp, argc, argv);
}

/*
 *			F I N D _ S O L I D _ W I T H _ P A T H
 */
struct solid *
find_solid_with_path(register struct db_full_path *pathp)
{
	register struct solid	*sp;
	int			count = 0;
	struct solid		*ret = (struct solid *)NULL;

	RT_CK_FULL_PATH(pathp);

	FOR_ALL_SOLIDS(sp, &dgop->dgo_headSolid)  {
		if( !db_identical_full_paths( pathp, &sp->s_fullpath ) )  continue;

		/* Paths are the same */
		ret = sp;
		count++;
	}
	if( count > 1 ){
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls,"find_solid_with_path() found %d matches\n", count);
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
	  bu_vls_free(&tmp_vls);
	}

	return ret;
}

/*
 *			C M D _ O E D
 *
 *  Transition from VIEW state to OBJECT EDIT state in a single command,
 *  rather than requiring "press oill", "ill leaf", "matpick a/b".
 *
 *  Takes two parameters which when combined represent the full path to
 *  the reference solid of the object to be edited.
 *  The conceptual slash between the two strings signifies which
 *  matrix is to be edited.
 */
int
cmd_oed(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct db_full_path	lhs;
	struct db_full_path	rhs;
	struct db_full_path	both;
	char			*new_argv[4];
	char			number[32];

	CHECK_DBI_NULL;

	if(argc < 3 || 3 < argc){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help oed");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	if( not_state( ST_VIEW, "Object Illuminate" ) )  {
		return TCL_ERROR;
	}
	if(BU_LIST_IS_EMPTY(&dgop->dgo_headSolid)) {
		Tcl_AppendResult(interp, "no solids in view", (char *)NULL);
		return TCL_ERROR;
	}

	if( db_string_to_path( &lhs, dbip, argv[1] ) < 0 )  {
		Tcl_AppendResult(interp, "bad lhs path", (char *)NULL);
		return TCL_ERROR;
	}
	if( db_string_to_path( &rhs, dbip, argv[2] ) < 0 )  {
		db_free_full_path( &lhs );
		Tcl_AppendResult(interp, "bad rhs path", (char *)NULL);
		return TCL_ERROR;
	}
	if( rhs.fp_len <= 0 )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		Tcl_AppendResult(interp, "rhs must not be null", (char *)NULL);
		return TCL_ERROR;
	}

	db_full_path_init( &both );
	db_dup_full_path( &both, &lhs );
	db_append_full_path( &both, &rhs );
#if 0
	db_pr_full_path( "lhs ", &lhs );
	db_pr_full_path( "rhs ", &rhs );
	db_pr_full_path( "both", &both);
#endif

	/* Patterned after  ill_common() ... */
	illump = BU_LIST_NEXT(solid, &dgop->dgo_headSolid);/* any valid solid would do */
	edobj = 0;		/* sanity */
	movedir = 0;		/* No edit modes set */
	MAT_IDN( modelchanges );	/* No changes yet */
	(void)chg_state( ST_VIEW, ST_O_PICK, "internal change of state");
	/* reset accumulation local scale factors */
	acc_sc[0] = acc_sc[1] = acc_sc[2] = 1.0;
	new_mats();

	/* Find the one solid, set s_iflag UP, point illump at it */
	illump = find_solid_with_path( &both );
	if( !illump )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "Unable to find solid matching path", (char *)NULL);
		illump = 0;
		(void)chg_state( ST_O_PICK, ST_VIEW, "error recovery");
		return TCL_ERROR;
	}
	(void)chg_state( ST_O_PICK, ST_O_PATH, "internal change of state");

	/* Select the matrix */
#if 0
	bu_log("matpick %d\n", lhs.fp_len);
#endif
	sprintf( number, "%d", lhs.fp_len );
	new_argv[0] = "matpick";
	new_argv[1] = number;
	new_argv[2] = NULL;
	if( f_matpick( clientData, interp, 2, new_argv ) != TCL_OK )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "error detected inside f_matpick", (char *)NULL);
		return TCL_ERROR;
	}
	if( not_state( ST_O_EDIT, "Object EDIT" ) )  {
		db_free_full_path( &lhs );
		db_free_full_path( &rhs );
		db_free_full_path( &both );
		Tcl_AppendResult(interp, "MGED state did not advance to Object EDIT", (char *)NULL);
		return TCL_ERROR;
	}
	db_free_full_path( &lhs );
	db_free_full_path( &rhs );
	db_free_full_path( &both );
	return TCL_OK;
}

/*
 *			F _ P U T M A T
 *
 *	Replace the matrix on an arc in the database from the command line,
 *	when NOT in an edit state.  Used mostly to facilitate writing shell
 *	scripts.  There are two valid syntaxes, each of which is implemented
 *	as an appropriate call to f_arced.  Commands of the form
 *
 *		    putmat a/b m0 m1 ... m15
 *
 *	are converted to
 *
 *		arced a/b matrix rarc m0 m1 ... m15,
 *
 *	while commands of the form
 *
 *			    putmat a/b I
 *
 *	are converted to
 *
 *	    arced a/b matrix rarc 1 0 0 0   0 1 0 0   0 0 1 0   0 0 0 1.
 *
 */
int
f_putmat (ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    int			result = TCL_OK;	/* Return code */
    char		*newargv[20+2];
    struct bu_vls	*avp;
    int			got;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 3 || 18 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help putmat");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if (not_state( ST_VIEW, "Command-line matrix replace"))
      return TCL_ERROR;

    if (!strchr( argv[1], '/'))
    {
      Tcl_AppendResult(interp, "putmat: bad path specification '",
		       argv[1], "'\n", (char *)NULL);
      return TCL_ERROR;
    }
    switch (argc)
    {
	case 18:
	    avp = bu_vls_vlsinit();
	    bu_vls_from_argv(avp, 16, (const char **)argv + 2);
	    break;
	case 3:
	    if ((argv[2][0] == 'I') && (argv[2][1] == '\0'))
	    {
		avp = bu_vls_vlsinit();
		bu_vls_printf(avp, "1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1 ");
		break;
	    }
    	    /* Sometimes the matrix is sent through Tcl as one long string.
    	     * Copy it so we can crack it, below.
    	     */
	    avp = bu_vls_vlsinit();
    	    bu_vls_strcat(avp, argv[2]);
    	    break;
	default:
	  Tcl_AppendResult(interp, "putmat: error in matrix specification (wrong number of args)\n", (char *)NULL);
	  return TCL_ERROR;
    }
    newargv[0] = "arced";
    newargv[1] = argv[1];
    newargv[2] = "matrix";
    newargv[3] = "rarc";

    /* 16+1 allows space for final NULL entry in argv[] */
    got = bu_argv_from_string( &newargv[4], 16+1, bu_vls_addr(avp) );
    if( got != 16 )  {
	  struct bu_vls tmp_vls;

	  bu_vls_init(&tmp_vls);
	  bu_vls_printf(&tmp_vls, "putmat: %s:%d: bad matrix, only got %d elements: %s\n",
			__FILE__, __LINE__, got, bu_vls_addr(&tmp_vls));
	  Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
    	  bu_vls_free(&tmp_vls);
	  result = TCL_ERROR;
    }

    if (result != TCL_ERROR)
	result = f_arced(clientData, interp, 20, newargv);

    bu_vls_vlsfree(avp);
    return result;
}

/*
 *			F _ C O P Y M A T
 *
 *	Copy the matrix on one arc in the database to another arc.
 *
 */
int
f_copymat(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
    char			*child;
    char			*parent;
    struct bu_vls		pvls;
    int				i;
    int				sep;
    int				status;
    struct db_tree_state	ts;
    struct directory		*dp;
    struct rt_comb_internal	*comb;
    struct rt_db_internal	intern;
    struct animate		*anp;
    union tree			*tp;

    CHECK_DBI_NULL;
    CHECK_READ_ONLY;

    if(argc < 3 || 3 < argc){
      struct bu_vls vls;

      bu_vls_init(&vls);
      bu_vls_printf(&vls, "help copymat");
      Tcl_Eval(interp, bu_vls_addr(&vls));
      bu_vls_free(&vls);
      return TCL_ERROR;
    }

    if (not_state( ST_VIEW, "Command-line matrix copy"))
	return TCL_ERROR;

    /*
     *	Ensure that each argument contains exactly one slash
     */
    for (i = 1; i <= 2; ++i)
	if (((child = strchr(argv[i], '/')) == NULL)
	 || (strchr(++child, '/') != NULL))
	{
	    Tcl_AppendResult(interp,
		"copymat: bad arc: '", argv[i], "'\n", (char *) NULL);
	    return TCL_ERROR;
	}

    BU_GETSTRUCT(anp, animate);
    anp -> magic = ANIMATE_MAGIC;

    ts = mged_initial_tree_state;	/* struct copy */
    ts.ts_dbip = dbip;
    ts.ts_resp = &rt_uniresource;
    MAT_IDN(ts.ts_mat);
    db_full_path_init(&anp -> an_path);
    if (db_follow_path_for_state(&ts, &(anp -> an_path), argv[1], LOOKUP_NOISY)
	< 0 )
    {
	Tcl_AppendResult(interp,
	    "copymat: cannot follow path for arc: '", argv[1], "'\n",
	    (char *) NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&pvls);
    bu_vls_strcat(&pvls, argv[2]);
    parent = bu_vls_addr(&pvls);
    sep = strchr(parent, '/') - parent;
    bu_vls_trunc(&pvls, sep);
    switch (rt_db_lookup_internal(dbip, parent, &dp, &intern, LOOKUP_NOISY, &rt_uniresource))
    {
	case ID_COMBINATION:
	    if (dp -> d_flags & DIR_COMB)
		break;
	    else
	    {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "Non-combination directory <x%lx> '%s' for combination rt_db_internal <x%lx>\nThis should not happen\n",
		    (long)dp, dp -> d_namep, &intern);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *) NULL);
	    }
	    /* Fall through this case */
	default:
	    Tcl_AppendResult(interp,
		"copymat: Object '", parent, "' is not a combination\n",
		(char *) NULL);
	    /* Fall through this case */
	case ID_NULL:
	    bu_vls_free(&pvls);
	    return TCL_ERROR;
    }
    comb = (struct rt_comb_internal *) intern.idb_ptr;
    RT_CK_COMB(comb);

    if ((tp = db_find_named_leaf(comb -> tree, child)) == TREE_NULL)
    {
	Tcl_AppendResult(interp, "copymat: unable to find instance of '",
		child, "' in combination '", dp -> d_namep,
		"'\n", (char *)NULL);
	status = TCL_ERROR;
	goto wrapup;
    }

    /*
     *	Finally, copy the matrix
     */
    if (!bn_mat_is_identity(ts.ts_mat))
    {
	if (tp -> tr_l.tl_mat == NULL)
	    tp -> tr_l.tl_mat = bn_mat_dup(ts.ts_mat);
	else
	    MAT_COPY(tp -> tr_l.tl_mat, ts.ts_mat);
    }
    else if (tp -> tr_l.tl_mat != NULL)
    {
	bu_free((genptr_t) tp -> tr_l.tl_mat, "tl_mat");
	tp -> tr_l.tl_mat = (matp_t) 0;
    }

    if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0)
    {
	TCL_WRITE_ERR;
	status = TCL_ERROR;
	goto wrapup;
    }

    status = TCL_OK;

wrapup:

    bu_vls_free(&pvls);
    if (status == TCL_ERROR)
	rt_db_free_internal(&intern, &rt_uniresource);
    return status;
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
