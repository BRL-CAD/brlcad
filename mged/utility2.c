/*
 *			U T I L I T Y 2 . C
 *
 *
 *	f_pathsum()	gives various path summaries
 *	f_copyeval()	copy an evaluated solid
 *	f_push()	control routine to push transformations to bottom of paths
 *	f_showmats()	shows matrices along a path
 *
 */

#include "conf.h"

#include <signal.h>
#include <stdio.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "nmg.h"
#include "raytrace.h"
#include "wdb.h"
#include "./ged.h"
#include "./sedit.h"
#include "../librt/debug.h"	/* XXX */

extern struct bn_tol	mged_tol;	/* from ged.c */

int
f_shells(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;

	return wdb_shells_cmd(wdbp, interp, argc, argv);
}

/*  	F _ P A T H S U M :   does the following
 *		1.  produces path for purposes of matching
 *      	2.  gives all paths matching the input path OR
 *		3.  gives a summary of all paths matching the input path
 *		    including the final parameters of the solids at the bottom
 *		    of the matching paths
 */
int
f_pathsum(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int	argc;
char	**argv;
{
	int	ret;

	CHECK_DBI_NULL;

	if (argc < 2) {
		/* get the path */
		Tcl_AppendResult(interp, MORE_ARGS_STR,
				 "Enter the path: ", (char *)NULL);
		return TCL_ERROR;
	}

	if (setjmp(jmp_env) == 0)
		(void)signal(SIGINT, sig3);  /* allow interupts */
        else
		return TCL_OK;

	ret = wdb_pathsum_cmd(wdbp, interp, argc, argv);

	(void)signal( SIGINT, SIG_IGN );
	return ret;
}

/*   	F _ C O P Y E V A L : copys an evaluated solid
 */

int
f_copyeval(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int ret;

	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	if (argc < 3) {
		Tcl_AppendResult(interp, MORE_ARGS_STR,
				 "Enter new_solid_name and full path to old_solid\n",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if (setjmp(jmp_env) == 0)
		(void)signal(SIGINT, sig3);  /* allow interupts */
        else
		return TCL_OK;

	ret = wdb_copyeval_cmd(wdbp, interp, argc, argv);

	(void)signal( SIGINT, SIG_IGN );
	return ret;
}


/*			F _ P U S H
 *
 * The push command is used to move matrices from combinations 
 * down to the solids. At some point, it is worth while thinking
 * about adding a limit to have the push go only N levels down.
 *
 * the -d flag turns on the treewalker debugging output.
 * the -P flag allows for multi-processor tree walking (not useful)
 * the -l flag is there to select levels even if it does not currently work.
 */
int
f_push(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	return wdb_push_cmd(wdbp, interp, argc, argv);
}

int
f_hide(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	return wdb_hide_cmd(wdbp, interp, argc, argv);
}

int
f_unhide(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	return wdb_unhide_cmd(wdbp, interp, argc, argv);
}

int
f_xpush(ClientData	clientData,
	Tcl_Interp	*interp,
	int		argc,
	char		**argv)
{
	CHECK_DBI_NULL;

	return wdb_xpush_cmd(wdbp, interp, argc, argv);
}

int
f_showmats(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;

	return wdb_showmats_cmd(wdbp, interp, argc, argv);
}

int
f_nmg_simplify(clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char *argv[];
{
	CHECK_DBI_NULL;

	return wdb_nmg_simplify_cmd(wdbp, interp, argc, argv);
}

/*			F _ M A K E _ B B
 *
 *	Build an RPP bounding box for the list of objects and/or paths passed to this routine
 */

int
f_make_bb(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;
	CHECK_READ_ONLY;

	return wdb_make_bb_cmd(wdbp, interp, argc, argv);
}

int
f_whatid(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	CHECK_DBI_NULL;

	return wdb_whatid_cmd(wdbp, interp, argc, argv);
}

int
f_eac(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	int i,j;
	int item;
	struct directory *dp;
	struct bu_vls v;
	int new_argc;
	int lim;

	CHECK_DBI_NULL;

	if(argc < 2){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help eac");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	bu_vls_init( &v );

	if( setjmp( jmp_env ) == 0 )
	  (void)signal( SIGINT, sig3);  /* allow interupts */
        else{
	  bu_vls_free( &v );
	  return TCL_OK;
	}

	bu_vls_strcat( &v, "e" );
	lim = 1;

	for( j=1; j<argc; j++)
	{
		item = atoi( argv[j] );
		if( item < 1 )
			continue;

		for( i = 0; i < RT_DBNHASH; i++ )
		{
			for( dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw )
			{
				struct rt_db_internal intern;
				struct rt_comb_internal *comb;

				if( !(dp->d_flags & DIR_REGION) )
					continue;

				if( rt_db_get_internal( &intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource ) < 0 )
					TCL_READ_ERR_return;
				comb = (struct rt_comb_internal *)intern.idb_ptr;
				if( comb->region_id != 0 ||
					comb->aircode != item )
				{
					rt_comb_ifree( &intern, &rt_uniresource );
					continue;
				}
				rt_comb_ifree( &intern, &rt_uniresource );

				bu_vls_strcat( &v, " " );
				bu_vls_strcat( &v, dp->d_namep );
				lim++;
			}
		}
	}

	if( lim > 1 )
	{
		int retval;
		char **new_argv;

		new_argv = (char **)bu_calloc( lim+1, sizeof( char *), "f_eac: new_argv" );
		new_argc = rt_split_cmd( new_argv, lim+1, bu_vls_addr( &v ) );
		retval = f_edit( clientData, interp, new_argc, new_argv );
		bu_free( (genptr_t)new_argv, "f_eac: new_argv" );
		bu_vls_free( &v );
		(void)signal( SIGINT, SIG_IGN );
		return retval;
	}
	else
	{
		bu_vls_free( &v );
		(void)signal( SIGINT, SIG_IGN );
		return TCL_OK;
	}
}


int
f_edge_collapse( clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	char *av[3];

	CHECK_DBI_NULL;

	if (wdb_nmg_collapse_cmd(wdbp, interp, argc, argv) == TCL_ERROR)
		return TCL_ERROR;

	av[0] = "e";
	av[1] = argv[2];
	av[2] = NULL;

	return f_edit(clientData, interp, 2, av);
}

/*			F _ M A K E _ N A M E
 *
 * Generate an identifier that is guaranteed not to be the name
 * of any object currently in the database.
 *
 */
int
f_make_name(ClientData	clientData,
	    Tcl_Interp	*interp,
	    int		argc,
	    char	**argv)
{
	CHECK_DBI_NULL;
	return wdb_make_name_cmd(wdbp, interp, argc, argv);
}
