/*
 *				W D B _ O B J . C
 *
 *  A database object contains the attributes and
 *  methods for controlling a BRLCAD database.
 * 
 *  Authors -
 *	Michael John Muuss
 *      Glenn Durfee
 *	Robert G. Parker
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" license agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"
#include <ctype.h>

#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <math.h>
#if unix
# include <fcntl.h>
# include <sys/errno.h>
#endif
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"		/* this includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "db.h"
#include "mater.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

#include "./debug.h"

extern void rt_insert_color( struct mater *newp );

/* A verbose message to attempt to soothe and advise the user */
#define	WDB_TCL_ERROR_RECOVERY_SUGGESTION\
        Tcl_AppendResult(interp, "\
The in-memory table of contents may not match the status of the on-disk\n\
database.  The on-disk database should still be intact.  For safety,\n\
you should exit MGED now, and resolve the I/O problem, before continuing.\n", (char *)NULL)

#define WDB_MAX_LEVELS 12
#define WDB_CPEVAL	0
#define WDB_LISTPATH	1
#define WDB_LISTEVAL	2

/* from librt/tcl.c */
extern int rt_tcl_rt();
extern int rt_tcl_import_from_path();
extern void rt_generic_make();

/* from librt/dg_obj.c */
extern void dgo_eraseobjall_callback();	/* called when killing database object's */
extern void dgo_impending_wdb_close();	/* called in wdb_deleteProc */
extern void dgo_zapall();		/* called in wdb_reopen_tcl */

/* from librt/wdb_comb_std.c */
extern int wdb_comb_std_tcl();

/* from db5_scan.c */
HIDDEN int db5_scan();


static int wdb_open_tcl();
static int wdb_close_tcl();
static int wdb_decode_dbip();
static struct db_i *wdb_prep_dbip();

static int wdb_cmd();
static int wdb_match_tcl();
int wdb_get_tcl();
static int wdb_put_tcl();
static int wdb_adjust_tcl();
static int wdb_form_tcl();
static int wdb_tops_tcl();
static int wdb_rt_gettrees_tcl();
static int wdb_dump_tcl();
static int wdb_dbip_tcl();
static int wdb_ls_tcl();
static int wdb_list_tcl();
static int wdb_pathsum_tcl();
static int wdb_expand_tcl();
static int wdb_kill_tcl();
static int wdb_killall_tcl();
static int wdb_killtree_tcl();
static void wdb_killtree_callback();
static int wdb_copy_tcl();
static int wdb_move_tcl();
static int wdb_move_all_tcl();
static int wdb_concat_tcl();
static int wdb_dup_tcl();
static int wdb_group_tcl();
static int wdb_remove_tcl();
static int wdb_region_tcl();
static int wdb_comb_tcl();
static int wdb_find_tcl();
static int wdb_which_tcl();
static int wdb_title_tcl();
static int wdb_tree_tcl();
static int wdb_color_tcl();
static int wdb_prcolor_tcl();
static int wdb_tol_tcl();
static int wdb_push_tcl();
static int wdb_whatid_tcl();
static int wdb_keep_tcl();
static int wdb_cat_tcl();
static int wdb_instance_tcl();
static int wdb_observer_tcl();
static int wdb_reopen_tcl();
static int wdb_make_bb_tcl();
static int wdb_make_name_tcl();
static int wdb_units_tcl();

static void wdb_deleteProc();
static void wdb_deleteProc_rt();

static int wdb_trace();

int wdb_cmpdirname();
void wdb_vls_col_item();
void wdb_vls_col_eol();
void wdb_vls_col_pr4v();
void wdb_vls_line_dpp();
void wdb_do_list();
struct directory ** wdb_getspace();
struct directory *wdb_combadd();
void wdb_identitize();

static struct bu_cmdtab wdb_cmds[] = {
	{"adjust",	wdb_adjust_tcl},
	{"c",		wdb_comb_std_tcl},
	{"cat",		wdb_cat_tcl},
	{"close",	wdb_close_tcl},
	{"color",	wdb_color_tcl},
	{"comb",	wdb_comb_tcl},
	{"concat",	wdb_concat_tcl},
	{"cp",		wdb_copy_tcl},
	{"dbip",	wdb_dbip_tcl},
	{"dump",	wdb_dump_tcl},
	{"dup",		wdb_dup_tcl},
	{"expand",	wdb_expand_tcl},
	{"find",	wdb_find_tcl},
	{"form",	wdb_form_tcl},
	{"g",		wdb_group_tcl},
	{"get",		wdb_get_tcl},
	{"i",		wdb_instance_tcl},
	{"keep",	wdb_keep_tcl},
	{"kill",	wdb_kill_tcl},
	{"killall",	wdb_killall_tcl},
	{"killtree",	wdb_killtree_tcl},
	{"l",		wdb_list_tcl},
	{"listeval",	wdb_pathsum_tcl},
	{"ls",		wdb_ls_tcl},
	{"make_bb",	wdb_make_bb_tcl},
	{"make_name",	wdb_make_name_tcl},
	{"match",	wdb_match_tcl},
	{"mv",		wdb_move_tcl},
	{"mvall",	wdb_move_all_tcl},
	{"observer",	wdb_observer_tcl},
	{"open",	wdb_reopen_tcl},
	{"paths",	wdb_pathsum_tcl},
	{"prcolor",	wdb_prcolor_tcl},
	{"push",	wdb_push_tcl},
	{"put",		wdb_put_tcl},
	{"r",		wdb_region_tcl},
	{"rm",		wdb_remove_tcl},
	{"rt_gettrees",	wdb_rt_gettrees_tcl},
	{"title",	wdb_title_tcl},
	{"tol",		wdb_tol_tcl},
	{"tops",	wdb_tops_tcl},
	{"tree",	wdb_tree_tcl},
	{"units",	wdb_units_tcl},
	{"whatid",	wdb_whatid_tcl},
	{"whichair",	wdb_which_tcl},
	{"whichid",	wdb_which_tcl},
#if 0
	/* Commands to be added */
	{"comb_color",	wdb_comb_color_tcl},
	{"copymat",	wdb_copymat_tcl},
	{"copyeval",	wdb_copyeval_tcl},
	{"pathlist",	wdb_pathlist_tcl},
	{"getmat",	wdb_getmat_tcl},
	{"putmat",	wdb_putmat_tcl},
	{"summary",	wdb_summary_tcl},
	{"which_shader",	wdb_which_shader_tcl},
	{"rcodes",	wdb_rcodes_tcl},
	{"wcodes",	wdb_wcodes_tcl},
	{"rmater",	wdb_rmater_tcl},
	{"wmater",	wdb_wmater_tcl},
	{"analyze",	wdb_analyze_tcl},
	{"inside",	wdb_inside_tcl},
#endif
	{(char *)NULL,	(int (*)())0 }
};

/* This could go in struct rt_wdb */
static Tcl_Interp *wdb_obj_interp;

int
Wdb_Init(interp)
     Tcl_Interp *interp;
{
	(void)Tcl_CreateCommand(interp, "wdb_open", wdb_open_tcl,
				(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	return TCL_OK;
}

/*
 *			W D B _ C M D
 *
 * Generic interface for database commands.
 * Usage:
 *        procname cmd ?args?
 *
 * Returns: result of wdb command.
 */
static int
wdb_cmd(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	return bu_cmd(clientData, interp, argc, argv, wdb_cmds, 1);
}

/*
 * Called by Tcl when the object is destroyed.
 */
static void
wdb_deleteProc(clientData)
     ClientData clientData;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	/* free observers */
	bu_observer_free(&wdbp->wdb_observers);

	/* notify drawable geometry objects of the impending close */
	dgo_impending_wdb_close(wdbp, wdb_obj_interp);

	RT_CK_WDB(wdbp);
	BU_LIST_DEQUEUE(&wdbp->l);
	bu_vls_free(&wdbp->wdb_name);
	wdb_close(wdbp);
}

/*
 * Close a BRLCAD database object.
 *
 * USAGE:
 *	  procname close
 */
static int
wdb_close_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/*
	 * Maybe this should go in "struct rt_wdb". For now,
	 * use a global.
	 */
	wdb_obj_interp = interp;

	/*
	 * Among other things, this will call wdb_deleteProc.
	 * Note - wdb_deleteProc is being passed clientdata.
	 *        It ought to get interp as well.
	 */
	Tcl_DeleteCommand(interp, bu_vls_addr(&wdbp->wdb_name));

	return TCL_OK;
}

/*
 *			W D B _ O P E N _ T C L
 *
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be rt_wdb pointer
 *	for this instance of the database.
 *	Easily allows keeping track of multiple databases.
 *
 *  Explicit return -
 *	wdb pointer, for more traditional C-style interfacing.
 *
 *  Example -
 *	set wdbp [wdb_open .inmem inmem $dbip]
 *	.inmem get box.s
 *	.inmem close
 *
 *	wdb_open db file "bob.g"
 *	db get white.r
 *	db close
 */
static int
wdb_open_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int		argc;
     char		**argv;
{
	struct rt_wdb *wdbp;

	if (argc == 1) {
		/* get list of database objects */
		for (BU_LIST_FOR(wdbp, rt_wdb, &rt_g.rtg_headwdb.l))
			Tcl_AppendResult(interp, bu_vls_addr(&wdbp->wdb_name), " ", (char *)NULL);

		return TCL_OK;
	}

	if (argc != 4) {
#if 0
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_open");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
#else
		Tcl_AppendResult(interp, "\
Usage: wdb_open\n\
       wdb_open newprocname file filename\n\
       wdb_open newprocname disk $dbip\n\
       wdb_open newprocname disk_append $dbip\n\
       wdb_open newprocname inmem $dbip\n\
       wdb_open newprocname inmem_append $dbip\n\
       wdb_open newprocname db filename\n",
				 NULL);
		return TCL_ERROR;
#endif
	}

	/* Delete previous proc (if any) to release all that memory, first */
	(void)Tcl_DeleteCommand(interp, argv[1]);

	if (strcmp(argv[2], "file") == 0) {
		wdbp = wdb_fopen( argv[3] );
	} else {
		struct db_i	*dbip;

		if (strcmp(argv[2], "db") == 0) {
			if ((dbip = wdb_prep_dbip(interp, argv[3])) == DBI_NULL)
				return TCL_ERROR;
			RT_CK_DBI_TCL(interp,dbip);

			/* --- Scan geometry database and build in-memory directory --- */
			db_dirbuild(dbip);

			wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
		} else {
			if (wdb_decode_dbip(interp, argv[3], &dbip) != TCL_OK)
				return TCL_ERROR;

			if (strcmp( argv[2], "disk" ) == 0)
				wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
			else if (strcmp(argv[2], "disk_append") == 0)
				wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY);
			else if (strcmp( argv[2], "inmem" ) == 0)
				wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM);
			else if (strcmp( argv[2], "inmem_append" ) == 0)
				wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY);
			else {
				Tcl_AppendResult(interp, "wdb_open ", argv[2],
						 " target type not recognized", NULL);
				return TCL_ERROR;
			}
		}
	}
	if (wdbp == RT_WDB_NULL) {
		Tcl_AppendResult(interp, "wdb_open ", argv[1], " failed", NULL);
		return TCL_ERROR;
	}

	/* initialize rt_wdb */
	bu_vls_init(&wdbp->wdb_name);
	bu_vls_strcpy(&wdbp->wdb_name, argv[1]);

#if 0
	/*XXXX already initialize by wdb_dbopen */
	/* initilize tolerance structures */
	wdbp->wdb_ttol.magic = RT_TESS_TOL_MAGIC;
	wdbp->wdb_ttol.abs = 0.0;		/* disabled */
	wdbp->wdb_ttol.rel = 0.01;
	wdbp->wdb_ttol.norm = 0.0;		/* disabled */

	wdbp->wdb_tol.magic = BN_TOL_MAGIC;
	wdbp->wdb_tol.dist = 0.005;
	wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
	wdbp->wdb_tol.perp = 1e-6;
	wdbp->wdb_tol.para = 1 - wdbp->wdb_tol.perp;
#endif

	/* initialize tree state */
	wdbp->wdb_initial_tree_state = rt_initial_tree_state;  /* struct copy */
	wdbp->wdb_initial_tree_state.ts_ttol = &wdbp->wdb_ttol;
	wdbp->wdb_initial_tree_state.ts_tol = &wdbp->wdb_tol;

	/* default region ident codes */
	wdbp->wdb_item_default = 1000;
	wdbp->wdb_air_default = 0;
	wdbp->wdb_mat_default = 1;
	wdbp->wdb_los_default = 100;

	/* resource structure */
	wdbp->wdb_resp = &rt_uniresource;

	BU_LIST_INIT(&wdbp->wdb_observers.l);

	/* append to list of rt_wdb's */
	BU_LIST_APPEND(&rt_g.rtg_headwdb.l,&wdbp->l);

	/* Instantiate the newprocname, with clientData of wdbp */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand(interp, argv[1], wdb_cmd,
				(ClientData)wdbp, wdb_deleteProc);

	/* Return new function name as result */
	Tcl_AppendResult(interp, argv[1], (char *)NULL);
	
	return TCL_OK;
}

int
wdb_decode_dbip(interp, dbip_string, dbipp)
     Tcl_Interp *interp;
     char *dbip_string;
     struct db_i **dbipp;
{

	*dbipp = (struct db_i *)atol(dbip_string);

	/* Could core dump */
	RT_CK_DBI_TCL(interp,*dbipp);

	return TCL_OK;
}

struct db_i *
wdb_prep_dbip(interp, filename)
     Tcl_Interp *interp;
     char *filename;
{
	struct db_i *dbip;

	/* open database */
	if (((dbip = db_open(filename, "r+w")) == DBI_NULL) &&
	    ((dbip = db_open(filename, "r"  )) == DBI_NULL)) {
		/*
		 * Check to see if we can access the database
		 */
#if unix
		if (access(filename, R_OK|W_OK) != 0 && errno != ENOENT) {
			perror(filename);
			return DBI_NULL;
		}
#endif
#if WIN32
#endif

		if ((dbip = db_create(filename, 5)) == DBI_NULL) {
			Tcl_AppendResult(interp,
					 "wdb_open: failed to create ", filename,
					 (char *)NULL);
			if (dbip == DBI_NULL)
				Tcl_AppendResult(interp,
						 "opendb: no database is currently opened!", \
						 (char *)NULL);

			return DBI_NULL;
		}
	}

	return dbip;
}

/****************** Database Object Methods ********************/
/*
 *
 * Usage:
 *        procname open [filename]
 */
static int
wdb_reopen_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct db_i *dbip;
	struct bu_vls vls;

	/* get database filename */
	if (argc == 2) {
		Tcl_AppendResult(interp, wdbp->dbip->dbi_filename, (char *)NULL);
		return TCL_OK;
	}

	/* set database filename */
	if (argc == 3) {
		if ((dbip = wdb_prep_dbip(interp, argv[2])) == DBI_NULL) {
			return TCL_ERROR;
		}

		/* XXXnotify observers */
		/* notify drawable geometry objects associated with this database */
		dgo_zapall(wdbp, interp);

		/* close current database */
		db_close(wdbp->dbip);

		wdbp->dbip = dbip;

		/* --- Scan geometry database and build in-memory directory --- */
		db_dirbuild(wdbp->dbip);

		Tcl_AppendResult(interp, wdbp->dbip->dbi_filename, (char *)NULL);
		return TCL_OK;
	}

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "helplib wdb_reopen");
	Tcl_Eval(interp, bu_vls_addr(&vls));
	bu_vls_free(&vls);
	return TCL_ERROR;
}

/*
 *			W D B _ M A T C H _ T C L
 *
 * Returns (in interp->result) a list (possibly empty) of all matches to
 * the (possibly wildcard-containing) arguments given.
 * Does *NOT* return tokens that do not match anything, unlike the
 * "expand" command.
 */

static int
wdb_match_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct bu_vls	matches;

	/* skip past procname */
	--argc;
	++argv;

	RT_CK_WDB_TCL(interp,wdbp);
	
	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if (wdbp->dbip == 0) {
		Tcl_AppendResult( interp, "this database does not support lookup operations" );
		return TCL_ERROR;
	}

	bu_vls_init(&matches);
	for (++argv; *argv != NULL; ++argv) {
		if (db_regexp_match_all(&matches, wdbp->dbip, *argv) > 0)
			bu_vls_strcat(&matches, " ");
	}
	bu_vls_trimspace(&matches);
	Tcl_AppendResult(interp, bu_vls_addr(&matches), (char *)NULL);
	bu_vls_free(&matches);
	return TCL_OK;
}

/*
 *			W D B _ G E T_ T C L
 *
 **
 ** For use with Tcl, this routine accepts as its first argument the name
 ** of an object in the database.  If only one argument is given, this routine
 ** then fills the result string with the (minimal) attributes of the item.
 ** If a second, optional, argument is provided, this function looks up the
 ** property with that name of the item given, and returns it as the result
 ** string.
 **/
/* NOTE: This is called directly by gdiff/g_diff.c */

int
wdb_get_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	int			status;
	struct rt_db_internal	intern;
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	/* skip past procname */
	--argc;
	++argv;

	if (argc < 2 || argc > 3) {
		Tcl_AppendResult(interp,
				 "wrong # args: should be \"", argv[0],
				 " objName ?attr?\"", (char *)NULL);
		return TCL_ERROR;
	}

	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if (wdbp->dbip == 0) {
		Tcl_AppendResult(interp,
				 "db does not support lookup operations",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if (rt_tcl_import_from_path(interp, &intern, argv[1], wdbp) == TCL_ERROR)
		return TCL_ERROR;

	status = intern.idb_meth->ft_tclget(interp, &intern, argv[2]);
	rt_db_free_internal(&intern, &rt_uniresource);
	return status;
}

/*
 *			W D B _ P U T _ T C L
 **
 ** Creates an object and stuffs it into the databse.
 ** All arguments must be specified.  Object cannot already exist.
 **/

static int
wdb_put_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_db_internal			intern;
	register CONST struct rt_functab	*ftp;
	int					i;
	char				       *name;
	char				        type[16];
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	/* skip past procname */
	--argc;
	++argv;

	if (argc < 3) {
		Tcl_AppendResult(interp,
				 "wrong # args: should be db put objName objType attrs",
				 (char *)NULL);
		return TCL_ERROR;
	}

	name = argv[1];
    
	/* Verify that this wdb supports lookup operations (non-null dbip).
	 * stdout/file wdb objects don't, but can still be written to.
	 * If not, just skip the lookup test and write the object
	 */
	if (wdbp->dbip && db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL ) {
		Tcl_AppendResult(interp, argv[1], " already exists",
				 (char *)NULL);
		return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL(&intern);

	for (i = 0; argv[2][i] != 0 && i < 16; i++) {
		type[i] = isupper(argv[2][i]) ? tolower(argv[2][i]) :
			argv[2][i];
	}
	type[i] = 0;

	ftp = rt_get_functab_by_label(type);
	if (ftp == NULL) {
		Tcl_AppendResult(interp, type,
				 " is an unknown object type.",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if (ftp->ft_make) {
		ftp->ft_make(ftp, &intern, 0.0);
	} else {
		rt_generic_make(ftp, &intern, 0.0);
	}

	if (ftp->ft_tcladjust(interp, &intern, argc-3, argv+3, &rt_uniresource) == TCL_ERROR) {
		rt_db_free_internal(&intern, &rt_uniresource);
		return TCL_ERROR;
	}

	if (wdb_put_internal(wdbp, name, &intern, 1.0) < 0)  {
		Tcl_AppendResult(interp, "wdb_put_internal(", argv[1],
				 ") failure", (char *)NULL);
		rt_db_free_internal(&intern, &rt_uniresource);
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern, &rt_uniresource );
	return TCL_OK;
}

/*
 *			W D B _ A D J U S T _ T C L
 *
 **
 ** For use with Tcl, this routine accepts as its first argument an item in
 ** the database; as its remaining arguments it takes the properties that
 ** need to be changed and their values.
 *
 *  Example of adjust operation on a solid:
 *	.inmem adjust LIGHT V { -46 -13 5 }
 *
 *  Example of adjust operation on a combination:
 *	.inmem adjust light.r rgb { 255 255 255 }
 */

static int
wdb_adjust_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	register struct directory	*dp;
	int				 status;
	char				*name;
	struct rt_db_internal		 intern;
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	if (argc < 5) {
		Tcl_AppendResult(interp,
				 "wrong # args: should be \"db adjust objName attr value ?attr? ?value?...\"",
				 (char *)NULL);
		return TCL_ERROR;
	}
	name = argv[2];

	/* Verify that this wdb supports lookup operations (non-null dbip) */
	RT_CK_DBI_TCL(interp,wdbp->dbip);

	dp = db_lookup(wdbp->dbip, name, LOOKUP_QUIET);
	if (dp == DIR_NULL) {
		Tcl_AppendResult(interp, name, ": not found",
				 (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal(&intern, dp, wdbp->dbip, (matp_t)NULL, &rt_uniresource);
	if (status < 0) {
		Tcl_AppendResult(interp, "rt_db_get_internal(", name,
				 ") failure", (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL(&intern);

	/* Find out what type of object we are dealing with and tweak it. */
	RT_CK_FUNCTAB(intern.idb_meth);

	status = intern.idb_meth->ft_tcladjust(interp, &intern, argc-3, argv+3, &rt_uniresource);
	if( status == TCL_OK && wdb_put_internal(wdbp, name, &intern, 1.0) < 0)  {
		Tcl_AppendResult(interp, "wdb_export(", name,
				 ") failure", (char *)NULL);
		rt_db_free_internal(&intern, &rt_uniresource);
		return TCL_ERROR;
	}

	/* notify observers */
	bu_observer_notify(interp, &wdbp->wdb_observers, bu_vls_addr(&wdbp->wdb_name));

	return status;
}

/*
 *			W D B _ F O R M _ T C L
 */
static int
wdb_form_tcl( clientData, interp, argc, argv )
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
	CONST struct rt_functab		*ftp;

	--argc;
	++argv;

	if (argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"db form objType\"",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if ((ftp = rt_get_functab_by_label(argv[1])) == NULL) {
		Tcl_AppendResult(interp, "There is no geometric object type \"",
				 argv[1], "\".", (char *)NULL);
		return TCL_ERROR;
	}
	return ftp->ft_tclform(ftp, interp);
}

/*
 *			W D B _ T O P S _ T C L
 *
 *  NON-PARALLEL because of rt_uniresource
 */
static int
wdb_tops_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;

	RT_CK_WDB_TCL(interp, wdbp);
	RT_CK_DBI_TCL(interp, wdbp->dbip);

	/* Can this be executed only sometimes?
	   Perhaps a "dirty bit" on the database? */
	db_update_nref(wdbp->dbip, &rt_uniresource);
	
	for (i = 0; i < RT_DBNHASH; i++)
		for (dp = wdbp->dbip->dbi_Head[i];
		     dp != DIR_NULL;
		     dp = dp->d_forw)  {
			if (dp->d_nref == 0)
				Tcl_AppendElement( interp, dp->d_namep);
		}
	return TCL_OK;
}

/*
 *			R T _ T C L _ D E L E T E P R O C _ R T
 *
 *  Called when the named proc created by rt_gettrees() is destroyed.
 */
static void
wdb_deleteProc_rt(clientData)
     ClientData clientData;
{
	struct application	*ap = (struct application *)clientData;
	struct rt_i		*rtip;

	RT_AP_CHECK(ap);
	rtip = ap->a_rt_i;
	RT_CK_RTI(rtip);

	rt_free_rti(rtip);
	ap->a_rt_i = (struct rt_i *)NULL;

	bu_free( (genptr_t)ap, "struct application" );
}

/*
 *			W D B _ R T _ G E T T R E E S _ T C L
 *
 *  Given an instance of a database and the name of some treetops,
 *  create a named "ray-tracing" object (proc) which will respond to
 *  subsequent operations.
 *  Returns new proc name as result.
 *
 *  Example:
 *	.inmem rt_gettrees .rt all.g light.r
 */
static int
wdb_rt_gettrees_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct rt_i	*rtip;
	struct application	*ap;
	struct resource		*resp;
	char		*newprocname;

	RT_CK_WDB_TCL(interp, wdbp);
	RT_CK_DBI_TCL(interp, wdbp->dbip);

	if (argc < 4) {
		Tcl_AppendResult(interp,
				 "rt_gettrees: wrong # args: should be \"",
				 argv[0], " ", argv[1],
				 " newprocname [-i] [-u] treetops...\"", (char *)NULL );
		return TCL_ERROR;
	}

	rtip = rt_new_rti(wdbp->dbip);
	newprocname = argv[2];

	/* Delete previous proc (if any) to release all that memory, first */
	(void)Tcl_DeleteCommand(interp, newprocname);

	while (argv[3][0] == '-') {
		if (strcmp( argv[3], "-i") == 0) {
			rtip->rti_dont_instance = 1;
			argc--;
			argv++;
			continue;
		}
		if (strcmp(argv[3], "-u") == 0) {
			rtip->useair = 1;
			argc--;
			argv++;
			continue;
		}
		break;
	}

	if (rt_gettrees(rtip, argc-3, (CONST char **)&argv[3], 1) < 0) {
		Tcl_AppendResult(interp,
				 "rt_gettrees() returned error", (char *)NULL);
		rt_free_rti(rtip);
		return TCL_ERROR;
	}

	/* Establish defaults for this rt_i */
	rtip->rti_hasty_prep = 1;	/* Tcl isn't going to fire many rays */

	/*
	 *  In case of multiple instances of the library, make sure that
	 *  each instance has a separate resource structure,
	 *  because the bit vector lengths depend on # of solids.
	 *  And the "overwrite" sequence in Tcl is to create the new
	 *  proc before running the Tcl_CmdDeleteProc on the old one,
	 *  which in this case would trash rt_uniresource.
	 *  Once on the rti_resources list, rt_clean() will clean 'em up.
	 */
	BU_GETSTRUCT(resp, resource);
	rt_init_resource(resp, 0, rtip);
	BU_ASSERT_PTR( BU_PTBL_GET(&rtip->rti_resources, 0), !=, NULL );

	BU_GETSTRUCT(ap, application);
	ap->a_magic = RT_AP_MAGIC;
	ap->a_resource = resp;
	ap->a_rt_i = rtip;
	ap->a_purpose = "Conquest!";

	rt_ck(rtip);

	/* Instantiate the proc, with clientData of wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand(interp, newprocname, rt_tcl_rt,
				(ClientData)ap, wdb_deleteProc_rt);

	/* Return new function name as result */
	Tcl_AppendResult(interp, newprocname, (char *)NULL);

	return TCL_OK;
}

/*
 *			W D B _ D U M P _ T C L
 *
 *  Write the current state of a database object out to a file.
 *
 *  Example:
 *	.inmem dump "/tmp/foo.g"
 */
static int
wdb_dump_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct rt_wdb	*op;
	int		ret;

	RT_CK_WDB_TCL(interp, wdbp);
	RT_CK_DBI_TCL(interp, wdbp->dbip);

	if (argc != 3) {
		Tcl_AppendResult(interp,
				 "dump: wrong # args: should be \"",
				 argv[0], "dump filename.g", (char *)NULL);
		return TCL_ERROR;
	}

	if ((op = wdb_fopen(argv[2])) == RT_WDB_NULL) {
		Tcl_AppendResult(interp,
				 argv[0],
				 " dump:  ",
				 argv[2],
				 ": cannot create",
				 (char *)NULL);
		return TCL_ERROR;
	}
	ret = db_dump(op, wdbp->dbip);
	wdb_close(op);
	if (ret < 0) {
		Tcl_AppendResult(interp,
				 argv[0], " dump ", argv[2], ": db_dump() error",
				 (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 *
 * Usage:
 *        procname dbip
 *
 * Returns: database objects dbip.
 */
static int
wdb_dbip_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc != 2) {
		bu_vls_printf(&vls, "helplib wdb_dbip");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%lu", (unsigned long)wdbp->dbip);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	return TCL_OK;
}

/*
 *
 * Usage:
 *        procname ls [args]
 *
 * Returns: list objects in this database object.
 */
static int
wdb_ls_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct bu_vls vls;
	register struct directory *dp;
	register int i;
	int c;
	int aflag = 0;		/* print all objects without formatting */
	int cflag = 0;		/* print combinations */
	int rflag = 0;		/* print regions */
	int sflag = 0;		/* print solids */
	struct directory **dirp;
	struct directory **dirp0 = (struct directory **)NULL;

	bu_vls_init(&vls);

	/* skip past procname */
	--argc;
	++argv;

	if (argc < 1 || MAXARGS < argc) {
		bu_vls_printf(&vls, "helplib wdb_ls");
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
			bu_vls_printf(&vls, "Unrecognized option - %c", c);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
			return TCL_ERROR;
		}
	}
	argc -= (bu_optind - 1);
	argv += (bu_optind - 1);

	/* create list of objects in database */
	if (argc > 1) {
		/* Just list specified names */
		dirp = wdb_getspace(wdbp->dbip, argc-1);
		dirp0 = dirp;
		/*
		 * Verify the names, and add pointers to them to the array.
		 */
		for (i = 1; i < argc; i++) {
			if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) ==
			    DIR_NULL)
				continue;
			*dirp++ = dp;
		}
	} else {
		/* Full table of contents */
		dirp = wdb_getspace(wdbp->dbip, 0);	/* Enough for all */
		dirp0 = dirp;
		/*
		 * Walk the directory list adding pointers (to the directory
		 * entries) to the array.
		 */
		for (i = 0; i < RT_DBNHASH; i++)
			for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
				*dirp++ = dp;
	}

	if (aflag || cflag || rflag || sflag)
		wdb_vls_line_dpp(&vls, dirp0, (int)(dirp - dirp0),
				 aflag, cflag, rflag, sflag);
	else
		wdb_vls_col_pr4v(&vls, dirp0, (int)(dirp - dirp0));

	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	bu_free( (genptr_t)dirp0, "wdb_getspace dp[]" );

	return TCL_OK;
}

/*
 *
 *  Usage:
 *        procname l [-r] arg(s)
 *
 *  List object information, verbose.
 */
static int
wdb_list_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int arg;
	struct bu_vls str;
	int id;
	int recurse = 0;
	char *listeval = "listeval";
	struct rt_db_internal intern;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_list");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc > 2 && strcmp(argv[2], "-r") == 0) {
		recurse = 1;

		/* skip past used args */
		argc -= 1;
		argv += 1;
	}

	/* skip past used args */
	argc -= 2;
	argv += 2;

	bu_vls_init( &str );

	for (arg = 0; arg < argc; arg++) {
		if (recurse) {
			char *tmp_argv[4];

			tmp_argv[0] = "bogus";
			tmp_argv[1] = listeval;
			tmp_argv[2] = argv[arg];
			tmp_argv[3] = (char *)NULL;

			wdb_pathsum_tcl(clientData, interp, 3, tmp_argv);
		} else if (strchr(argv[arg], '/')) {
			struct db_tree_state ts;
			struct db_full_path path;

			bzero( (char *)&ts, sizeof( ts ) );
			db_full_path_init( &path );

			ts.ts_dbip = wdbp->dbip;
			bn_mat_idn(ts.ts_mat);

			if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
				continue;

			dp = DB_FULL_PATH_CUR_DIR( &path );

			if ((id = rt_db_get_internal(&intern, dp, wdbp->dbip, ts.ts_mat, &rt_uniresource)) < 0) {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
						 ") failure", (char *)NULL );
				continue;
			}

			db_free_full_path( &path );

			bu_vls_printf( &str, "%s:  ", argv[arg] );

			if (rt_functab[id].ft_describe(&str, &intern, 99, wdbp->dbip->dbi_base2local, &rt_uniresource) < 0)
				Tcl_AppendResult(interp, dp->d_namep, ": describe error", (char *)NULL);

			rt_db_free_internal(&intern, &rt_uniresource);
		} else {
			if ((dp = db_lookup(wdbp->dbip, argv[arg], LOOKUP_NOISY)) == DIR_NULL)
				continue;

			wdb_do_list(wdbp->dbip, interp, &str, dp, 99);	/* very verbose */
		}
	}

	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 *			W D B _ T R A C E
 *
 *  Follow a given path.  What happens next depends on setting of 'flag'.
 *
 *  Return -
 *	0	path not found
 *	1	OK
 */
static int
wdb_trace(interp, dbip, old_xlate, flag, wdb_xform, des_path)
     Tcl_Interp			*interp;
     struct db_i		*dbip;
     mat_t old_xlate;
     int flag;
     mat_t wdb_xform;
     CONST struct db_full_path *des_path;
{
	struct bu_vls str;
	struct db_tree_state ts;
	struct db_full_path accumulated_path;
	struct directory *dp;
	int ret = 0;

	RT_CK_DBI(dbip);
	RT_CK_FULL_PATH(des_path);

	db_full_path_init(&accumulated_path);
	db_init_db_tree_state( &ts, dbip, &rt_uniresource );
	bn_mat_copy( ts.ts_mat, old_xlate );
	if( db_follow_path( &ts, &accumulated_path, des_path, LOOKUP_NOISY, 0 ) < 0 )  {
		ret = 0;
		goto out;
	}
	bn_mat_copy( wdb_xform, ts.ts_mat );

	if (flag == WDB_CPEVAL)  {
		/* all they wanted was the matrix */
		ret = 1;
		goto out;
	}

	/* print the path */
	db_full_path_appendresult( interp, &accumulated_path );

	if (flag == WDB_LISTPATH) {
		ret = 1;
		goto out;
	}

	BU_ASSERT(flag == WDB_LISTEVAL);
	bu_vls_init( &str );
	dp = DB_FULL_PATH_CUR_DIR(&accumulated_path);
	RT_CK_DIR(dp);
	wdb_do_list(dbip, interp, &str, dp, 1);
	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);
	ret = 1;
out:
	db_free_full_path(&accumulated_path);
	db_free_db_tree_state( &ts );
	return ret;
}

/*
 *			W D B _ P A T H S U M _ T C L
 *
 *  Common code for several direct db methods: listeval, paths
 *  Also used as support routine for "l" (list) command.
 *
 *  1.  produces path for purposes of matching
 *  2.  gives all paths matching the input path OR
 *  3.  gives a summary of all paths matching the input path
 *	including the final parameters of the solids at the bottom
 *	of the matching paths
 *
 * Usage:
 *        procname (WDB_LISTEVAL|paths) args(s)
 */
static int
wdb_pathsum_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	int flag = WDB_CPEVAL;
	mat_t	wdb_xform;
	struct db_full_path	desired_path;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help %s%s", "wdb_", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	--argc;
	++argv;

	/*
	 *	paths are matched up to last input member
	 *      ANY path the same up to this point is considered as matching
	 */

	/* find out which command was entered */
	if (strcmp(argv[0], "paths") == 0) {
		/* want to list all matching paths */
		flag = WDB_LISTPATH;
	}
	if (strcmp(argv[0], "WDB_LISTEVAL") == 0) {
		/* want to list evaluated solid[s] */
		flag = WDB_LISTEVAL;
	}

	if (argc == 2 && strchr(argv[1], '/')) {
		if( db_string_to_path( &desired_path, wdbp->dbip, argv[1] ) < 0 )
			goto err;
	} else {
		if( db_argv_to_path( &desired_path, wdbp->dbip, argc-1, (const char*const*)(argv+1) ) < 0 )
			goto err;
	}

	bn_mat_idn( wdb_xform );

	if( wdb_trace(interp, wdbp->dbip,
	    bn_mat_identity, flag, wdb_xform, &desired_path) != 0
	)  {
		db_free_full_path( &desired_path );
		return TCL_OK;
	}

err:
	/* path not found */
	Tcl_AppendResult(interp, "Path:  ", (char *)NULL);
	db_full_path_appendresult( interp, &desired_path );
	Tcl_AppendResult(interp, "  not found", (char *)NULL);

	db_free_full_path( &desired_path );
    	return TCL_ERROR;
}


static void
wdb_scrape_escapes_AppendResult(interp, str)
     Tcl_Interp *interp;
     char *str;
{
	char buf[2];
	buf[1] = '\0';
    
	while (*str) {
		buf[0] = *str;
		if (*str != '\\') {
			Tcl_AppendResult(interp, buf, NULL);
		} else if (*(str+1) == '\\') {
			Tcl_AppendResult(interp, buf, NULL);
			++str;
		}
		if (*str == '\0')
			break;
		++str;
	}
}

/*
 * Performs wildcard expansion (matched to the database elements)
 * on its given arguments.  The result is returned in interp->result.
 *
 * Usage:
 *        procname expand [args]
 */
static int
wdb_expand_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register char *pattern;
	register struct directory *dp;
	register int i, whicharg;
	int regexp, nummatch, thismatch, backslashed;

	if (argc < 2 || MAXARGS < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "help wdb_expand");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	nummatch = 0;
	backslashed = 0;
	for (whicharg = 2; whicharg < argc; whicharg++) {
		/* If * ? or [ are present, this is a regular expression */
		pattern = argv[whicharg];
		regexp = 0;
		do {
			if ((*pattern == '*' || *pattern == '?' || *pattern == '[') &&
			    !backslashed) {
				regexp = 1;
				break;
			}
			if (*pattern == '\\' && !backslashed)
				backslashed = 1;
			else
				backslashed = 0;
		} while (*pattern++);

		/* If it isn't a regexp, copy directly and continue */
		if (regexp == 0) {
			if (nummatch > 0)
				Tcl_AppendResult(interp, " ", NULL);
			wdb_scrape_escapes_AppendResult(interp, argv[whicharg]);
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
		for (i = 0; i < RT_DBNHASH; i++) {
			for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
				if (!db_regexp_match(pattern, dp->d_namep))
					continue;
				/* Successful match */
				if (nummatch == 0)
					Tcl_AppendResult(interp, dp->d_namep, NULL);
				else 
					Tcl_AppendResult(interp, " ", dp->d_namep, NULL);
				++nummatch;
				++thismatch;
			}
		}
		if (thismatch == 0) {
			if (nummatch > 0)
				Tcl_AppendResult(interp, " ", NULL);
			wdb_scrape_escapes_AppendResult(interp, argv[whicharg]);
		}
	}

	return TCL_OK;
}

/*
 * Usage:
 *        procname kill arg(s)
 */
static int
wdb_kill_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;
	int	is_phony;
	int	verbose = LOOKUP_NOISY;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_kill");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* skip past procname */
	argc--;
	argv++;

	/* skip past "-f" */
	if (argc > 1 && strcmp(argv[1], "-f") == 0) {
		verbose = LOOKUP_QUIET;
		argc--;
		argv++;
	}

	for (i = 1; i < argc; i++) {
		if ((dp = db_lookup(wdbp->dbip,  argv[i], verbose)) != DIR_NULL) {
			is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

			/* don't worry about phony objects */
			if (is_phony)
				continue;

			/* notify drawable geometry objects associated with this database object */
			dgo_eraseobjall_callback(wdbp->dbip, interp, dp);

			if (db_delete(wdbp->dbip, dp) < 0 ||
			    db_dirdelete(wdbp->dbip, dp) < 0) {
				/* Abort kill processing on first error */
				Tcl_AppendResult(interp,
						 "an error occurred while deleting ",
						 argv[i], (char *)NULL);
				return TCL_ERROR;
			}
		}
	}

	return TCL_OK;
}

/*
 * Kill object[s] and remove all references to the object[s].
 *
 * Usage:
 *        procname killall arg(s)
 */
static int
wdb_killall_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	register int	i,k;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if(argc < 3 || MAXARGS < argc){
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib  wdb_killall");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	ret = TCL_OK;

	/* Examine all COMB nodes */
	for (i = 0; i < RT_DBNHASH; i++) {
		for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
			if (!(dp->d_flags & DIR_COMB))
				continue;

			if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
						 ") failure", (char *)NULL );
				ret = TCL_ERROR;
				continue;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB(comb);

			for (k=2; k<argc; k++) {
				int	code;

				code = db_tree_del_dbleaf(&(comb->tree), argv[k], &rt_uniresource);
				if (code == -1)
					continue;	/* not found */
				if (code == -2)
					continue;	/* empty tree */
				if (code < 0) {
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

			if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
				Tcl_AppendResult(interp,
						 "ERROR: Unable to write new combination into database.\n",
						 (char *)NULL);
				ret = TCL_ERROR;
				continue;
			}
		}
	}

	if (ret != TCL_OK) {
		Tcl_AppendResult(interp,
				 "KILL skipped because of earlier errors.\n",
				 (char *)NULL);
		return ret;
	}

	/* ALL references removed...now KILL the object[s] */
	/* reuse argv[] */
	argv[1] = "kill";
	return wdb_kill_tcl(clientData, interp, argc, argv);
}

/*
 * Kill all paths belonging to an object.
 *
 * Usage:
 *        procname killtree arg(s)
 */
static int
wdb_killtree_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_killtree");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* skip past procname */
	argc--;
	argv++;

	for (i=1; i<argc; i++) {
		if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
			continue;

		/* ignore phony objects */
		if (dp->d_addr == RT_DIR_PHONY_ADDR)
			continue;

		db_functree(wdbp->dbip, dp,
			wdb_killtree_callback, wdb_killtree_callback, wdbp->wdb_resp,
			(genptr_t)interp );
	}

	return TCL_OK;
}

/*
 *			K I L L T R E E
 */
static void
wdb_killtree_callback(dbip, dp, ptr)
     struct db_i	*dbip;
     register struct directory *dp;
     genptr_t *ptr;
{
	Tcl_Interp *interp = (Tcl_Interp *)ptr;

	if (dbip == DBI_NULL)
		return;

	Tcl_AppendResult(interp, "KILL ", (dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
			 ":  ", dp->d_namep, "\n", (char *)NULL);

	/* notify drawable geometry objects associated with this database object */
	dgo_eraseobjall_callback(interp, dbip, dp);

	if (db_delete(dbip, dp) < 0 || db_dirdelete(dbip, dp) < 0) {
		Tcl_AppendResult(interp,
				 "an error occurred while deleting ",
				 dp->d_namep, "\n", (char *)NULL);
	}
}

/*
 * Usage:
 *        procname cp from to
 */
static int
wdb_copy_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *proto;
	register struct directory *dp;
	struct bu_external external;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_copy");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((proto = db_lookup(wdbp->dbip,  argv[2], LOOKUP_NOISY)) == DIR_NULL)
		return TCL_ERROR;

	if (db_lookup(wdbp->dbip, argv[3], LOOKUP_QUIET) != DIR_NULL) {
		Tcl_AppendResult(interp, argv[3], ":  already exists", (char *)NULL);
		return TCL_ERROR;
	}

	if (db_get_external(&external , proto , wdbp->dbip)) {
		Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	if ((dp=db_diradd(wdbp->dbip, argv[3], -1, proto->d_len, proto->d_flags, NULL)) == DIR_NULL ) {
		Tcl_AppendResult(interp,
				 "An error has occured while adding a new object to the database.",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if (db_put_external(&external, dp, wdbp->dbip) < 0) {
		bu_free_external(&external);
		Tcl_AppendResult(interp, "Database write error, aborting", (char *)NULL);
		return TCL_ERROR;
	}
	bu_free_external(&external);

	return TCL_OK;
}

/*
 * Rename an object.
 *
 * Usage:
 *        procname mv from to
 */
static int
wdb_move_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	struct rt_db_internal	intern;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_move");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((dp = db_lookup(wdbp->dbip,  argv[2], LOOKUP_NOISY)) == DIR_NULL)
		return TCL_ERROR;

	if (db_lookup(wdbp->dbip, argv[3], LOOKUP_QUIET) != DIR_NULL) {
		Tcl_AppendResult(interp, argv[3], ":  already exists", (char *)NULL);
		return TCL_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	/*  Change object name in the in-memory directory. */
	if (db_rename(wdbp->dbip, dp, argv[3]) < 0) {
		rt_db_free_internal(&intern, &rt_uniresource);
		Tcl_AppendResult(interp, "error in db_rename to ", argv[3],
				 ", aborting", (char *)NULL);
		return TCL_ERROR;
	}

	/* Re-write to the database.  New name is applied on the way out. */
	if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database write error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Rename all occurences of an object
 *
 * Usage:
 *        procname mvall from to
 */
static int
wdb_move_all_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register int	i;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal *comb;
	struct bu_ptbl		stack;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_moveall");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((int)strlen(argv[3]) > RT_NAMESIZE) {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "ERROR: name length limited to %d characters", RT_NAMESIZE);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
		return TCL_ERROR;
	}

	/* rename the record itself */
	if ((dp = db_lookup(wdbp->dbip, argv[2], LOOKUP_NOISY )) == DIR_NULL)
		return TCL_ERROR;

	if (db_lookup(wdbp->dbip, argv[3], LOOKUP_QUIET) != DIR_NULL) {
		Tcl_AppendResult(interp, argv[3], ":  already exists", (char *)NULL);
		return TCL_ERROR;
	}

	/*  Change object name in the directory. */
	if (db_rename(wdbp->dbip, dp, argv[3]) < 0) {
		Tcl_AppendResult(interp, "error in rename to ", argv[3],
				 ", aborting", (char *)NULL);
		return TCL_ERROR;
	}

	/* Change name in the file */
	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database write error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	bu_ptbl_init( &stack, 64, "combination stack for f_mvall" );

	/* Examine all COMB nodes */
	for (i = 0; i < RT_DBNHASH; i++) {
		for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
			union tree	*comb_leaf;
			int		done=0;
			int		changed=0;

			if (!(dp->d_flags & DIR_COMB))
				continue;

			if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
				continue;
			comb = (struct rt_comb_internal *)intern.idb_ptr;

			bu_ptbl_reset(&stack);
			/* visit each leaf in the combination */
			comb_leaf = comb->tree;
			if (comb_leaf) {
				while (!done) {
					while(comb_leaf->tr_op != OP_DB_LEAF) {
						bu_ptbl_ins(&stack, (long *)comb_leaf);
						comb_leaf = comb_leaf->tr_b.tb_left;
					}

					if (!strncmp(comb_leaf->tr_l.tl_name, argv[2], RT_NAMESIZE)) {
						bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
						comb_leaf->tr_l.tl_name = bu_strdup(argv[3]);
						changed = 1;
					}

					if (BU_PTBL_END(&stack) < 1) {
						done = 1;
						break;
					}
					comb_leaf = (union tree *)BU_PTBL_GET(&stack, BU_PTBL_END(&stack)-1);
					if (comb_leaf->tr_op != OP_DB_LEAF) {
						bu_ptbl_rm( &stack, (long *)comb_leaf );
						comb_leaf = comb_leaf->tr_b.tb_right;
					}
				}
			}

			if (changed) {
				if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource)) {
					bu_ptbl_free( &stack );
					rt_db_free_internal( &intern, &rt_uniresource );
					Tcl_AppendResult(interp,
							 "Database write error, aborting",
							 (char *)NULL);
					return TCL_ERROR;
				}
			}
			else
				rt_db_free_internal(&intern, &rt_uniresource);
		}
	}

	bu_ptbl_free(&stack);
	return TCL_OK;
}

static void
wdb_do_update(dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3)
     struct db_i		*dbip;
     struct rt_comb_internal *comb;
     union tree		*comb_leaf;
     genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	char	*mref;
	char	mref4[RT_NAMESIZE+2];
	struct bu_vls mref5;
	struct bu_vls *prestr;
	int	*ncharadd;

	if(dbip == DBI_NULL)
		return;

	RT_CK_DBI(dbip);
	RT_CK_TREE(comb_leaf);

	ncharadd = (int *)user_ptr1;
	prestr = (struct bu_vls *)user_ptr2;

	if( dbip->dbi_version < 5 ) {
		mref = mref4;
		(void)strncpy(mref, bu_vls_addr( prestr ), *ncharadd);
		(void)strncpy(mref+(*ncharadd),
			      comb_leaf->tr_l.tl_name,
			      RT_NAMESIZE-(*ncharadd) );
	} else {
		bu_vls_init( &mref5 );
		bu_vls_vlscat( &mref5, prestr );
		bu_vls_strcat( &mref5, comb_leaf->tr_l.tl_name );
		mref = bu_vls_addr( &mref5 );
	}
	bu_free(comb_leaf->tr_l.tl_name, "comb_leaf->tr_l.tl_name");
	comb_leaf->tr_l.tl_name = bu_strdup(mref);

	if( dbip->dbi_version >= 5 )
		bu_vls_free( &mref5 );
}

static int wdb_dir_add BU_ARGS((struct db_i *input_dbip, CONST char
	*name, long laddr, int len, int flags, genptr_t ptr));

struct dir_add_stuff {
	Tcl_Interp	*interp;
	struct db_i	*main_dbip;		/* the main database */
	struct rt_wdb	*wdbp;
};

/*
 *			W D B _ D I R _ A D D 5
 * V5 version of wdb_dir_add
 */
static void
wdb_dir_add5(
	struct db_i		*dbip,		/* db_i to add this object to */
	const struct db5_raw_internal *rip,
	long			laddr,
	genptr_t		client_data )
{
	struct bu_vls 		local5;
	char			*local;
	struct bu_external	ext;
	struct dir_add_stuff	*dasp = (struct dir_add_stuff *)client_data;
	struct db5_raw_internal	raw;
	struct directory	*dp;

	RT_CK_DBI( dbip );

	if( rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT )  return;
	if( rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE ) {
		/* Record available free storage */
		rt_memfree( &(dbip->dbi_freep), rip->object_length, laddr );
		return;
	}
	
	/* If somehow it doesn't have a name, ignore it */
	if( rip->name.ext_buf == NULL )  return;

	if(rt_g.debug&DEBUG_DB) {
		bu_log("wdb_dir_add5(dbip=x%x, name='%s', addr=x%x, len=%d)\n",
			dbip, (char *)rip->name.ext_buf, rip->object_length );
	}

	/* add to its own directory first */
	(void)db5_diradd( dbip, rip, laddr, NULL );

	/* do not concat GLOBAL object */
	if( rip->major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	    rip->minor_type == 0 )
		return;

	/* now add this object to the current database */
	bu_vls_init( &local5 );

	/* Add the prefix, if any */
	if( dasp->wdbp->wdb_ncharadd > 0 ) {
		bu_vls_vlscat( &local5, &dasp->wdbp->wdb_prestr );
		bu_vls_strcat( &local5, rip->name.ext_buf );
	} else {
		bu_vls_strcat( &local5, rip->name.ext_buf );
	}
	local = bu_vls_addr( &local5 );

	if( rip->minor_type != DB5_MINORTYPE_BRLCAD_COMBINATION ) {

		/* export object */
		db5_export_object3( &ext, rip->h_dli, local, 0, &rip->attributes,
			    &rip->body, rip->major_type, rip->minor_type,
			    rip->a_zzz, rip->b_zzz );

		if( db5_get_raw_internal_ptr( &raw, ext.ext_buf ) == NULL ) {
			Tcl_AppendResult(dasp->interp,
					 "db5_get_raw_internal_ptr() failed for ",
					 local,
					 ". This object ignored.\n",
					 (char *)NULL );
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			return;
		}

		/* add to the main directory */
		dp = (struct directory *)db5_diradd( dasp->main_dbip, &raw, -1L, NULL );
		dp->d_len = 0;

		/* write to main database */
		if( db_put_external5( &ext, dp, dasp->main_dbip ) ) {
			Tcl_AppendResult(dasp->interp,
					 "db_put_external5() failed for ",
					 rip->name.ext_buf,
					 ". This object ignored.\n",
					 (char *)NULL );
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			return;
		}

		Tcl_AppendResult(dasp->interp,
				 "Added ",
				 local,
				 "\n",
				 (char *)NULL );

		bu_vls_free( &local5 );
		bu_free( ext.ext_buf, "ext.ext_buf" );
	} else if( rip->major_type == DB5_MAJORTYPE_BRLCAD &&
		   rip->minor_type == DB5_MINORTYPE_BRLCAD_COMBINATION ) { 
		struct rt_db_internal in;
		struct rt_comb_internal *comb;

		/* export object */
		db5_export_object3( &ext, rip->h_dli, local, 0, &rip->attributes,
			    &rip->body, rip->major_type, rip->minor_type,
			    rip->a_zzz, rip->b_zzz );

		if( db5_get_raw_internal_ptr( &raw, ext.ext_buf ) == NULL ) {
			Tcl_AppendResult(dasp->interp,
					 "db5_get_raw_internal_ptr() failed for ",
					 local,
					 ". This object ignored.\n",
					 (char *)NULL );
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			return;
		}

		RT_INIT_DB_INTERNAL( &in );
		if( rip->attributes.ext_nbytes > 0 ) {
			if( db5_import_attributes( &in.idb_avs, &rip->attributes ) < 0 )  {
				Tcl_AppendResult(dasp->interp,
						 "db5_import_attributes() failed for ",
						 local,
						 " (combination), this object will be missing attributes.\n",
						 (char *)NULL );
			}
		} else {
			in.idb_avs.magic = BU_AVS_MAGIC;
			in.idb_avs.count = 0;
		}

		if( rt_comb_import5( &in, &rip->body, NULL, dasp->main_dbip, dasp->wdbp->wdb_resp, 0 ) ) {
			Tcl_AppendResult(dasp->interp,
					 "rt_comb_import5() Failed for ",
					 local,
					 ". Skipping this combination.\n",
					 (char *)NULL );
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			return;
		}
		comb = (struct rt_comb_internal *)in.idb_ptr;
		RT_CHECK_COMB( comb );
		if (dasp->wdbp->wdb_ncharadd && comb->tree) {
			db_tree_funcleaf(dasp->main_dbip, comb, comb->tree, wdb_do_update,
			 (genptr_t)&(dasp->wdbp->wdb_ncharadd),
			 (genptr_t)&(dasp->wdbp->wdb_prestr), (genptr_t)NULL);
		}

		/* add to the main directory */
		dp = (struct directory *)db5_diradd( dasp->main_dbip, &raw, -1L, NULL );
		dp->d_len = 0;

		if (rt_db_put_internal(dp, dasp->main_dbip, &in, dasp->wdbp->wdb_resp ) < 0) {
			Tcl_AppendResult(dasp->interp,
				 "Failed writing ",
				 dp->d_namep, " to database\n", (char *)NULL);
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			rt_db_free_internal( &in, dasp->wdbp->wdb_resp );
			return;
		}
		Tcl_AppendResult(dasp->interp,
				 "Added combination ",
				 local,
				 "\n",
				 (char *)NULL );
		bu_vls_free( &local5 );
		rt_db_free_internal( &in, dasp->wdbp->wdb_resp );
		bu_free( ext.ext_buf, "ext.ext_buf" );
	} else {
		Tcl_AppendResult(dasp->interp,
				 "Skipping non-BRLCAD object ",
				 local,
				 ", not yet supported\n",
				 (char *)NULL );
			bu_vls_free( &local5 );
			bu_free( ext.ext_buf, "ext.ext_buf" );
			return;
	}
}

/*
 *			W D B _ D I R _ A D D
 *
 *  Add a solid or conbination from an auxillary database
 *  into the primary database.
 */
static int
wdb_dir_add(input_dbip, name, laddr, len, flags, ptr)
     register struct db_i	*input_dbip;
     register CONST char	*name;
     long			laddr;
     int			len;
     int			flags;
     genptr_t			ptr;
{
	register struct directory *input_dp;
	register struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	struct bu_vls		local5;
	char			*local;
	char			local4[RT_NAMESIZE+2+2];
	struct dir_add_stuff	*dasp = (struct dir_add_stuff *)ptr;

	RT_CK_DBI(input_dbip);

	/* Add the prefix, if any */
	if( dasp->main_dbip->dbi_version < 5 ) {
		local = local4;
		if (dasp->wdbp->wdb_ncharadd > 0) {
			(void)strncpy(local, bu_vls_addr( &dasp->wdbp->wdb_prestr ), dasp->wdbp->wdb_ncharadd);
			(void)strncpy(local+dasp->wdbp->wdb_ncharadd, name, RT_NAMESIZE-dasp->wdbp->wdb_ncharadd);
		} else {
			(void)strncpy(local, name, RT_NAMESIZE);
		}
		local[RT_NAMESIZE] = '\0';
	} else {
		bu_vls_init( &local5 );
		bu_vls_vlscat( &local5, &dasp->wdbp->wdb_prestr );
		bu_vls_strcat( &local5, name );
		local = bu_vls_addr( &local5 );
	}
		
	/* Look up this new name in the existing (main) database */
	if ((dp = db_lookup(dasp->main_dbip, local, LOOKUP_QUIET)) != DIR_NULL) {
		register int	c;
		char		*loc2;
		char		loc2_4[RT_NAMESIZE+2+2];

		/* This object already exists under the (prefixed) name */
		/* Protect the database against duplicate names! */
		/* Change object names, but NOT any references made by combinations. */
		if( dasp->main_dbip->dbi_version < 5 ) {
			loc2 = loc2_4;
			(void)strncpy( loc2, local, RT_NAMESIZE );
			/* Shift name right two characters, and further prefix */
			strncpy(local+2, loc2, RT_NAMESIZE-2);
			local[1] = '_';			/* distinctive separater */
			local[RT_NAMESIZE] = '\0';	/* ensure null termination */

			for (c = 'A'; c <= 'Z'; c++) {
				local[0] = c;
				if ((dp = db_lookup(dasp->main_dbip, local, LOOKUP_QUIET)) == DIR_NULL)
					break;
			}
		} else {
			for( c = 'A' ; c <= 'Z' ; c++ ) {
				bu_vls_trunc( &local5, 0 );
				bu_vls_putc( &local5, c );
				bu_vls_putc( &local5, '_' );
				bu_vls_strncat( &local5, bu_vls_addr( &dasp->wdbp->wdb_prestr ), dasp->wdbp->wdb_ncharadd);
				bu_vls_strcat( &local5, name );
				local = bu_vls_addr( &local5 );
				if ((dp = db_lookup(dasp->main_dbip, local, LOOKUP_QUIET)) == DIR_NULL)
					break;
			}
			loc2 = bu_vls_addr( &local5 ) + 2;
		}
		if (c > 'Z') {
			Tcl_AppendResult(dasp->interp,
					 "wdb_dir_add: Duplicate of name '",
					 local, "', ignored\n", (char *)NULL);
			if( dasp->main_dbip->dbi_version >= 5 )
				bu_vls_free( &local5 );
			return 0;
		}
		Tcl_AppendResult(dasp->interp,
				 "mged_dir_add: Duplicate of '",
				 loc2, "' given new name '",
				 local, "'\nYou should have used the 'dup' command to detect this,\nand then specified a prefix for the 'dbconcat' command.\n");
	}

	/* First, register this object in input database */
	if ((input_dp = db_diradd(input_dbip, name, laddr, len, flags, NULL)) == DIR_NULL) {
		if( dasp->main_dbip->dbi_version >= 5 )
			bu_vls_free( &local5 );
		return(-1);
	}

	/* Then, register a new object in the main database */
	if ((dp = db_diradd(dasp->main_dbip, local, -1L, 0, flags, NULL)) == DIR_NULL) {
		if( dasp->main_dbip->dbi_version >= 5 )
			bu_vls_free( &local5 );
		return(-1);
	}

	if (rt_db_get_internal(&intern, input_dp, input_dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(dasp->interp, "Database read error, aborting\n", (char *)NULL);
		if (db_delete(dasp->main_dbip, dp) < 0 ||
		    db_dirdelete(dasp->main_dbip, dp) < 0) {
			Tcl_AppendResult(dasp->interp, "Database write error, aborting\n", (char *)NULL);
		}
		if( dasp->main_dbip->dbi_version >= 5 )
			bu_vls_free( &local5 );
	    	/* Abort processing on first error */
		return -1;
	}

	/* Update any references.  Name is already correct. */
	if (flags & DIR_SOLID) {
		Tcl_AppendResult(dasp->interp,
				 "adding solid '",
				 local, "'\n", (char *)NULL);
		if (dasp->main_dbip->dbi_version < 5 && (dasp->wdbp->wdb_ncharadd + strlen(name)) > (unsigned)RT_NAMESIZE)
			Tcl_AppendResult(dasp->interp,
					 "WARNING: solid name \"",
					 bu_vls_addr( &dasp->wdbp->wdb_prestr ), name,
					 "\" truncated to \"",
					 local, "\"\n", (char *)NULL);
	} else if(flags & DIR_COMB) {
		Tcl_AppendResult(dasp->interp,
				 "adding  comb '",
				 local, "'\n", (char *)NULL);

		/* Update all the member records */
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		if (dasp->wdbp->wdb_ncharadd && comb->tree) {
			db_tree_funcleaf(dasp->main_dbip, comb, comb->tree, wdb_do_update,
			 (genptr_t)&(dasp->wdbp->wdb_ncharadd),
			 (genptr_t)&(dasp->wdbp->wdb_prestr), (genptr_t)NULL);
		}
	} else {
		Tcl_AppendResult(dasp->interp,
				 "WARNING: object name \"",
				 bu_vls_addr( &dasp->wdbp->wdb_prestr ), name,
				 "\" is of an unsupported type, not copied.\n",
				 (char *)NULL);
		return -1;
	}

	if (rt_db_put_internal(dp, dasp->main_dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(dasp->interp,
				 "Failed writing ",
				 dp->d_namep, " to database\n", (char *)NULL);
		if( dasp->main_dbip->dbi_version >= 5 )
			bu_vls_free( &local5 );
		return( -1 );
	}

	if( dasp->main_dbip->dbi_version >= 5 )
		bu_vls_free( &local5 );
	return 0;
}

/*
 *  Concatenate another GED file into the current file.
 *
 * Usage:
 *        procname concat file.g prefix
 */
static int
wdb_concat_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct db_i		*newdbp;
	int bad = 0;
	struct dir_add_stuff	das;
	int version;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc != 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_concat");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_trunc( &wdbp->wdb_prestr, 0 );
	if (strcmp(argv[3], "/") != 0) {
		(void)bu_vls_strcpy(&wdbp->wdb_prestr, argv[3]);
	}

	if( wdbp->dbip->dbi_version < 5 ) {
		if ((wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr)) > 12) {
			wdbp->wdb_ncharadd = 12;
			bu_vls_trunc( &wdbp->wdb_prestr, 12 );
		}
	} else {
		wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr);
	}

	/* open the input file */
	if ((newdbp = db_open(argv[2], "r")) == DBI_NULL) {
		perror(argv[2]);
		Tcl_AppendResult(interp, "concat: Can't open ",
				 argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	/* Scan new database, adding everything encountered. */
	version = db_get_version( newdbp );
	if( version > 4 && wdbp->dbip->dbi_version < 5 ) {
		Tcl_AppendResult(interp, "concat: databases are incompatible, convert ",
				 wdbp->dbip->dbi_filename, " to version 5 first",
				 (char *)NULL );
		return TCL_ERROR;
	}

	das.interp = interp;
	das.main_dbip = wdbp->dbip;
	das.wdbp = wdbp;
	if( version < 5 ) {
		if (db_scan(newdbp, wdb_dir_add, 1, (genptr_t)&das) < 0) {
			Tcl_AppendResult(interp, "concat: db_scan failure", (char *)NULL);
			bad = 1;	
			/* Fall through, to close off database */
		}
	} else {
		if (db5_scan(newdbp, wdb_dir_add5, (genptr_t)&das) < 0) {
			Tcl_AppendResult(interp, "concat: db_scan failure", (char *)NULL);
			bad = 1;	
			/* Fall through, to close off database */
		}
	}
	rt_mempurge( &(newdbp->dbi_freep) );        /* didn't really build a directory */

	/* Free all the directory entries, and close the input database */
	db_close(newdbp);

	db_sync(wdbp->dbip);	/* force changes to disk */

	return bad ? TCL_ERROR : TCL_OK;
}

BU_EXTERN(int wdb_dir_check, ( struct
db_i *input_dbip, CONST char *name, long laddr, int len, int flags,
genptr_t ptr));

struct dir_check_stuff {
 	struct db_i	*main_dbip;
	struct rt_wdb	*wdbp;
	struct directory **dup_dirp;
};

BU_EXTERN(void wdb_dir_check5, ( struct db_i *input_dbip, const struct db5_raw_internal *rip, long addr, genptr_t ptr));

void
wdb_dir_check5( input_dbip, rip, addr, ptr )
     register struct db_i		*input_dbip;
     const struct db5_raw_internal	*rip;
     long				addr;
     genptr_t				ptr;
{
	char			*name;
	struct directory	*dupdp;
	struct bu_vls		local;
	struct dir_check_stuff	*dcsp = (struct dir_check_stuff *)ptr;

	if (dcsp->main_dbip == DBI_NULL)
		return;

	RT_CK_DBI(input_dbip);
	RT_CK_RIP( rip );

	if( rip->h_dli == DB5HDR_HFLAGS_DLI_HEADER_OBJECT ) return;
	if( rip->h_dli == DB5HDR_HFLAGS_DLI_FREE_STORAGE ) return;

	name = (char *)rip->name.ext_buf;

	if( name == (char *)NULL ) return;

	/* do not compare _GLOBAL */
	if( rip->major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY &&
	    rip->minor_type == 0 )
		return;

	/* Add the prefix, if any */
	bu_vls_init( &local );
	if( dcsp->main_dbip->dbi_version < 5 ) {
		if (dcsp->wdbp->wdb_ncharadd > 0) {
			bu_vls_strncpy( &local, bu_vls_addr( &dcsp->wdbp->wdb_prestr ), dcsp->wdbp->wdb_ncharadd );
			bu_vls_strcat( &local, name );
		} else {
			bu_vls_strncpy( &local, name, RT_NAMESIZE );
		}
		bu_vls_trunc( &local, RT_NAMESIZE );
	} else {
		if (dcsp->wdbp->wdb_ncharadd > 0) {
			(void)bu_vls_vlscat( &local, &dcsp->wdbp->wdb_prestr );
			(void)bu_vls_strcat( &local, name );
		} else {
			(void)bu_vls_strcat( &local, name );
		}
	}
		
	/* Look up this new name in the existing (main) database */
	if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr( &local ), LOOKUP_QUIET)) != DIR_NULL) {
		/* Duplicate found, add it to the list */
		dcsp->wdbp->wdb_num_dups++;
		*dcsp->dup_dirp++ = dupdp;
	}
	return;
}

/*
 *			W D B _ D I R _ C H E C K
 *
 * Check a name against the global directory.
 */
int
wdb_dir_check(input_dbip, name, laddr, len, flags, ptr)
     register struct db_i	*input_dbip;
     register CONST char	*name;
     long			laddr;
     int			len;
     int			flags;
     genptr_t			ptr;
{
	struct directory	*dupdp;
	struct bu_vls		local;
	struct dir_check_stuff	*dcsp = (struct dir_check_stuff *)ptr;

	if (dcsp->main_dbip == DBI_NULL)
		return 0;

	RT_CK_DBI(input_dbip);

	/* Add the prefix, if any */
	bu_vls_init( &local );
	if( dcsp->main_dbip->dbi_version < 5 ) {
		if (dcsp->wdbp->wdb_ncharadd > 0) {
			bu_vls_strncpy( &local, bu_vls_addr( &dcsp->wdbp->wdb_prestr ), dcsp->wdbp->wdb_ncharadd );
			bu_vls_strcat( &local, name );
		} else {
			bu_vls_strncpy( &local, name, RT_NAMESIZE );
		}
		bu_vls_trunc( &local, RT_NAMESIZE );
	} else {
		if (dcsp->wdbp->wdb_ncharadd > 0) {
			bu_vls_vlscat( &local, &dcsp->wdbp->wdb_prestr );
			bu_vls_strcat( &local, name );
		} else {
			bu_vls_strcat( &local, name );
		}
	}
		
	/* Look up this new name in the existing (main) database */
	if ((dupdp = db_lookup(dcsp->main_dbip, bu_vls_addr( &local ), LOOKUP_QUIET)) != DIR_NULL) {
		/* Duplicate found, add it to the list */
		dcsp->wdbp->wdb_num_dups++;
		*dcsp->dup_dirp++ = dupdp;
	}
	bu_vls_free( &local );
	return 0;
}

/*
 * Usage:
 *        procname dup file.g [prefix]
 */
static int
wdb_dup_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	struct db_i		*newdbp = DBI_NULL;
	struct directory	**dirp0 = (struct directory **)NULL;
	struct bu_vls vls;
	struct dir_check_stuff	dcs;

	if (argc < 3 || argc > 4) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_dup");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_trunc( &wdbp->wdb_prestr, 0 );
	if( argc == 4 )
		(void)bu_vls_strcpy(&wdbp->wdb_prestr, argv[3]);

	wdbp->wdb_num_dups = 0;
	if( wdbp->dbip->dbi_version < 5 ) {
		if ((wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr)) > 12) {
			wdbp->wdb_ncharadd = 12;
			bu_vls_trunc( &wdbp->wdb_prestr, 12 );
		}
	} else {
		wdbp->wdb_ncharadd = bu_vls_strlen(&wdbp->wdb_prestr);
	}

	/* open the input file */
	if ((newdbp = db_open(argv[2], "r")) == DBI_NULL) {
		perror(argv[2]);
		Tcl_AppendResult(interp, "dup: Can't open ", argv[2], (char *)NULL);
		return TCL_ERROR;
	}

	Tcl_AppendResult(interp, "\n*** Comparing ",
			wdbp->dbip->dbi_filename,
			 "  with ", argv[2], " for duplicate names\n", (char *)NULL);
	if (wdbp->wdb_ncharadd) {
		Tcl_AppendResult(interp, "  For comparison, all names in ",
				 argv[2], " were prefixed with:  ",
				 bu_vls_addr( &wdbp->wdb_prestr ), "\n", (char *)NULL);
	}

	/* Get array to hold names of duplicates */
	if ((dirp0 = wdb_getspace(wdbp->dbip, 0)) == (struct directory **) 0) {
		Tcl_AppendResult(interp, "f_dup: unable to get memory\n", (char *)NULL);
		db_close( newdbp );
		return TCL_ERROR;
	}

	/* Scan new database for overlaps */
	dcs.main_dbip = wdbp->dbip;
	dcs.wdbp = wdbp;
	dcs.dup_dirp = dirp0;
	if( newdbp->dbi_version < 5 ) {
		if (db_scan(newdbp, wdb_dir_check, 0, (genptr_t)&dcs) < 0) {
			Tcl_AppendResult(interp, "dup: db_scan failure", (char *)NULL);
			bu_free((genptr_t)dirp0, "wdb_getspace array");
			db_close(newdbp);
			return TCL_ERROR;
		}
	} else {
		if( db5_scan( newdbp, wdb_dir_check5, (genptr_t)&dcs) < 0) {
			Tcl_AppendResult(interp, "dup: db_scan failure", (char *)NULL);
			bu_free((genptr_t)dirp0, "wdb_getspace array");
			db_close(newdbp);
			return TCL_ERROR;
		}
	}
	rt_mempurge( &(newdbp->dbi_freep) );        /* didn't really build a directory */

	bu_vls_init(&vls);
	wdb_vls_col_pr4v(&vls, dirp0, (int)(dcs.dup_dirp - dirp0));
	bu_vls_printf(&vls, "\n -----  %d duplicate names found  -----", wdbp->wdb_num_dups);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
	bu_free((genptr_t)dirp0, "wdb_getspace array");
	db_close(newdbp);

	return TCL_OK;
}

/*
 * Usage:
 *        procname group groupname object(s)
 */
static int
wdb_group_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int i;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_group");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* get objects to add to group */
	for (i = 3; i < argc; i++) {
		if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) != DIR_NULL) {
			if (wdb_combadd(interp, wdbp->dbip, dp, argv[2], 0,
					WMOP_UNION, 0, 0, wdbp) == DIR_NULL)
				return TCL_ERROR;
		}  else
			Tcl_AppendResult(interp, "skip member ", argv[i], "\n", (char *)NULL);
	}
	return TCL_OK;
}

/*
 * Remove members from a combination.
 *
 * Usage:
 *        procname remove comb object(s)
 */
static int
wdb_remove_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int	i;
	int		num_deleted;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_remove");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((dp = db_lookup(wdbp->dbip,  argv[2], LOOKUP_NOISY)) == DIR_NULL)
		return TCL_ERROR;

	if ((dp->d_flags & DIR_COMB) == 0) {
		Tcl_AppendResult(interp, "rm: ", dp->d_namep,
				 " is not a combination", (char *)NULL );
		return TCL_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	/* Process each argument */
	num_deleted = 0;
	ret = TCL_OK;
	for (i = 3; i < argc; i++) {
		if (db_tree_del_dbleaf( &(comb->tree), argv[i], &rt_uniresource ) < 0) {
			Tcl_AppendResult(interp, "  ERROR_deleting ",
					 dp->d_namep, "/", argv[i],
					 "\n", (char *)NULL);
			ret = TCL_ERROR;
		} else {
			Tcl_AppendResult(interp, "deleted ",
					 dp->d_namep, "/", argv[i],
					 "\n", (char *)NULL);
			num_deleted++;
		}
	}

	if (rt_db_put_internal(dp, wdbp->dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database write error, aborting", (char *)NULL);
		return TCL_ERROR;
	}

	return ret;
}

/*
 * Usage:
 *        procname r object(s)
 */
static int
wdb_region_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	int i;
	char oper;
	struct bu_list head;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_region");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	BU_LIST_INIT(&head);

	/* skip past procname */
	--argc;
	++argv;
 
	/* Check for even number of arguments */
	if (argc & 01) {
		Tcl_AppendResult(interp, "error in number of args!", (char *)NULL);
		return TCL_ERROR;
	}

	if (db_lookup(wdbp->dbip, argv[1], LOOKUP_QUIET) == DIR_NULL) {
		/* will attempt to create the region */
		if (wdbp->wdb_item_default) {
			struct bu_vls tmp_vls;

			wdbp->wdb_item_default++;
			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls, "Defaulting item number to %d\n",
				wdbp->wdb_item_default);
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
		}
	}

	/* Get operation and solid name for each solid */
	for (i = 2; i < argc; i += 2) {
		if (argv[i][1] != '\0') {
			Tcl_AppendResult(interp, "bad operation: ", argv[i],
					 " skip member: ", argv[i+1], "\n", (char *)NULL);
			continue;
		}
		oper = argv[i][0];
		if ((dp = db_lookup(wdbp->dbip,  argv[i+1], LOOKUP_NOISY )) == DIR_NULL ) {
			Tcl_AppendResult(interp, "skipping ", argv[i+1], "\n", (char *)NULL);
			continue;
		}

		if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
			struct bu_vls tmp_vls;

			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				      oper, dp->d_namep );
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
			continue;
		}

		/* Adding region to region */
		if (dp->d_flags & DIR_REGION) {
			Tcl_AppendResult(interp, "Note: ", dp->d_namep,
					 " is a region\n", (char *)NULL);
		}

		mk_addmember(argv[i+1], &head, oper);
	}

	if (mk_comb(wdbp, argv[1], &head,
		    1, NULL, NULL, NULL,
		    wdbp->wdb_item_default, wdbp->wdb_air_default,
		    wdbp->wdb_mat_default, wdbp->wdb_los_default,
		    0, 1, 1) < 0) {
		/* failed to create region */
		if (wdbp->wdb_item_default > 1)
			wdbp->wdb_item_default--;

		Tcl_AppendResult(interp,
				 "An error has occured while adding '",
				 argv[1], "' to the database.\n", (char *)NULL);
		WDB_TCL_ERROR_RECOVERY_SUGGESTION;
		return TCL_ERROR;
	}

	return TCL_OK;
}

/*
 * Create or add to the end of a combination, with one or more solids,
 * with explicitly specified operations.
 *
 * Usage:
 *        procname comb comb_name opr1 sol1 opr2 sol2 ... oprN solN
 */
static int
wdb_comb_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	char	*comb_name;
	register int	i;
	char	oper;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	/* skip past procname */
	--argc;
	++argv;

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_comb");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Check for odd number of arguments */
	if (argc & 01) {
		Tcl_AppendResult(interp, "error in number of args!", (char *)NULL);
		return TCL_ERROR;
	}

	/* Save combination name, for use inside loop */
	comb_name = argv[1];
	if ((dp=db_lookup(wdbp->dbip, comb_name, LOOKUP_QUIET)) != DIR_NULL) {
		if (!(dp->d_flags & DIR_COMB)) {
			Tcl_AppendResult(interp,
					 "ERROR: ", comb_name,
					 " is not a combination", (char *)0 );
			return TCL_ERROR;
		}
	}

	/* Get operation and solid name for each solid */
	for (i = 2; i < argc; i += 2) {
		if (argv[i][1] != '\0') {
			Tcl_AppendResult(interp, "bad operation: ", argv[i],
					 " skip member: ", argv[i+1], "\n", (char *)NULL);
			continue;
		}
		oper = argv[i][0];
		if ((dp = db_lookup(wdbp->dbip,  argv[i+1], LOOKUP_NOISY)) == DIR_NULL) {
			Tcl_AppendResult(interp, "skipping ", argv[i+1], "\n", (char *)NULL);
			continue;
		}

		if (oper != WMOP_UNION && oper != WMOP_SUBTRACT && oper != WMOP_INTERSECT) {
			struct bu_vls tmp_vls;

			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls, "bad operation: %c skip member: %s\n",
				      oper, dp->d_namep);
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			continue;
		}

		if (wdb_combadd(interp, wdbp->dbip, dp, comb_name, 0, oper, 0, 0, wdbp) == DIR_NULL) {
			Tcl_AppendResult(interp, "error in combadd", (char *)NULL);
			return TCL_ERROR;
		}
	}

	if (db_lookup(wdbp->dbip, comb_name, LOOKUP_QUIET) == DIR_NULL) {
		Tcl_AppendResult(interp, "Error:  ", comb_name,
				 " not created", (char *)NULL);
		return TCL_ERROR;
	}

	return TCL_OK;
}

static void
wdb_find_ref(dbip, comb, comb_leaf, object, comb_name_ptr, user_ptr3)
     struct db_i		*dbip;
     struct rt_comb_internal	*comb;
     union tree		*comb_leaf;
     genptr_t		object;
     genptr_t		comb_name_ptr;
     genptr_t		user_ptr3;
{
	char *obj_name;
	char *comb_name;
	Tcl_Interp *interp = (Tcl_Interp *)user_ptr3;

	RT_CK_TREE(comb_leaf);

	obj_name = (char *)object;
	if (strcmp(comb_leaf->tr_l.tl_name, obj_name))
		return;

	comb_name = (char *)comb_name_ptr;

	Tcl_AppendElement(interp, comb_name);
}

/*
 * Usage:
 *        procname find object(s)
 */
static int
wdb_find_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register int	i,k;
	register struct directory *dp;
	struct rt_db_internal intern;
	register struct rt_comb_internal *comb=(struct rt_comb_internal *)NULL;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_find");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Examine all COMB nodes */
	for (i = 0; i < RT_DBNHASH; i++) {
		for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
			if (!(dp->d_flags & DIR_COMB))
				continue;

			if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
				Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
				return TCL_ERROR;
			}

			comb = (struct rt_comb_internal *)intern.idb_ptr;
			for (k=2; k<argc; k++)
				db_tree_funcleaf(wdbp->dbip, comb, comb->tree, wdb_find_ref, (genptr_t)argv[k], (genptr_t)dp->d_namep, (genptr_t)interp);

			rt_db_free_internal(&intern, &rt_uniresource);
		}
	}

	return TCL_OK;
}

struct wdb_id_names {
	struct bu_list l;
	struct bu_vls name;		/* name associated with region id */
};

struct wdb_id_to_names {
	struct bu_list l;
	int id;				/* starting id (i.e. region id or air code) */
	struct wdb_id_names headName;	/* head of list of names */
};

/*
 * Usage:
 *        procname whichair/whichid [-s] id(s)
 */
static int
wdb_which_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	register int	i,j;
	register struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	struct wdb_id_to_names headIdName;
	struct wdb_id_to_names *itnp;
	struct wdb_id_names *inp;
	int isAir;
	int sflag;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_%s", argv[1]);
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (!strcmp(argv[1], "whichair"))
		isAir = 1;
	else
		isAir = 0;

	if (strcmp(argv[2], "-s") == 0) {
		--argc;
		++argv;

		if (argc < 2) {
			struct bu_vls vls;

			bu_vls_init(&vls);
			bu_vls_printf(&vls, "helplib wdb_%s", argv[-1]);
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_ERROR;
		}

		sflag = 1;
	} else {
		sflag = 0;
	}

	BU_LIST_INIT(&headIdName.l);

	/* Build list of id_to_names */
	for (j=2; j<argc; j++) {
		int n;
		int start, end;
		int range;
		int k;

		n = sscanf(argv[j], "%d%*[:-]%d", &start, &end);
		switch (n) {
		case 1:
			for (BU_LIST_FOR(itnp,wdb_id_to_names,&headIdName.l))
				if (itnp->id == start)
					break;

			/* id not found */
			if (BU_LIST_IS_HEAD(itnp,&headIdName.l)) {
				BU_GETSTRUCT(itnp,wdb_id_to_names);
				itnp->id = start;
				BU_LIST_INSERT(&headIdName.l,&itnp->l);
				BU_LIST_INIT(&itnp->headName.l);
			}

			break;
		case 2:
			if (start < end)
				range = end - start + 1;
			else if (end < start) {
				range = start - end + 1;
				start = end;
			} else
				range = 1;

			for (k = 0; k < range; ++k) {
				int id = start + k;

				for (BU_LIST_FOR(itnp,wdb_id_to_names,&headIdName.l))
					if (itnp->id == id)
						break;

				/* id not found */
				if (BU_LIST_IS_HEAD(itnp,&headIdName.l)) {
					BU_GETSTRUCT(itnp,wdb_id_to_names);
					itnp->id = id;
					BU_LIST_INSERT(&headIdName.l,&itnp->l);
					BU_LIST_INIT(&itnp->headName.l);
				}
			}

			break;
		}
	}

	/* Examine all COMB nodes */
	for (i = 0; i < RT_DBNHASH; i++) {
		for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
			if (!(dp->d_flags & DIR_REGION))
				continue;

			if (rt_db_get_internal( &intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource ) < 0) {
				Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
				return TCL_ERROR;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			if (comb->region_id != 0 && comb->aircode != 0) {
				Tcl_AppendResult(interp, "ERROR: ", dp->d_namep,
						 " has id and aircode!!!\n", (char *)NULL);
				continue;
			}

			/* check to see if the region id or air code matches one in our list */
			for (BU_LIST_FOR(itnp,wdb_id_to_names,&headIdName.l)) {
				if ((!isAir && comb->region_id == itnp->id) ||
				    (isAir && comb->aircode == itnp->id)) {
					/* add region name to our name list for this region */
					BU_GETSTRUCT(inp,wdb_id_names);
					bu_vls_init(&inp->name);
					bu_vls_strcpy(&inp->name, dp->d_namep);
					BU_LIST_INSERT(&itnp->headName.l,&inp->l);
					break;
				}
			}

			rt_comb_ifree( &intern, &rt_uniresource );
		}
	}

	/* place data in interp and free memory */
	 while (BU_LIST_WHILE(itnp,wdb_id_to_names,&headIdName.l)) {
		if (!sflag) {
			struct bu_vls vls;

			bu_vls_init(&vls);
			bu_vls_printf(&vls, "Region[s] with %s %d:\n",
				      isAir ? "air code" : "ident", itnp->id);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		}

		while (BU_LIST_WHILE(inp,wdb_id_names,&itnp->headName.l)) {
			if (sflag)
				Tcl_AppendElement(interp, bu_vls_addr(&inp->name));
			else
				Tcl_AppendResult(interp, "   ", bu_vls_addr(&inp->name),
						 "\n", (char *)NULL);

			BU_LIST_DEQUEUE(&inp->l);
			bu_vls_free(&inp->name);
			bu_free((genptr_t)inp, "which: inp");
		}

		BU_LIST_DEQUEUE(&itnp->l);
		bu_free((genptr_t)itnp, "which: itnp");
	}

	return TCL_OK;
}

/*
 * Change or return the database title.
 *
 * Usage:
 *        procname title [description]
 */
static int
wdb_title_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct bu_vls	title;
	int bad = 0;

	RT_CK_WDB(wdbp);
	RT_CK_DBI(wdbp->dbip);
	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 2 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_title");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc < 3) {
		Tcl_AppendResult(interp, wdbp->dbip->dbi_title, (char *)NULL);
		return TCL_OK;
	}

	bu_vls_init(&title);
	bu_vls_from_argv(&title, argc-2, argv+2);

	if (db_update_ident(wdbp->dbip, bu_vls_addr(&title), wdbp->dbip->dbi_base2local) < 0) {
		Tcl_AppendResult(interp, "Error: unable to change database title");
		bad = 1;
	}

	bu_vls_free(&title);
	return bad ? TCL_ERROR : TCL_OK;
}

/*
 *			W D B _ P R I N T _ N O D E
 *
 *  NON-PARALLEL due to rt_uniresource
 */
static void
wdb_print_node(wdbp, interp, dp, pathpos, prefix, cflag)
     struct rt_wdb		*wdbp;
     Tcl_Interp			*interp;
     register struct directory	*dp;
     int			pathpos;
     char			prefix;
     int			cflag;
{	
	register int			i;
	register struct directory	*nextdp;
	struct rt_db_internal		intern;
	struct rt_comb_internal		*comb;

	if (cflag && !(dp->d_flags & DIR_COMB))
		return;

	for (i=0; i<pathpos; i++) 
		Tcl_AppendResult(interp, "\t", (char *)NULL);

	if (prefix) {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "%c ", prefix);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
	}

	Tcl_AppendResult(interp, dp->d_namep, (char *)NULL);
	/* Output Comb and Region flags (-F?) */
	if(dp->d_flags & DIR_COMB)
		Tcl_AppendResult(interp, "/", (char *)NULL);
	if(dp->d_flags & DIR_REGION)
		Tcl_AppendResult(interp, "R", (char *)NULL);

	Tcl_AppendResult(interp, "\n", (char *)NULL);

	if(!(dp->d_flags & DIR_COMB))
		return;

	/*
	 *  This node is a combination (eg, a directory).
	 *  Process all the arcs (eg, directory members).
	 */

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database read error, aborting", (char *)NULL);
		return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	if (comb->tree){
		int node_count;
		int actual_count;
		struct rt_tree_array *rt_tree_array;

		if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
			db_non_union_push(comb->tree, &rt_uniresource);
			if (db_ck_v4gift_tree(comb->tree) < 0) {
				Tcl_AppendResult(interp, "Cannot flatten tree for listing", (char *)NULL);
				return;
			}
		}
		node_count = db_tree_nleaves(comb->tree);
		if (node_count > 0) {
			rt_tree_array = (struct rt_tree_array *)bu_calloc( node_count,
									   sizeof( struct rt_tree_array ), "tree list" );
			actual_count = (struct rt_tree_array *)db_flatten_tree(
				rt_tree_array, comb->tree, OP_UNION,
				1, &rt_uniresource ) - rt_tree_array;
			BU_ASSERT_PTR( actual_count, ==, node_count );
			comb->tree = TREE_NULL;
		} else {
			actual_count = 0;
			rt_tree_array = NULL;
		}

		for (i=0 ; i<actual_count ; i++) {
			char op;

			switch (rt_tree_array[i].tl_op) {
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

			if ((nextdp = db_lookup(wdbp->dbip, rt_tree_array[i].tl_tree->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL) {
				int j;
				struct bu_vls tmp_vls;
  			
				for (j=0; j<pathpos+1; j++) 
					Tcl_AppendResult(interp, "\t", (char *)NULL);

				bu_vls_init(&tmp_vls);
				bu_vls_printf(&tmp_vls, "%c ", op);
				Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
				bu_vls_free(&tmp_vls);

				Tcl_AppendResult(interp, rt_tree_array[i].tl_tree->tr_l.tl_name, "\n", (char *)NULL);
			} else
				wdb_print_node(wdbp, interp, nextdp, pathpos+1, op, cflag);
			db_free_tree( rt_tree_array[i].tl_tree, &rt_uniresource );
		}
		if(rt_tree_array) bu_free((char *)rt_tree_array, "printnode: rt_tree_array");
	}
	rt_db_free_internal(&intern, &rt_uniresource);
}

/*
 * Usage:
 *        procname tree object(s)
 */
static int
wdb_tree_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb			*wdbp = (struct rt_wdb *)clientData;
	register struct directory	*dp;
	register int			j;
	int				cflag = 0;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_tree");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argv[2][0] == '-' && argv[2][1] == 'c') {
		cflag = 1;
		--argc;
		++argv;
	}

	for (j = 2; j < argc; j++) {
		if (j > 2)
			Tcl_AppendResult(interp, "\n", (char *)NULL);
		if ((dp = db_lookup(wdbp->dbip, argv[j], LOOKUP_NOISY)) == DIR_NULL)
			continue;
		wdb_print_node(wdbp, interp, dp, 0, 0, cflag);
	}

	return TCL_OK;
}

/*
 *  			W D B _ C O L O R _ P U T R E C
 *  
 *  Used to create a database record and get it written out to a granule.
 *  In some cases, storage will need to be allocated.
 */
static void
wdb_color_putrec(mp, interp, dbip)
     register struct mater *mp;
     Tcl_Interp *interp;
     struct db_i *dbip;
{
	struct directory dir;
	union record rec;

	if (dbip->dbi_read_only)
		return;

	BU_ASSERT_LONG( dbip->dbi_version, ==, 4 );

	rec.md.md_id = ID_MATERIAL;
	rec.md.md_low = mp->mt_low;
	rec.md.md_hi = mp->mt_high;
	rec.md.md_r = mp->mt_r;
	rec.md.md_g = mp->mt_g;
	rec.md.md_b = mp->mt_b;

	/* Fake up a directory entry for db_* routines */
	RT_DIR_SET_NAMEP( &dir, "color_putrec" );
	dir.d_magic = RT_DIR_MAGIC;
	dir.d_flags = 0;

	if (mp->mt_daddr == MATER_NO_ADDR) {
		/* Need to allocate new database space */
		if (db_alloc(dbip, &dir, 1) < 0) {
			Tcl_AppendResult(interp,
					 "Database alloc error, aborting",
					 (char *)NULL);
			return;
		}
		mp->mt_daddr = dir.d_addr;
	} else {
		dir.d_addr = mp->mt_daddr;
		dir.d_len = 1;
	}

	if (db_put(dbip, &dir, &rec, 0, 1) < 0) {
		Tcl_AppendResult(interp,
				 "Database write error, aborting",
				 (char *)NULL);
		return;
	}
}

/*
 *  			W D B _ C O L O R _ Z A P R E C
 *  
 *  Used to release database resources occupied by a material record.
 */
static void
wdb_color_zaprec(mp, interp, dbip)
     register struct mater *mp;
     Tcl_Interp *interp;
     struct db_i *dbip;
{
	struct directory dir;

	BU_ASSERT_LONG( dbip->dbi_version, ==, 4 );

	if (dbip->dbi_read_only || mp->mt_daddr == MATER_NO_ADDR)
		return;

	dir.d_magic = RT_DIR_MAGIC;
	RT_DIR_SET_NAMEP( &dir, "color_zaprec" );
	dir.d_len = 1;
	dir.d_addr = mp->mt_daddr;
	dir.d_flags = 0;

	if (db_delete(dbip, &dir) < 0) {
		Tcl_AppendResult(interp,
				 "Database delete error, aborting",
				 (char *)NULL);
		return;
	}
	mp->mt_daddr = MATER_NO_ADDR;
}

/*
 * Usage:
 *        procname color low high r g b
 */
static int
wdb_color_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct mater *newp,*next_mater;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc != 7) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_color");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Delete all color records from the database */
	newp = rt_material_head;
	while (newp != MATER_NULL) {
		next_mater = newp->mt_forw;
		wdb_color_zaprec(newp, interp, wdbp->dbip);
		newp = next_mater;
	}

	/* construct the new color record */
	BU_GETSTRUCT(newp, mater);
	newp->mt_low = atoi(argv[2]);
	newp->mt_high = atoi(argv[3]);
	newp->mt_r = atoi(argv[4]);
	newp->mt_g = atoi(argv[5]);
	newp->mt_b = atoi(argv[6]);
	newp->mt_daddr = MATER_NO_ADDR;		/* not in database yet */

	/* Insert new color record in the in-memory list */
	rt_insert_color(newp);

	/* Write new color records for all colors in the list */
	newp = rt_material_head;
	while (newp != MATER_NULL) {
		next_mater = newp->mt_forw;
		wdb_color_putrec(newp, interp, wdbp->dbip);
		newp = next_mater;
	}

	return TCL_OK;
}

static void
wdb_pr_mater(mp, interp, ccp, clp)
     register struct mater	*mp;
     Tcl_Interp			*interp;
     int			*ccp;
     int			*clp;
{
	char buf[128];
	struct bu_vls vls;

	bu_vls_init(&vls);

	(void)sprintf(buf, "%5d..%d", mp->mt_low, mp->mt_high );
	wdb_vls_col_item(&vls, buf, ccp, clp);
	(void)sprintf( buf, "%3d,%3d,%3d", mp->mt_r, mp->mt_g, mp->mt_b);
	wdb_vls_col_item(&vls, buf, ccp, clp);
	wdb_vls_col_eol(&vls, ccp, clp);

	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);
}

/*
 * Usage:
 *        procname prcolor
 */
static int
wdb_prcolor_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	register struct mater *mp;
	int col_count = 0;
	int col_len = 0;

	if (argc != 2) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_prcolor");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (rt_material_head == MATER_NULL) {
		Tcl_AppendResult(interp, "none", (char *)NULL);
		return TCL_OK;
	}

	for (mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw)
		wdb_pr_mater(mp, interp, &col_count, &col_len);

	return TCL_OK;
}

/*
 * Usage:
 *        procname tol [abs|rel|norm|dist|perp [#]]
 *
 *  abs #	sets absolute tolerance.  # > 0.0
 *  rel #	sets relative tolerance.  0.0 < # < 1.0
 *  norm #	sets normal tolerance, in degrees.
 *  dist #	sets calculational distance tolerance
 *  perp #	sets calculational normal tolerance.
 *
 */
static int
wdb_tol_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct bu_vls vls;
	double	f;

	if (argc < 2 || 4 < argc){
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_tol");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* print all tolerance settings */
	if (argc == 2) {
		Tcl_AppendResult(interp, "Current tolerance settings are:\n", (char *)NULL);
		Tcl_AppendResult(interp, "Tesselation tolerances:\n", (char *)NULL );

		if (wdbp->wdb_ttol.abs > 0.0) {
			bu_vls_init(&vls);
			bu_vls_printf(&vls, "\tabs %g mm\n", wdbp->wdb_ttol.abs);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\tabs None\n", (char *)NULL);
		}

		if (wdbp->wdb_ttol.rel > 0.0) {
			bu_vls_init(&vls);
			bu_vls_printf(&vls, "\trel %g (%g%%)\n",
				      wdbp->wdb_ttol.rel, wdbp->wdb_ttol.rel * 100.0 );
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\trel None\n", (char *)NULL);
		}

		if (wdbp->wdb_ttol.norm > 0.0) {
			int	deg, min;
			double	sec;

			bu_vls_init(&vls);
			sec = wdbp->wdb_ttol.norm * bn_radtodeg;
			deg = (int)(sec);
			sec = (sec - (double)deg) * 60;
			min = (int)(sec);
			sec = (sec - (double)min) * 60;

			bu_vls_printf(&vls, "\tnorm %g degrees (%d deg %d min %g sec)\n",
				      wdbp->wdb_ttol.norm * bn_radtodeg, deg, min, sec);
			Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
			bu_vls_free(&vls);
		} else {
			Tcl_AppendResult(interp, "\tnorm None\n", (char *)NULL);
		}

		bu_vls_init(&vls);
		bu_vls_printf(&vls,"Calculational tolerances:\n");
		bu_vls_printf(&vls,
			      "\tdistance = %g mm\n\tperpendicularity = %g (cosine of %g degrees)",
			      wdbp->wdb_tol.dist, wdbp->wdb_tol.perp,
			      acos(wdbp->wdb_tol.perp)*bn_radtodeg);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_OK;
	}

	/* get the specified tolerance */
	if (argc == 3) {
		int status = TCL_OK;

		bu_vls_init(&vls);

		switch (argv[2][0]) {
		case 'a':
			if (wdbp->wdb_ttol.abs > 0.0)
				bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.abs);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'r':
			if (wdbp->wdb_ttol.rel > 0.0)
				bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.rel);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'n':
			if (wdbp->wdb_ttol.norm > 0.0)
				bu_vls_printf(&vls, "%g", wdbp->wdb_ttol.norm);
			else
				bu_vls_printf(&vls, "None");
			break;
		case 'd':
			bu_vls_printf(&vls, "%g", wdbp->wdb_tol.dist);
			break;
		case 'p':
			bu_vls_printf(&vls, "%g", wdbp->wdb_tol.perp);
			break;
		default:
			bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[2]);
			status = TCL_ERROR;
			break;
		}

		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return status;
	}

	/* set the specified tolerance */
	if (sscanf(argv[3], "%lf", &f) != 1) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "bad tolerance - %s", argv[3]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	switch (argv[2][0]) {
	case 'a':
		/* Absolute tol */
		if (f <= 0.0)
			wdbp->wdb_ttol.abs = 0.0;
		else
			wdbp->wdb_ttol.abs = f;
		break;
	case 'r':
		if (f < 0.0 || f >= 1.0) {
			   Tcl_AppendResult(interp,
					    "relative tolerance must be between 0 and 1, not changed\n",
					    (char *)NULL);
			   return TCL_ERROR;
		}
		/* Note that a value of 0.0 will disable relative tolerance */
		wdbp->wdb_ttol.rel = f;
		break;
	case 'n':
		/* Normal tolerance, in degrees */
		if (f < 0.0 || f > 90.0) {
			Tcl_AppendResult(interp,
					 "Normal tolerance must be in positive degrees, < 90.0\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		/* Note that a value of 0.0 or 360.0 will disable this tol */
		wdbp->wdb_ttol.norm = f * bn_degtorad;
		break;
	case 'd':
		/* Calculational distance tolerance */
		if (f < 0.0) {
			Tcl_AppendResult(interp,
					 "Calculational distance tolerance must be positive\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		wdbp->wdb_tol.dist = f;
		wdbp->wdb_tol.dist_sq = wdbp->wdb_tol.dist * wdbp->wdb_tol.dist;
		break;
	case 'p':
		/* Calculational perpendicularity tolerance */
		if (f < 0.0 || f > 1.0) {
			Tcl_AppendResult(interp,
					 "Calculational perpendicular tolerance must be from 0 to 1\n",
					 (char *)NULL);
			return TCL_ERROR;
		}
		wdbp->wdb_tol.perp = f;
		wdbp->wdb_tol.para = 1.0 - f;
		break;
	default:
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "unrecognized tolerance type - %s", argv[2]);
		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);

		return TCL_ERROR;
	}

	return TCL_OK;
}

/* structure to hold all solids that have been pushed. */
struct wdb_push_id {
	long	magic;
	struct wdb_push_id *forw, *back;
	struct directory *pi_dir;
	mat_t	pi_mat;
};

#define WDB_MAGIC_PUSH_ID	0x50495323
#define FOR_ALL_WDB_PUSH_SOLIDS(_p,_phead) \
	for(_p=_phead.forw; _p!=&_phead; _p=_p->forw)
struct wdb_push_data {
	Tcl_Interp		*interp;
	struct wdb_push_id	pi_head;
	int			push_error;
};

/*
 *		P U S H _ L E A F
 *
 * This routine must be prepared to run in parallel.
 *
 * This routine is called once for eas leaf (solid) that is to
 * be pushed.  All it does is build at push_id linked list.  The 
 * linked list could be handled by bu_list macros but it is simple
 * enough to do hear with out them.
 */
static union tree *
wdb_push_leaf(tsp, pathp, ip, client_data)
     struct db_tree_state	*tsp;
     struct db_full_path	*pathp;
     struct rt_db_internal	*ip;
     genptr_t		client_data;
{
	union tree	*curtree;
	struct directory *dp;
	register struct wdb_push_id *pip;
	struct wdb_push_data *wpdp = (struct wdb_push_data *)client_data;

	RT_CK_TESS_TOL(tsp->ts_ttol);
	BN_CK_TOL(tsp->ts_tol);
	RT_CK_RESOURCE(tsp->ts_resp);

	dp = pathp->fp_names[pathp->fp_len-1];

	if (rt_g.debug&DEBUG_TREEWALK) {
		char *sofar = db_path_to_string(pathp);

		Tcl_AppendResult(wpdp->interp, "wdb_push_leaf(",
				ip->idb_meth->ft_name,
				 ") path='", sofar, "'\n", (char *)NULL);
		bu_free((genptr_t)sofar, "path string");
	}
/*
 * XXX - This will work but is not the best method.  dp->d_uses tells us
 * if this solid (leaf) has been seen before.  If it hasn't just add
 * it to the list.  If it has, search the list to see if the matricies
 * match and do the "right" thing.
 *
 * (There is a question as to whether dp->d_uses is reset to zero
 *  for each tree walk.  If it is not, then d_uses is NOT a safe
 *  way to check and this method will always work.)
 */
	bu_semaphore_acquire(RT_SEM_WORKER);
	FOR_ALL_WDB_PUSH_SOLIDS(pip,wpdp->pi_head) {
		if (pip->pi_dir == dp ) {
			if (!bn_mat_is_equal(pip->pi_mat,
					     tsp->ts_mat, tsp->ts_tol)) {
				char *sofar = db_path_to_string(pathp);

				Tcl_AppendResult(wpdp->interp, "wdb_push_leaf: matrix mismatch between '", sofar,
						 "' and prior reference.\n", (char *)NULL);
				bu_free((genptr_t)sofar, "path string");
				wpdp->push_error = 1;
			}

			bu_semaphore_release(RT_SEM_WORKER);
			RT_GET_TREE(curtree, tsp->ts_resp);
			curtree->magic = RT_TREE_MAGIC;
			curtree->tr_op = OP_NOP;
			return curtree;
		}
	}
/*
 * This is the first time we have seen this solid.
 */
	pip = (struct wdb_push_id *) bu_malloc(sizeof(struct wdb_push_id), "Push ident");
	pip->magic = WDB_MAGIC_PUSH_ID;
	pip->pi_dir = dp;
	bn_mat_copy(pip->pi_mat, tsp->ts_mat);
	pip->back = wpdp->pi_head.back;
	wpdp->pi_head.back = pip;
	pip->forw = &wpdp->pi_head;
	pip->back->forw = pip;
	bu_semaphore_release(RT_SEM_WORKER);
	RT_GET_TREE( curtree, tsp->ts_resp );
	curtree->magic = RT_TREE_MAGIC;
	curtree->tr_op = OP_NOP;
	return curtree;
}
/*
 * A null routine that does nothing.
 */
static union tree *
wdb_push_region_end( tsp, pathp, curtree, client_data)
     register struct db_tree_state *tsp;
     struct db_full_path	*pathp;
     union tree		*curtree;
     genptr_t		client_data;
{
	return curtree;
}

/*
 * The push command is used to move matrices from combinations 
 * down to the solids. At some point, it is worth while thinking
 * about adding a limit to have the push go only N levels down.
 *
 * the -d flag turns on the treewalker debugging output.
 * the -P flag allows for multi-processor tree walking (not useful)
 *
 * Usage:
 *        procname push object(s)
 */
static int
wdb_push_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb 		*wdbp = (struct rt_wdb *)clientData;
	struct wdb_push_data	*wpdp;
	struct wdb_push_id	*pip;
	struct rt_db_internal	es_int;
	int			i;
	int			ncpu;
	int			c;
	int			old_debug;
	int			push_error;
	extern 	int		bu_optind;
	extern	char		*bu_optarg;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_push");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	RT_CHECK_DBI(wdbp->dbip);

	BU_GETSTRUCT(wpdp,wdb_push_data);
	wpdp->interp = interp;
	wpdp->push_error = 0;
	wpdp->pi_head.magic = WDB_MAGIC_PUSH_ID;
	wpdp->pi_head.forw = wpdp->pi_head.back = &wpdp->pi_head;
	wpdp->pi_head.pi_dir = (struct directory *) 0;

	old_debug = rt_g.debug;

	/* Initial values for options, must be reset each time */
	ncpu = 1;

	/* Parse options */
	bu_optind = 2;	/* re-init bu_getopt() */
	while ((c=bu_getopt(argc, argv, "P:d")) != EOF) {
		switch (c) {
		case 'P':
			ncpu = atoi(bu_optarg);
			if (ncpu<1) ncpu = 1;
			break;
		case 'd':
			rt_g.debug |= DEBUG_TREEWALK;
			break;
		case '?':
		default:
		  Tcl_AppendResult(interp, "push: usage push [-P processors] [-d] root [root2 ...]\n", (char *)NULL);
			break;
		}
	}

	argc -= bu_optind;
	argv += bu_optind;

	/*
	 * build a linked list of solids with the correct
	 * matrix to apply to each solid.  This will also
	 * check to make sure that a solid is not pushed in two
	 * different directions at the same time.
	 */
	i = db_walk_tree(wdbp->dbip, argc, (CONST char **)argv,
			 ncpu,
			 &wdbp->wdb_initial_tree_state,
			 0,				/* take all regions */
			 wdb_push_region_end,
			 wdb_push_leaf, (genptr_t)wpdp);

	/*
	 * If there was any error, then just free up the solid
	 * list we just built.
	 */
	if (i < 0 || wpdp->push_error) {
		while (wpdp->pi_head.forw != &wpdp->pi_head) {
			pip = wpdp->pi_head.forw;
			pip->forw->back = pip->back;
			pip->back->forw = pip->forw;
			bu_free((genptr_t)pip, "Push ident");
		}
		rt_g.debug = old_debug;
		bu_free((genptr_t)wpdp, "wdb_push_tcl: wpdp");
		Tcl_AppendResult(interp,
				 "push:\tdb_walk_tree failed or there was a solid moving\n\tin two or more directions",
				 (char *)NULL);
		return TCL_ERROR;
	}
/*
 * We've built the push solid list, now all we need to do is apply
 * the matrix we've stored for each solid.
 */
	FOR_ALL_WDB_PUSH_SOLIDS(pip,wpdp->pi_head) {
		if (rt_db_get_internal(&es_int, pip->pi_dir, wdbp->dbip, pip->pi_mat, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "f_push: Read error fetching '",
				   pip->pi_dir->d_namep, "'\n", (char *)NULL);
			wpdp->push_error = -1;
			continue;
		}
		RT_CK_DB_INTERNAL(&es_int);

		if (rt_db_put_internal(pip->pi_dir, wdbp->dbip, &es_int, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "push(", pip->pi_dir->d_namep,
					 "): solid export failure\n", (char *)NULL);
		}
		rt_db_free_internal(&es_int, &rt_uniresource);
	}

	/*
	 * Now use the wdb_identitize() tree walker to turn all the
	 * matricies in a combination to the identity matrix.
	 * It would be nice to use db_tree_walker() but the tree
	 * walker does not give us all combinations, just regions.
	 * This would work if we just processed all matricies backwards
	 * from the leaf (solid) towards the root, but all in all it
	 * seems that this is a better method.
	 */

	while (argc > 0) {
		struct directory *db;
		db = db_lookup(wdbp->dbip, *argv++, 0);
		if (db)
			wdb_identitize(db, wdbp->dbip, interp);
		--argc;
	}

	/*
	 * Free up the solid table we built.
	 */
	while (wpdp->pi_head.forw != &wpdp->pi_head) {
		pip = wpdp->pi_head.forw;
		pip->forw->back = pip->back;
		pip->back->forw = pip->forw;
		bu_free((genptr_t)pip, "Push ident");
	}

	rt_g.debug = old_debug;
	push_error = wpdp->push_error;
	bu_free((genptr_t)wpdp, "wdb_push_tcl: wpdp");

	return push_error ? TCL_ERROR : TCL_OK;
}

/*
 * Usage:
 *        procname whatid object
 */
static int
wdb_whatid_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	struct bu_vls vls;

	if (argc != 3) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_whatid");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((dp=db_lookup(wdbp->dbip, argv[2], LOOKUP_NOISY )) == DIR_NULL )
		return TCL_ERROR;

	if (!(dp->d_flags & DIR_REGION)) {
		Tcl_AppendResult(interp, argv[2], " is not a region", (char *)NULL );
		return TCL_ERROR;
	}

	if (rt_db_get_internal(&intern, dp, wdbp->dbip, (fastf_t *)NULL, &rt_uniresource) < 0)
		return TCL_ERROR;
	comb = (struct rt_comb_internal *)intern.idb_ptr;

	bu_vls_init(&vls);
	bu_vls_printf(&vls, "%d", comb->region_id);
	rt_comb_ifree(&intern, &rt_uniresource);
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

struct wdb_node_data {
	FILE	     *fp;
	Tcl_Interp   *interp;
};

/*
 *			W D B _ N O D E _ W R I T E
 *
 *  Support for the 'keep' method.
 *  Write each node encountered exactly once.
 */
void
wdb_node_write(dbip, dp, client_data)
     struct db_i		*dbip;
     register struct directory	*dp;
     genptr_t			client_data;
{
	struct bu_external	ext;
	struct wdb_node_data *wndp = (struct wdb_node_data *)client_data;
	

	if (dp->d_nref++ > 0)
		return;		/* already written */

	/* Use db_get_external() and db_fwrite_external().  Faster. */
	if( db_get_external( &ext, dp, dbip ) < 0 )  {
		Tcl_AppendResult(wndp->interp, "Input database read error on ",
			dp->d_namep, ", aborting", (char *)NULL);
		return;
	}

	if( db_fwrite_external( wndp->fp, dp->d_namep, &ext) < 0 )  {
		Tcl_AppendResult(wndp->interp,
				 "Output database write error on ",
			dp->d_namep, ", aborting", (char *)NULL);
	}
	bu_free_external(&ext);
}

/*
 * Usage:
 *        procname keep file object(s)
 */
static int
wdb_keep_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	FILE			*keepfp;
	register struct directory *dp;
	struct bu_vls		title;
	register int		i;
	struct wdb_node_data	wnd;

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_keep");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* First, clear any existing counts */
	for (i = 0; i < RT_DBNHASH; i++) {
		for (dp = wdbp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
			dp->d_nref = 0;
	}

	/* Alert user if named file already exists */
	if ((keepfp = fopen(argv[2], "r")) != NULL) {
		Tcl_AppendResult(interp, "keep:  appending to '", argv[2],
				 "'\n", (char *)NULL);
		fclose(keepfp);
	}

	if ((keepfp = fopen(argv[2], "a")) == NULL) {
		perror(argv[2]);
		return TCL_ERROR;
	}
	
	/* All databases must start with an ident record */
	bu_vls_init(&title);
	bu_vls_strcat(&title, "Parts of: ");
	bu_vls_strcat(&title, wdbp->dbip->dbi_title);

	/* use same editing preference */
	if( db_fwrite_ident( keepfp, bu_vls_addr(&title), wdbp->dbip->dbi_local2base ) )  {
		perror("fwrite");
		Tcl_AppendResult(interp, "db_fwrite_ident(", argv[2],
			") failed\n", (char *)NULL);
		fclose(keepfp);
		bu_vls_free(&title);
		return TCL_ERROR;
	}

	wnd.fp = keepfp;
	wnd.interp = interp;

	for (i = 3; i < argc; i++) {
		if ((dp = db_lookup(wdbp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
			continue;
		db_functree(wdbp->dbip, dp, wdb_node_write, wdb_node_write, wdbp->wdb_resp, (genptr_t)&wnd);
	}

	fclose(keepfp);
	bu_vls_free(&title);

	return TCL_OK;
}

/*
 * Usage:
 *        procname cat object(s)
 */
static int
wdb_cat_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	register int arg;
	struct bu_vls str;

	if (argc < 3 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_cat");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_init(&str);
	for (arg = 2; arg < argc; arg++) {
		if ((dp = db_lookup(wdbp->dbip, argv[arg], LOOKUP_NOISY)) == DIR_NULL)
			continue;

		bu_vls_trunc(&str, 0);
		wdb_do_list(wdbp->dbip, interp, &str, dp, 0);	/* non-verbose */
		Tcl_AppendResult(interp, bu_vls_addr(&str), "\n", (char *)NULL);
	}
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 * Add instance of obj to comb.
 *
 * Usage:
 *        procname i obj comb [op]
 */
static int
wdb_instance_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
	register struct directory *dp;
	char oper;

	if (wdbp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!", (char *)NULL);
		return TCL_ERROR;
	}

	if (argc < 4 || 5 < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_instance");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if ((dp = db_lookup(wdbp->dbip,  argv[2], LOOKUP_NOISY)) == DIR_NULL)
		return TCL_ERROR;

	oper = WMOP_UNION;
	if (argc == 5)
		oper = argv[4][0];

	if (oper != WMOP_UNION &&
	    oper != WMOP_SUBTRACT &&
	    oper != WMOP_INTERSECT) {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "bad operation: %c\n", oper);
		Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);
		return TCL_ERROR;
	}

	if (wdb_combadd(interp, wdbp->dbip, dp, argv[3], 0, oper, 0, 0, wdbp) == DIR_NULL)
		return TCL_ERROR;

	return TCL_OK;
}

/*
 * Attach/detach observers to/from list.
 *
 * Usage:
 *	  procname observer cmd [args]
 *
 */
static int
wdb_observer_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;

	if (argc < 3) {
		struct bu_vls vls;

		/* return help message */
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_observer");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	return bu_cmd((ClientData)&wdbp->wdb_observers,
		      interp, argc - 2, argv + 2, bu_observer_cmds, 0);
}

/*
 *	Build an RPP bounding box for the list of objects
 *	and/or paths passed to this routine
 *
 *	Usage:
 *		dbobjname make_bb bbname obj(s)
 */
static int
wdb_make_bb_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int	argc;
     char	**argv;
{
	struct rt_i		*rtip;
	int			i;
	point_t			rpp_min,rpp_max;
	struct db_full_path	path;
	struct directory	*dp;
	struct rt_arb_internal	*arb;
	struct rt_db_internal	new_intern;
	struct region		*regp;
	char			*new_name;
	struct rt_wdb		*wdbp = (struct rt_wdb *)clientData;

	if (argc < 4 || MAXARGS < argc) {
		struct bu_vls vls;

		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_make_bb");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Since arguments may be paths, make sure first argument isn't */
	if (strchr( argv[2], '/')) {
		Tcl_AppendResult(interp, "Do not use '/' in solid names: ", argv[2], "\n", (char *)NULL);
		return TCL_ERROR;
	}

	new_name = argv[2];
	if (db_lookup(wdbp->dbip, new_name, LOOKUP_QUIET) != DIR_NULL) {
		Tcl_AppendResult(interp, new_name, " already exists\n", (char *)NULL);
		return TCL_ERROR;
	}

	/* Make a new rt_i instance from the existing db_i sructure */
	if ((rtip=rt_new_rti(wdbp->dbip)) == RTI_NULL) {
		Tcl_AppendResult(interp, "rt_new_rti failure for ", wdbp->dbip->dbi_filename,
				 "\n", (char *)NULL);
		return TCL_ERROR;
	}

	/* Get trees for list of objects/paths */
	for (i = 3 ; i < argc ; i++) {
		int gottree;

		/* Get full_path structure for argument */
		db_full_path_init(&path);
		if (db_string_to_path(&path,  rtip->rti_dbip, argv[i])) {
			Tcl_AppendResult(interp, "db_string_to_path failed for ",
					 argv[i], "\n", (char *)NULL );
			rt_clean(rtip);
			bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
			return TCL_ERROR;
		}

		/* check if we alerady got this tree */
		gottree = 0;
		for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
			struct db_full_path tmp_path;

			db_full_path_init(&tmp_path);
			if (db_string_to_path(&tmp_path, rtip->rti_dbip, regp->reg_name)) {
				Tcl_AppendResult(interp, "db_string_to_path failed for ",
						 regp->reg_name, "\n", (char *)NULL);
				rt_clean(rtip);
				bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
				return TCL_ERROR;
			}
			if (path.fp_names[0] == tmp_path.fp_names[0])
				gottree = 1;
			db_free_full_path(&tmp_path);
			if (gottree)
				break;
		}

		/* if we don't already have it, get it */
		if (!gottree && rt_gettree(rtip, path.fp_names[0]->d_namep)) {
			Tcl_AppendResult(interp, "rt_gettree failed for ",
					 argv[i], "\n", (char *)NULL );
			rt_clean(rtip);
			bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
			return TCL_ERROR;
		}
		db_free_full_path(&path);
	}

	/* prep calculates bounding boxes of solids */
	rt_prep(rtip);

	/* initialize RPP bounds */
	VSETALL(rpp_min, MAX_FASTF);
	VREVERSE(rpp_max, rpp_min);
	for (i = 3 ; i < argc ; i++) {
		vect_t reg_min, reg_max;
		struct region *regp;
		CONST char *reg_name;

		/* check if input name is a region */
		for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
			reg_name = regp->reg_name;
			if (*argv[i] != '/' && *reg_name == '/')
				reg_name++;

			if (!strcmp( reg_name, argv[i]))
				goto found;
				
		}
		goto not_found;

		if (regp != REGION_NULL) {
found:
			/* input name was a region  */
			if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
				Tcl_AppendResult(interp, "rt_bound_tree failed for ",
						 regp->reg_name, "\n", (char *)NULL);
				rt_clean(rtip);
				bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
				return TCL_ERROR;
			}
			VMINMAX(rpp_min, rpp_max, reg_min);
			VMINMAX(rpp_min, rpp_max, reg_max);
		} else {
			int name_len;
not_found:

			/* input name may be a group, need to check all regions under
			 * that group
			 */
			name_len = strlen( argv[i] );
			for (BU_LIST_FOR( regp, region, &(rtip->HeadRegion))) {
				reg_name = regp->reg_name;
				if (*argv[i] != '/' && *reg_name == '/')
					reg_name++;

				if (strncmp(argv[i], reg_name, name_len))
					continue;

				/* This is part of the group */
				if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
					Tcl_AppendResult(interp, "rt_bound_tree failed for ",
							 regp->reg_name, "\n", (char *)NULL);
					rt_clean(rtip);
					bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
					return TCL_ERROR;
				}
				VMINMAX(rpp_min, rpp_max, reg_min);
				VMINMAX(rpp_min, rpp_max, reg_max);
			}
		}
	}

	/* build bounding RPP */
	arb = (struct rt_arb_internal *)bu_malloc(sizeof(struct rt_arb_internal), "arb");
	VMOVE(arb->pt[0], rpp_min);
	VSET(arb->pt[1], rpp_min[X], rpp_min[Y], rpp_max[Z]);
	VSET(arb->pt[2], rpp_min[X], rpp_max[Y], rpp_max[Z]);
	VSET(arb->pt[3], rpp_min[X], rpp_max[Y], rpp_min[Z]);
	VSET(arb->pt[4], rpp_max[X], rpp_min[Y], rpp_min[Z]);
	VSET(arb->pt[5], rpp_max[X], rpp_min[Y], rpp_max[Z]);
	VMOVE(arb->pt[6], rpp_max);
	VSET(arb->pt[7], rpp_max[X], rpp_max[Y], rpp_min[Z]);
	arb->magic = RT_ARB_INTERNAL_MAGIC;

	/* set up internal structure */
	RT_INIT_DB_INTERNAL(&new_intern);
	new_intern.idb_type = ID_ARB8;
	new_intern.idb_meth = &rt_functab[ID_ARB8];
	new_intern.idb_ptr = (genptr_t)arb;

	if ((dp=db_diradd( wdbp->dbip, new_name, -1L, 0, DIR_SOLID, NULL)) == DIR_NULL) {
		Tcl_AppendResult(interp, "Cannot add ", new_name, " to directory\n", (char *)NULL);
		return TCL_ERROR;
	}

	if (rt_db_put_internal(dp, wdbp->dbip, &new_intern, wdbp->wdb_resp) < 0) {
		rt_db_free_internal(&new_intern, wdbp->wdb_resp);
		Tcl_AppendResult(interp, "Database write error, aborting.\n", (char *)NULL);
		return TCL_ERROR;
	}

	rt_clean(rtip);
	bu_free((genptr_t)rtip, "wdb_make_bb_tcl: rtip");
	return TCL_OK;
}

/*
 *
 * Generate an identifier that is guaranteed not to be the name
 * of any object currently in the database.
 *
 * Usage:
 *	dbobjname make_name (template | -s [num])
 *
 */
static int
wdb_make_name_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;

{
	struct bu_vls	obj_name;
	char		*cp, *tp;
	static int	i = 0;
	int		len;
	struct rt_wdb	*wdbp = (struct rt_wdb *)clientData;

	switch (argc) {
	case 3:
		if (strcmp(argv[2], "-s") != 0)
			break;
		else {
			i = 0;
			return TCL_OK;
		}
	case 4:
		{
			int	new_i;

			if ((strcmp(argv[2], "-s") == 0)
			    && (sscanf(argv[3], "%d", &new_i) == 1)) {
				i = new_i;
				return TCL_OK;
			}
		}
	default:
		{
			struct bu_vls	vls;

			bu_vls_init(&vls);
			bu_vls_printf(&vls, "helplib make_name");
			Tcl_Eval(interp, bu_vls_addr(&vls));
			bu_vls_free(&vls);
			return TCL_ERROR;
		}
	}

	bu_vls_init(&obj_name);
	for (cp = argv[2], len = 0; *cp != '\0'; ++cp, ++len) {
		if (*cp == '@') {
			if (*(cp + 1) == '@')
				++cp;
			else
				break;
		}
		bu_vls_putc(&obj_name, *cp);
	}
	bu_vls_putc(&obj_name, '\0');
	tp = (*cp == '\0') ? "" : cp + 1;

	do {
		bu_vls_trunc(&obj_name, len);
		bu_vls_printf(&obj_name, "%d", i++);
		bu_vls_strcat(&obj_name, tp);
	}
	while (db_lookup(wdbp->dbip, bu_vls_addr(&obj_name), LOOKUP_QUIET) != DIR_NULL);
	Tcl_AppendResult(interp, bu_vls_addr(&obj_name), (char *) NULL);
	bu_vls_free(&obj_name);
	return TCL_OK;
}

/*
 * Set/get the database units. 
 *
 * Usage:
 *        dbobjname units [str]
 */
static int
wdb_units_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct rt_wdb	*wdbp = (struct rt_wdb *)clientData;
	double		loc2mm;
	int		new_unit = 0;
	struct bu_vls 	vls;
	CONST char	*str;
	int 		sflag = 0;

	bu_vls_init(&vls);
	if (argc < 2 || 3 < argc) {
		bu_vls_printf(&vls, "helplib wdb_units");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	if (argc == 3 && strcmp(argv[2], "-s") == 0) {
		--argc;
		++argv;

		sflag = 1;
	}

	if (argc < 3) {
		str = bu_units_string(wdbp->dbip->dbi_local2base);
		if (!str) str = "Unknown_unit";

		if (sflag)
			bu_vls_printf(&vls, "%s", str);
		else
			bu_vls_printf(&vls, "You are editing in '%s'.  1 %s = %g mm \n",
				      str, str, wdbp->dbip->dbi_local2base );

		Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
		bu_vls_free(&vls);
		return TCL_OK;
	}

	/* Allow inputs of the form "25cm" or "3ft" */
	if ((loc2mm = bu_mm_value(argv[2]) ) <= 0) {
		Tcl_AppendResult(interp, argv[2], ": unrecognized unit\n",
				 "valid units: <um|mm|cm|m|km|in|ft|yd|mi>\n", (char *)NULL);
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* See if this is a known v4 database unit */
	if ((new_unit = db_v4_get_units_code(bu_units_string(loc2mm))) >= 0) {
		/* One of the recognized db.h units */
		/* change database to remember the new local unit */
		if (wdbp->dbip->dbi_read_only ||
		    db_update_ident(wdbp->dbip, wdbp->dbip->dbi_title, new_unit) < 0)
			Tcl_AppendResult(interp,
					 "Warning: unable to stash working units into database\n",
					 (char *)NULL);
	} else {
		/*
		 *  Can't stash requested units into the database for next session,
		 *  but there is no problem with the user editing in these units.
		 */
		Tcl_AppendResult(interp, "\
Due to a database restriction in the current format of .g files,\n\
this choice of units will not be remembered on your next editing session.\n", (char *)NULL);
	}
	wdbp->dbip->dbi_local2base = loc2mm;
	wdbp->dbip->dbi_base2local = 1.0 / loc2mm;

	str = bu_units_string(wdbp->dbip->dbi_local2base);
	if (!str) str = "Unknown_unit";
	bu_vls_printf(&vls, "You are now editing in '%s'.  1 %s = %g mm \n",
		      str, str, wdbp->dbip->dbi_local2base );
	Tcl_AppendResult(interp, bu_vls_addr(&vls), (char *)NULL);
	bu_vls_free(&vls);

	return TCL_OK;
}

#if 0
/*
 * Usage:
 *        procname 
 */
static int
wdb__tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int	argc;
     char	**argv;
{
	struct rt_wdb *wdbp = (struct rt_wdb *)clientData;
}
#endif

/****************** utility routines ********************/

/*
 *			D B O _ C M P D I R N A M E
 *
 * Given two pointers to pointers to directory entries, do a string compare
 * on the respective names and return that value.
 *  This routine was lifted from mged/columns.c.
 */
int
wdb_cmpdirname(a, b)
     CONST genptr_t a;
     CONST genptr_t b;
{
	register struct directory **dp1, **dp2;

	dp1 = (struct directory **)a;
	dp2 = (struct directory **)b;
	return( strcmp( (*dp1)->d_namep, (*dp2)->d_namep));
}

#define RT_TERMINAL_WIDTH 80
#define RT_COLUMNS ((RT_TERMINAL_WIDTH + RT_NAMESIZE - 1) / RT_NAMESIZE)

/*
 *			V L S _ C O L _ I T E M
 */
void
wdb_vls_col_item(str, cp, ccp, clp)
     struct bu_vls	*str;
     register char	*cp;
     int		*ccp;   /* column count pointer */
     int		*clp;   /* column length pointer */
{
	/* Output newline if last column printed. */
	if (*ccp >= RT_COLUMNS || (*clp+RT_NAMESIZE-1) >= RT_TERMINAL_WIDTH) {
		/* line now full */
		bu_vls_putc(str, '\n');
		*ccp = 0;
	} else if (*ccp != 0) {
		/* Space over before starting new column */
		do {
			bu_vls_putc(str, ' ');
			++*clp;
		}  while ((*clp % RT_NAMESIZE) != 0);
	}
	/* Output string and save length for next tab. */
	*clp = 0;
	while (*cp != '\0') {
		bu_vls_putc(str, *cp);
		++cp;
		++*clp;
	}
	++*ccp;
}

/*
 */
void
wdb_vls_col_eol(str, ccp, clp)
     struct bu_vls	*str;
     int		*ccp;
     int		*clp;
{
	if (*ccp != 0)		/* partial line */
		bu_vls_putc(str, '\n');
	*ccp = 0;
	*clp = 0;
}

/*
 *			D B O _ V L S _ C O L _ P R 4 V
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list in column order over four columns.
 *  This routine was lifted from mged/columns.c.
 */
void
wdb_vls_col_pr4v(vls, list_of_names, num_in_list)
     struct bu_vls *vls;
     struct directory **list_of_names;
     int num_in_list;
{
	int lines, i, j, namelen, this_one;

	qsort((genptr_t)list_of_names,
	      (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	      (int (*)())wdb_cmpdirname);

	/*
	 * For the number of (full and partial) lines that will be needed,
	 * print in vertical format.
	 */
	lines = (num_in_list + 3) / 4;
	for (i=0; i < lines; i++) {
		for (j=0; j < 4; j++) {
			this_one = j * lines + i;
			/* Restrict the print to 16 chars per spec. */
			bu_vls_printf(vls,  "%.16s", list_of_names[this_one]->d_namep);
			namelen = strlen(list_of_names[this_one]->d_namep);
			if (namelen > 16)
				namelen = 16;
			/*
			 * Region and ident checks here....  Since the code
			 * has been modified to push and sort on pointers,
			 * the printing of the region and ident flags must
			 * be delayed until now.  There is no way to make the
			 * decision on where to place them before now.
			 */
			if (list_of_names[this_one]->d_flags & DIR_COMB) {
				bu_vls_putc(vls, '/');
				namelen++;
			}
			if (list_of_names[this_one]->d_flags & DIR_REGION) {
				bu_vls_putc(vls, 'R');
				namelen++;
			}
			/*
			 * Size check (partial lines), and line termination.
			 * Note that this will catch the end of the lines
			 * that are full too.
			 */
			if (this_one + lines >= num_in_list) {
				bu_vls_putc(vls, '\n');
				break;
			} else {
				/*
				 * Pad to next boundary as there will be
				 * another entry to the right of this one. 
				 */
				while (namelen++ < 20)
					bu_vls_putc(vls, ' ');
			}
		}
	}
}

/*
 *			D B O _ V L S _ L I N E _ D P P
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list on the same line.
 *  This routine was lifted from mged/columns.c.
 */
void
wdb_vls_line_dpp(vls, list_of_names, num_in_list, aflag, cflag, rflag, sflag)
     struct bu_vls *vls;
     struct directory **list_of_names;
     int num_in_list;
     int aflag;	/* print all objects */
     int cflag;	/* print combinations */
     int rflag;	/* print regions */
     int sflag;	/* print solids */
{
	int i;
	int isComb, isRegion;
	int isSolid;

	qsort( (genptr_t)list_of_names,
	       (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	       (int (*)())wdb_cmpdirname);

	/*
	 * i - tracks the list item
	 */
	for (i=0; i < num_in_list; ++i) {
		if (list_of_names[i]->d_flags & DIR_COMB) {
			isComb = 1;
			isSolid = 0;

			if (list_of_names[i]->d_flags & DIR_REGION)
				isRegion = 1;
			else
				isRegion = 0;
		} else {
			isComb = isRegion = 0;
			isSolid = 1;
		}

		/* print list item i */
		if (aflag ||
		    (!cflag && !rflag && !sflag) ||
		    (cflag && isComb) ||
		    (rflag && isRegion) ||
		    (sflag && isSolid)) {
			bu_vls_printf(vls,  "%s ", list_of_names[i]->d_namep);
		}
	}
}

/*
 *			D B O _ G E T S P A C E
 *
 * This routine walks through the directory entry list and mallocs enough
 * space for pointers to hold:
 *  a) all of the entries if called with an argument of 0, or
 *  b) the number of entries specified by the argument if > 0.
 *  This routine was lifted from mged/dir.c.
 */
struct directory **
wdb_getspace(dbip, num_entries)
     struct db_i *dbip;
     register int num_entries;
{
	register struct directory **dir_basep;

	if (num_entries < 0) {
		bu_log("wdb_getspace: was passed %d, used 0\n",
		       num_entries);
		num_entries = 0;
	}

	if (num_entries == 0)  num_entries = db_get_directory_size(dbip);

	/* Allocate and cast num_entries worth of pointers */
	dir_basep = (struct directory **) bu_malloc((num_entries+1) * sizeof(struct directory *),
						    "wdb_getspace *dir[]" );
	return(dir_basep);
}

/*
 *			W D B _ D O _ L I S T
 */
void
wdb_do_list(dbip, interp, outstrp, dp, verbose)
     struct db_i *dbip;
     Tcl_Interp *interp;
     struct bu_vls *outstrp;
     register struct directory *dp;
     int verbose;
{
	int			id;
	struct rt_db_internal	intern;

	RT_CK_DBI(dbip);

	if ((id = rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource)) < 0) {
		Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
				 ") failure\n", (char *)NULL);
		return;
	}

	bu_vls_printf(outstrp, "%s:  ", dp->d_namep);

	if (rt_functab[id].ft_describe(outstrp, &intern,
				       verbose, dbip->dbi_base2local, &rt_uniresource) < 0)
		Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_db_free_internal(&intern, &rt_uniresource);
}

/*
 *			C O M B A D D
 *
 * Add an instance of object 'dp' to combination 'name'.
 * If the combination does not exist, it is created.
 * region_flag is 1 (region), or 0 (group).
 *
 *  Preserves the GIFT semantics.
 */
struct directory *
wdb_combadd(interp, dbip, objp, combname, region_flag, relation, ident, air, wdbp)
     Tcl_Interp *interp;
     struct db_i *dbip;
     register struct directory *objp;
     char *combname;
     int region_flag;			/* true if adding region */
     int relation;			/* = UNION, SUBTRACT, INTERSECT */
     int ident;				/* "Region ID" */
     int air;				/* Air code */
     struct rt_wdb *wdbp;
{
	register struct directory *dp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	union tree *tp;
	struct rt_tree_array *tree_list;
	int node_count;
	int actual_count;

	if (dbip == DBI_NULL)
		return DIR_NULL;

	/*
	 * Check to see if we have to create a new combination
	 */
	if ((dp = db_lookup(dbip,  combname, LOOKUP_QUIET)) == DIR_NULL) {
		int flags;

		if (region_flag)
			flags = DIR_REGION | DIR_COMB;
		else
			flags = DIR_COMB;

		/* Update the in-core directory */
		if ((dp = db_diradd(dbip, combname, -1, 2, flags, NULL)) == DIR_NULL )  {
			Tcl_AppendResult(interp, "An error has occured while adding '",
					 combname, "' to the database.\n", (char *)NULL);
			return DIR_NULL;
		}

		BU_GETSTRUCT(comb, rt_comb_internal);
		comb->magic = RT_COMB_MAGIC;
		bu_vls_init(&comb->shader);
		bu_vls_init(&comb->material);
		comb->region_id = -1;
		comb->tree = TREE_NULL;

		RT_INIT_DB_INTERNAL(&intern);
		intern.idb_type = ID_COMBINATION;
		intern.idb_meth = &rt_functab[ID_COMBINATION];
		intern.idb_ptr = (genptr_t)comb;

		if (region_flag) {
			struct bu_vls tmp_vls;

			comb->region_flag = 1;
			comb->region_id = ident;
			comb->aircode = air;
			comb->los = wdbp->wdb_los_default;
			comb->GIFTmater = wdbp->wdb_mat_default;
			bu_vls_init(&tmp_vls);
			bu_vls_printf(&tmp_vls,
				      "Creating region id=%d, air=%d, GIFTmaterial=%d, los=%d\n",
				      ident, air, 
					wdbp->wdb_mat_default,
					wdbp->wdb_los_default);
			Tcl_AppendResult(interp, bu_vls_addr(&tmp_vls), (char *)NULL);
			bu_vls_free(&tmp_vls);
		}

		RT_GET_TREE( tp, &rt_uniresource );
		tp->magic = RT_TREE_MAGIC;
		tp->tr_l.tl_op = OP_DB_LEAF;
		tp->tr_l.tl_name = bu_strdup( objp->d_namep );
		tp->tr_l.tl_mat = (matp_t)NULL;
		comb->tree = tp;

		if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL );
			return( DIR_NULL );
		}
		return( dp );
	}

	/* combination exists, add a new member */
	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "read error, aborting\n", (char *)NULL);
		return DIR_NULL;
	}

	comb = (struct rt_comb_internal *)intern.idb_ptr;
	RT_CK_COMB(comb);

	if (region_flag && !comb->region_flag) {
		Tcl_AppendResult(interp, combname, ": not a region\n", (char *)NULL);
		return DIR_NULL;
	}

	if (comb->tree && db_ck_v4gift_tree(comb->tree) < 0) {
		db_non_union_push(comb->tree, &rt_uniresource);
		if (db_ck_v4gift_tree(comb->tree) < 0) {
			Tcl_AppendResult(interp, "Cannot flatten tree for editing\n", (char *)NULL);
			rt_db_free_internal(&intern, &rt_uniresource);
			return DIR_NULL;
		}
	}

	/* make space for an extra leaf */
	node_count = db_tree_nleaves( comb->tree ) + 1;
	tree_list = (struct rt_tree_array *)bu_calloc( node_count,
						       sizeof( struct rt_tree_array ), "tree list" );

	/* flatten tree */
	if (comb->tree) {
		actual_count = 1 + (struct rt_tree_array *)db_flatten_tree(
			tree_list, comb->tree, OP_UNION, 1, &rt_uniresource )
			- tree_list;
		BU_ASSERT_LONG( actual_count, ==, node_count );
		comb->tree = TREE_NULL;
	}

	/* insert new member at end */
	switch (relation) {
	case '+':
		tree_list[node_count - 1].tl_op = OP_INTERSECT;
		break;
	case '-':
		tree_list[node_count - 1].tl_op = OP_SUBTRACT;
		break;
	default:
		Tcl_AppendResult(interp, "unrecognized relation (assume UNION)\n",
				 (char *)NULL );
	case 'u':
		tree_list[node_count - 1].tl_op = OP_UNION;
		break;
	}

	/* make new leaf node, and insert at end of list */
	RT_GET_TREE( tp, &rt_uniresource );
	tree_list[node_count-1].tl_tree = tp;
	tp->tr_l.magic = RT_TREE_MAGIC;
	tp->tr_l.tl_op = OP_DB_LEAF;
	tp->tr_l.tl_name = bu_strdup( objp->d_namep );
	tp->tr_l.tl_mat = (matp_t)NULL;

	/* rebuild the tree */
	comb->tree = (union tree *)db_mkgift_tree( tree_list, node_count, &rt_uniresource );

	/* and finally, write it out */
	if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Failed to write ", dp->d_namep, (char *)NULL);
		return DIR_NULL;
	}

	bu_free((char *)tree_list, "combadd: tree_list");

	return (dp);
}

static void
wdb_do_identitize(dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3)
     struct db_i		*dbip;
     struct rt_comb_internal *comb;
     union tree		*comb_leaf;
     genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	struct directory *dp;
	Tcl_Interp *interp = (Tcl_Interp *)user_ptr1;

	RT_CK_DBI(dbip);
	RT_CK_TREE(comb_leaf);

	if (!comb_leaf->tr_l.tl_mat) {
		comb_leaf->tr_l.tl_mat = (matp_t)bu_malloc(sizeof(mat_t), "tl_mat");
	}
	bn_mat_idn(comb_leaf->tr_l.tl_mat);
	if ((dp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL)
		return;

	wdb_identitize(dp, dbip, interp);
}

/*
 *			W D B _ I D E N T I T I Z E ( ) 
 *
 *	Traverses an objects paths, setting all member matrices == identity
 *
 */
void
wdb_identitize(dp, dbip, interp)
     struct directory *dp;
     struct db_i *dbip;
     Tcl_Interp *interp;
{
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;

	if (dp->d_flags & DIR_SOLID)
		return;
	if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL, &rt_uniresource) < 0) {
		Tcl_AppendResult(interp, "Database read error, aborting\n", (char *)NULL);
		return;
	}
	comb = (struct rt_comb_internal *)intern.idb_ptr;
	if (comb->tree) {
		db_tree_funcleaf(dbip, comb, comb->tree, wdb_do_identitize,
				 (genptr_t)interp, (genptr_t)NULL, (genptr_t)NULL);
		if (rt_db_put_internal(dp, dbip, &intern, &rt_uniresource) < 0) {
			Tcl_AppendResult(interp, "Cannot write modified combination (", dp->d_namep,
					 ") to database\n", (char *)NULL );
			return;
		}
	}
}
