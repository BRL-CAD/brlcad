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
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1997 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#include "conf.h"
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include <fcntl.h>
#include <math.h>
#include <sys/errno.h>
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"		/* this includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#include "./debug.h"

#define WDB_MAX_LEVELS 12
#define WDB_CPEVAL	0
#define WDB_LISTPATH	1
#define WDB_LISTEVAL	2

/* from librt/tcl.c */
extern int rt_tcl_rt();
extern int rt_tcl_import_from_path();
extern void rt_generic_make();

/* from librt/dg_obj.c */
extern void dgo_eraseobjall_callback();

HIDDEN int wdb_open_tcl();
HIDDEN int wdb_close_tcl();
HIDDEN int wdb_decode_dbip();
HIDDEN struct db_i *wdb_prep_dbip();

HIDDEN int wdb_cmd();
HIDDEN int wdb_match_tcl();
HIDDEN int wdb_get_tcl();
HIDDEN int wdb_put_tcl();
HIDDEN int wdb_adjust_tcl();
HIDDEN int wdb_form_tcl();
HIDDEN int wdb_tops_tcl();
HIDDEN int wdb_rt_gettrees_tcl();
HIDDEN int wdb_dump_tcl();
HIDDEN int wdb_dbip_tcl();
HIDDEN int wdb_ls_tcl();
HIDDEN int wdb_list_tcl();
HIDDEN int wdb_pathsum_tcl();
HIDDEN int wdb_expand_tcl();
HIDDEN int wdb_kill_tcl();
HIDDEN int wdb_killall_tcl();
HIDDEN int wdb_killtree_tcl();
HIDDEN void wdb_killtree_callback();

HIDDEN void wdb_deleteProc();
HIDDEN void wdb_deleteProc_rt();

HIDDEN void wdb_do_trace();
HIDDEN void wdb_trace();

HIDDEN int wdb_cmpdirname();
HIDDEN void wdb_vls_col_pr4v();
HIDDEN void wdb_vls_line_dpp();
HIDDEN struct directory ** wdb_getspace();
HIDDEN void wdb_do_list();

struct wdb_obj HeadWDBObj;	/* head of BRLCAD database object list */

HIDDEN Tcl_Interp *curr_interp;		/* current Tcl interpreter */

/* input path */
HIDDEN struct directory *wdb_objects[WDB_MAX_LEVELS];
HIDDEN int wdb_objpos;

/* print flag */
HIDDEN int wdb_prflag;

/* path transformation matrix ... calculated in trace() */
HIDDEN mat_t wdb_xform;

HIDDEN struct directory *wdb_path[WDB_MAX_LEVELS];

HIDDEN struct bu_cmdtab wdb_cmds[] = {
	"match",	wdb_match_tcl,
	"get",		wdb_get_tcl,
	"put",		wdb_put_tcl,
	"adjust",	wdb_adjust_tcl,
	"form",		wdb_form_tcl,
	"tops",		wdb_tops_tcl,
	"rt_gettrees",	wdb_rt_gettrees_tcl,
	"dump",		wdb_dump_tcl,
	"dbip",		wdb_dbip_tcl,
	"ls",		wdb_ls_tcl,
	"l",		wdb_list_tcl,
	"listeval",	wdb_pathsum_tcl,
	"paths",	wdb_pathsum_tcl,
	"expand",	wdb_expand_tcl,
	"kill",		wdb_kill_tcl,
	"killall",	wdb_killall_tcl,
	"killtree",	wdb_killtree_tcl,
#if 0
	"c",		wdb_comb_std_tcl,
	"cat",		wdb_cat_tcl,
	"comb",		wdb_comb_tcl,
	"color",	wdb_color_tcl,
	"prcolor",	wdb_prcolor_tcl,
	"comb_color",	wdb_comb_color_tcl,
	"concat",	wdb_concat_tcl,
	"cp",		wdb_copy_tcl,
	"copymat",	wdb_copymat_tcl,
	"copyeval",	wdb_copyeval_tcl,
	"dup",		wdb_dup_tcl,
	"find",		wdb_find_tcl,
	"g",		wdb_group_tcl,
	"i",		wdb_instance_tcl,
	"inside",	wdb_inside_tcl,
	"keep",		wdb_keep_tcl,
	"mv",		wdb_move_tcl,
	"mvall",	wdb_move_all_tcl,
	"pathlist",	wdb_pathlist_tcl,
	"push",		wdb_push_tcl,
	"getmat",	wdb_getmat_tcl,
	"putmat",	wdb_putmat_tcl,
	"r",		wdb_region_tcl,
	"rm",		wdb_remove_tcl,
	"summary",	wdb_summary_tcl,
	"title",	wdb_title_tcl,
	"tree",		wdb_tree_tcl,
	"units",	wdb_units_tcl,
	"whatid",	wdb_whatid_tcl,
	"whichair",	wdb_which_tcl,
	"whichid",	wdb_which_tcl,
	"which_shader",	wdb_which_shader_tcl,
	"xpush",	wdb_xpush_tcl,
	"rcodes",	wdb_rcodes_tcl,
	"wcodes",	wdb_wcodes_tcl,
	"rmater",	wdb_rmater_tcl,
	"wmater",	wdb_wmater_tcl,
#endif
	"close",	wdb_close_tcl,
	(char *)0,	(int (*)())0
};

int
Wdb_Init(interp)
     Tcl_Interp *interp;
{
	BU_LIST_INIT(&HeadWDBObj.l);
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
HIDDEN int
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
HIDDEN void
wdb_deleteProc(clientData)
     ClientData clientData;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;

	bu_vls_free(&wdbop->wdb_name);

	RT_CK_WDB(wdbop->wdb_wp);
	wdb_close(wdbop->wdb_wp);

	BU_LIST_DEQUEUE(&wdbop->l);
	bu_free((genptr_t)wdbop, "wdb_deleteProc: wdbop");
}

/*
 * Close a BRLCAD database object.
 *
 * USAGE:
 *	  procname close
 */
HIDDEN int
wdb_close_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct bu_vls vls;
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;

	if (argc != 2) {
		bu_vls_init(&vls);
		bu_vls_printf(&vls, "helplib wdb_close");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	/* Among other things, this will call wdb_deleteProc. */
	Tcl_DeleteCommand(interp, bu_vls_addr(&wdbop->wdb_name));

	return TCL_OK;
}

/*
 *			W D B _ O P E N _ T C L
 *
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be wdb_obj pointer
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
HIDDEN int
wdb_open_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp	*interp;
     int		argc;
     char		**argv;
{
	struct wdb_obj *wdbop;
	struct rt_wdb	*wdbp;

	if (argc == 1) {
		/* get list of database objects */
		for (BU_LIST_FOR(wdbop, wdb_obj, &HeadWDBObj.l))
			Tcl_AppendResult(interp, bu_vls_addr(&wdbop->wdb_name), " ", (char *)NULL);

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
						 " target type not recognized\n", NULL);
				return TCL_ERROR;
			}
		}
	}
	if (wdbp == RT_WDB_NULL) {
		Tcl_AppendResult(interp, "wdb_open ", argv[1], " failed\n", NULL);
		return TCL_ERROR;
	}

	/* acquire wdb_obj struct */
	BU_GETSTRUCT(wdbop,wdb_obj);

	/* initialize wdb_obj */
	bu_vls_init(&wdbop->wdb_name);
	bu_vls_strcpy(&wdbop->wdb_name, argv[1]);
	wdbop->wdb_wp = wdbp;

	/* append to list of wdb_obj's */
	BU_LIST_APPEND(&HeadWDBObj.l,&wdbop->l);

	/* Instantiate the newprocname, with clientData of wdbop */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand(interp, argv[1], wdb_cmd,
				(ClientData)wdbop, wdb_deleteProc);

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
	if (((dbip = db_open(filename, "r+w")) == DBI_NULL ) &&
	    ((dbip = db_open(filename, "r"  )) == DBI_NULL )) {
		/*
		 * Check to see if we can access the database
		 */
		if (access(filename, R_OK|W_OK) != 0 && errno != ENOENT) {
			perror(filename);
			return DBI_NULL;
		}

		if ((dbip = db_create(filename)) == DBI_NULL) {
			Tcl_AppendResult(interp, "wdb_open: failed to create ", filename, "\n",\
					 (char *)NULL);
			if (dbip == DBI_NULL)
				Tcl_AppendResult(interp, "opendb: no database is currently opened!", \
						 (char *)NULL);

			return DBI_NULL;
		}
	}

	/* --- Scan geometry database and build in-memory directory --- */
	db_scan(dbip, (int (*)())db_diradd, 1);

	return dbip;
}

/****************** Database Object Methods ********************/
/*
 *			W D B _ M A T C H _ T C L
 *
 * Returns (in interp->result) a list (possibly empty) of all matches to
 * the (possibly wildcard-containing) arguments given.
 * Does *NOT* return tokens that do not match anything, unlike the
 * "expand" command.
 */

HIDDEN int
wdb_match_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdb = wdbop->wdb_wp;
	struct bu_vls	matches;

	--argc;
	++argv;

	RT_CK_WDB_TCL(interp,wdb);
	
	/* Verify that this wdb supports lookup operations
	   (non-null dbip) */
	if( wdb->dbip == 0 ) {
		Tcl_AppendResult( interp, "this database does not support lookup operations" );
		return TCL_ERROR;
	}

	bu_vls_init( &matches );
	for( ++argv; *argv != NULL; ++argv ) {
		if( db_regexp_match_all( &matches, wdb->dbip, *argv ) > 0 )
			bu_vls_strcat( &matches, " " );
	}
	bu_vls_trimspace( &matches );
	Tcl_AppendResult( interp, bu_vls_addr(&matches), (char *)NULL );
	bu_vls_free( &matches );
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

HIDDEN int
wdb_get_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	register struct directory	       *dp;
	register struct bu_structparse	       *sp = NULL;
	int			id, status;
	struct rt_db_internal	intern;
	char			objecttype;
	char		       *objname;
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdb = wdbop->wdb_wp;
	Tcl_DString		ds;
	struct bu_vls		str;
	char			*curr_elem;

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
	if (wdb->dbip == 0) {
		Tcl_AppendResult(interp,
				 "db does not support lookup operations",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if (rt_tcl_import_from_path(interp, &intern, argv[1], wdb) == TCL_ERROR)
		return TCL_ERROR;

	status = intern.idb_meth->ft_tclget(interp, &intern, argv[2]);
	intern.idb_meth->ft_ifree(&intern);
	return status;
}

/*
 *			W D B _ P U T _ T C L
 **
 ** Creates an object and stuffs it into the databse.
 ** All arguments must be specified.  Object cannot already exist.
 **/

HIDDEN int
wdb_put_tcl(clientData, interp, argc, argv)
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct rt_db_internal			intern;
	register CONST struct rt_functab	*ftp;
	register struct directory	       *dp;
	int					status, ngran, i;
	char				       *name;
	char				        type[16];
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdb = wdbop->wdb_wp;

	--argc;
	++argv;

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be db put objName objType attrs",
				  (char *)NULL );
		return TCL_ERROR;
	}

	name = argv[1];
    
	/* Verify that this wdb supports lookup operations (non-null dbip).
	 * stdout/file wdb objects don't, but can still be written to.
	 * If not, just skip the lookup test and write the object
	 */
	if( wdb->dbip && db_lookup(wdb->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL ) {
		Tcl_AppendResult(interp, argv[1], " already exists",
				 (char *)NULL);
		return TCL_ERROR;
	}

	RT_INIT_DB_INTERNAL(&intern);

	for( i = 0; argv[2][i] != 0 && i < 16; i++ ) {
		type[i] = isupper(argv[2][i]) ? tolower(argv[2][i]) :
			argv[2][i];
	}
	type[i] = 0;

	ftp = rt_get_functab_by_label( type );
	if( ftp == NULL ) {
		Tcl_AppendResult( interp, type,
				  " is an unknown object type.",
				  (char *)NULL );
		return TCL_ERROR;
	}

	if( ftp->ft_make )  {
		ftp->ft_make( ftp, &intern, 0.0 );
	} else {
		rt_generic_make( ftp, &intern, 0.0 );
	}

	if( ftp->ft_tcladjust( interp, &intern, argc-3, argv+3 ) == TCL_ERROR ) {
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	if( wdb_export( wdb, name, intern.idb_ptr, intern.idb_type,
			1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", argv[1],
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
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

HIDDEN int
wdb_adjust_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	register struct directory	*dp;
	register CONST struct bu_structparse	*sp = NULL;
	int				 id, status, i;
	char				*name;
	struct rt_db_internal		 intern;
	mat_t				 idn;
	char				 objecttype;
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdb = wdbop->wdb_wp;

	if( argc < 5 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"db adjust objName attr value ?attr? ?value?...\"",
				  (char *)NULL );
		return TCL_ERROR;
	}
	name = argv[2];

	/* Verify that this wdb supports lookup operations (non-null dbip) */
	RT_CK_DBI_TCL(interp,wdb->dbip);

	dp = db_lookup( wdb->dbip, name, LOOKUP_QUIET );
	if( dp == DIR_NULL ) {
		Tcl_AppendResult( interp, name, ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal(", name,
				  ") failure\n", (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL( &intern );

	/* Find out what type of object we are dealing with and tweak it. */
	id = intern.idb_type;
	RT_CK_FUNCTAB(intern.idb_meth);

	status = intern.idb_meth->ft_tcladjust( interp, &intern, argc-3, argv+3 );
	if( status == TCL_OK && wdb_export( wdb, name, intern.idb_ptr,
					    intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult( interp, "wdb_export(", name,
				  ") failure\n", (char *)NULL );
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}

	rt_db_free_internal( &intern );
	return status;
}

/*
 *			W D B _ F O R M _ T C L
 */
HIDDEN int
wdb_form_tcl( clientData, interp, argc, argv )
     ClientData clientData;
     Tcl_Interp *interp;
     int argc;
     char **argv;
{
	CONST struct bu_structparse	*sp = NULL;
	CONST struct rt_functab		*ftp;

	--argc;
	++argv;

	if (argc != 2) {
		Tcl_AppendResult(interp, "wrong # args: should be \"db form objType\"",
				 (char *)NULL);
		return TCL_ERROR;
	}

	if( (ftp = rt_get_functab_by_label(argv[1])) == NULL )  {
		Tcl_AppendResult(interp, "There is no geometric object type \"",
				 argv[1], "\".", (char *)NULL);
		return TCL_ERROR;
	}
	return ftp->ft_tclform( ftp, interp );
}

/*
 *			W D B _ T O P S _ T C L
 */
HIDDEN int
wdb_tops_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdp = wdbop->wdb_wp;
	register struct directory *dp;
	register int i;

	RT_CK_WDB_TCL(interp, wdp);
	RT_CK_DBI_TCL(interp, wdp->dbip);

	/* Can this be executed only sometimes?
	   Perhaps a "dirty bit" on the database? */
	db_update_nref( wdp->dbip );
	
	for( i = 0; i < RT_DBNHASH; i++ )
		for( dp = wdp->dbip->dbi_Head[i];
		     dp != DIR_NULL;
		     dp = dp->d_forw )
			if( dp->d_nref == 0 )
				Tcl_AppendElement( interp, dp->d_namep );
	return TCL_OK;
}

/*
 *			R T _ T C L _ D E L E T E P R O C _ R T
 *
 *  Called when the named proc created by rt_gettrees() is destroyed.
 */
HIDDEN void
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
HIDDEN int
wdb_rt_gettrees_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdp = wdbop->wdb_wp;
	struct rt_i	*rtip;
	struct application	*ap;
	struct resource		*resp;
	char		*newprocname;

	RT_CK_WDB_TCL(interp, wdp);
	RT_CK_DBI_TCL(interp, wdp->dbip);

	if( argc < 4 )  {
		Tcl_AppendResult( interp,
				  "rt_gettrees: wrong # args: should be \"",
				  argv[0], " ", argv[1],
				  " newprocname [-i] [-u] treetops...\"\n", (char *)NULL );
		return TCL_ERROR;
	}

	rtip = rt_new_rti(wdp->dbip);
	newprocname = argv[2];

	/* Delete previous proc (if any) to release all that memory, first */
	(void)Tcl_DeleteCommand(interp, newprocname);

	while( argv[3][0] == '-' )  {
		if( strcmp( argv[3], "-i" ) == 0 )  {
			rtip->rti_dont_instance = 1;
			argc--;
			argv++;
			continue;
		}
		if( strcmp( argv[3], "-u" ) == 0 )  {
			rtip->useair = 1;
			argc--;
			argv++;
			continue;
		}
		break;
	}

	if( rt_gettrees( rtip, argc-3, (CONST char **)&argv[3], 1 ) < 0 )  {
		Tcl_AppendResult( interp,
				  "rt_gettrees() returned error\n", (char *)NULL );
		rt_free_rti( rtip );
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
	BU_GETSTRUCT( resp, resource );
	rt_init_resource( resp, 0 );
	bu_ptbl_ins_unique( &rtip->rti_resources, (long *)resp );

	BU_GETSTRUCT( ap, application );
	ap->a_resource = resp;
	ap->a_rt_i = rtip;
	ap->a_purpose = "Conquest!";

	rt_ck(rtip);

	/* Instantiate the proc, with clientData of wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand(interp, newprocname, rt_tcl_rt,
				(ClientData)ap, wdb_deleteProc_rt);

	/* Return new function name as result */
	Tcl_AppendResult( interp, newprocname, (char *)NULL );

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
HIDDEN int
wdb_dump_tcl( clientData, interp, argc, argv )
     ClientData	clientData;
     Tcl_Interp     *interp;
     int		argc;
     char	      **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct rt_wdb  *wdp = wdbop->wdb_wp;
	struct rt_wdb	*op;
	int		ret;

	RT_CK_WDB_TCL(interp, wdp);
	RT_CK_DBI_TCL(interp, wdp->dbip);

	if( argc != 3 )  {
		Tcl_AppendResult( interp,
				  "dump: wrong # args: should be \"",
				  argv[0], "dump filename.g\n", (char *)NULL );
		return TCL_ERROR;
	}

	if( (op = wdb_fopen( argv[2] )) == RT_WDB_NULL )  {
		Tcl_AppendResult( interp,
				  argv[0], " dump:  ", argv[2], ": cannot create\n",
				  (char *)NULL );
		return TCL_ERROR;
	}
	ret = db_dump(op, wdp->dbip);
	wdb_close( op );
	if( ret < 0 )  {
		Tcl_AppendResult( interp,
				  argv[0], " dump ", argv[2], ": db_dump() error\n",
				  (char *)NULL );
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
HIDDEN int
wdb_dbip_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	struct bu_vls vls;

	bu_vls_init(&vls);

	if (argc != 2) {
		bu_vls_printf(&vls, "helplib wdb_dbip");
		Tcl_Eval(interp, bu_vls_addr(&vls));
		bu_vls_free(&vls);
		return TCL_ERROR;
	}

	bu_vls_printf(&vls, "%lu", (unsigned long)wdbop->wdb_wp->dbip);
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
HIDDEN int
wdb_ls_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
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

	if(argc < 1 || MAXARGS < argc){
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
			bu_vls_printf(&vls, "Unrecognized option - %c\n", c);
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
		dirp = wdb_getspace(wdbop->wdb_wp->dbip, argc-1);
		dirp0 = dirp;
		/*
		 * Verify the names, and add pointers to them to the array.
		 */
		for (i = 1; i < argc; i++) {
			if ((dp = db_lookup(wdbop->wdb_wp->dbip, argv[i], LOOKUP_NOISY)) ==
			    DIR_NULL)
				continue;
			*dirp++ = dp;
		}
	} else {
		/* Full table of contents */
		dirp = wdb_getspace(wdbop->wdb_wp->dbip, 0);	/* Enough for all */
		dirp0 = dirp;
		/*
		 * Walk the directory list adding pointers (to the directory
		 * entries) to the array.
		 */
		for (i = 0; i < RT_DBNHASH; i++)
			for (dp = wdbop->wdb_wp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
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
HIDDEN int
wdb_list_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
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

			ts.ts_dbip = wdbop->wdb_wp->dbip;
			bn_mat_idn(ts.ts_mat);

			if (db_follow_path_for_state(&ts, &path, argv[arg], 1))
				continue;

			dp = DB_FULL_PATH_CUR_DIR( &path );

			if ((id = rt_db_get_internal(&intern, dp, wdbop->wdb_wp->dbip, ts.ts_mat)) < 0) {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
						 ") failure\n", (char *)NULL );
				continue;
			}

			db_free_full_path( &path );

			bu_vls_printf( &str, "%s:  ", argv[arg] );

			if (rt_functab[id].ft_describe(&str, &intern, 99, wdbop->wdb_wp->dbip->dbi_base2local) < 0)
				Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);

			rt_functab[id].ft_ifree(&intern);
		} else {
			if ((dp = db_lookup(wdbop->wdb_wp->dbip, argv[arg], LOOKUP_NOISY)) == DIR_NULL)
				continue;

			wdb_do_list(wdbop->wdb_wp->dbip, interp, &str, dp, 99);	/* very verbose */
		}
	}

	Tcl_AppendResult(interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);

	return TCL_OK;
}

/*
 */
HIDDEN void
wdb_do_trace(dbip, comb, comb_leaf, user_ptr1, user_ptr2, user_ptr3)
     struct db_i		*dbip;
     struct rt_comb_internal *comb;
     union tree		*comb_leaf;
     genptr_t		user_ptr1, user_ptr2, user_ptr3;
{
	int			*pathpos;
	int			*flag;
	matp_t			old_xlate;
	mat_t			new_xlate;
	struct directory	*nextdp;

	RT_CK_DBI(dbip);
	RT_CK_TREE(comb_leaf);

	pathpos = (int *)user_ptr1;
	old_xlate = (matp_t)user_ptr2;
	flag = (int *)user_ptr3;

	if (comb_leaf->tr_l.tl_mat)  {
		bn_mat_mul(new_xlate, old_xlate, comb_leaf->tr_l.tl_mat);
	} else {
		bn_mat_copy(new_xlate, old_xlate);
	}
	if ((nextdp = db_lookup(dbip, comb_leaf->tr_l.tl_name, LOOKUP_NOISY)) == DIR_NULL)
		return;

	wdb_trace(dbip, nextdp, (*pathpos)+1, new_xlate, *flag);
}

/*
 */
HIDDEN void
wdb_trace(dbip, dp, pathpos, old_xlate, flag)
     struct db_i		*dbip;
     register struct directory *dp;
     int pathpos;
     mat_t old_xlate;
     int flag;
{
	struct directory *nextdp;
	struct rt_db_internal intern;
	struct rt_comb_internal *comb;
	mat_t new_xlate;
	int nparts, i, k;
	int id;
	struct bu_vls str;

	bu_vls_init( &str );
	
	if (pathpos >= WDB_MAX_LEVELS) {
		struct bu_vls tmp_vls;

		bu_vls_init(&tmp_vls);
		bu_vls_printf(&tmp_vls, "nesting exceeds %d levels\n", WDB_MAX_LEVELS);
		Tcl_AppendResult(curr_interp, bu_vls_addr(&tmp_vls), (char *)NULL);
		bu_vls_free(&tmp_vls);

		for(i=0; i < WDB_MAX_LEVELS; i++)
			Tcl_AppendResult(curr_interp, "/", wdb_path[i]->d_namep, (char *)NULL);

		Tcl_AppendResult(curr_interp, "\n", (char *)NULL);
		return;
	}

	if (dp->d_flags & DIR_COMB) {
		if (rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL) < 0) {
			Tcl_AppendResult(curr_interp, "Database read error, aborting\n", (char *)NULL);
			return;
		}

		wdb_path[pathpos] = dp;
		comb = (struct rt_comb_internal *)intern.idb_ptr;
		if (comb->tree)
			db_tree_funcleaf(dbip, comb, comb->tree, wdb_do_trace,
					 (genptr_t)&pathpos, (genptr_t)old_xlate, (genptr_t)&flag);
		rt_comb_ifree(&intern);
		return;
	}

	/* not a combination  -  should have a solid */

	/* last (bottom) position */
	wdb_path[pathpos] = dp;

	/* check for desired path */
	for (k=0; k<wdb_objpos; k++) {
		if (wdb_path[k]->d_addr != wdb_objects[k]->d_addr) {
			/* not the desired path */
			return;
		}
	}

	/* have the desired path up to wdb_objpos */
	bn_mat_copy(wdb_xform, old_xlate);
	wdb_prflag = 1;

	if (flag == WDB_CPEVAL)
		return;

	/* print the path */
	for (k=0; k<pathpos; k++)
		Tcl_AppendResult(curr_interp, "/", wdb_path[k]->d_namep, (char *)NULL);

	if (flag == WDB_LISTPATH) {
		bu_vls_printf( &str, "/%16s:\n", dp->d_namep );
		Tcl_AppendResult(curr_interp, bu_vls_addr(&str), (char *)NULL);
		bu_vls_free(&str);
		return;
	}

	/* NOTE - only reach here if flag == WDB_LISTEVAL */
	Tcl_AppendResult(curr_interp, "/", (char *)NULL);
	if ((id=rt_db_get_internal(&intern, dp, dbip, wdb_xform)) < 0) {
		Tcl_AppendResult(curr_interp, "rt_db_get_internal(", dp->d_namep,
				 ") failure", (char *)NULL );
		return;
	}
	bu_vls_printf(&str, "%16s:\n", dp->d_namep);
	if (rt_functab[id].ft_describe(&str, &intern, 1, dbip->dbi_base2local) < 0)
		Tcl_AppendResult(curr_interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_functab[id].ft_ifree(&intern);
	Tcl_AppendResult(curr_interp, bu_vls_addr(&str), (char *)NULL);
	bu_vls_free(&str);
}

/*
 *  1.  produces path for purposes of matching
 *  2.  gives all paths matching the input path OR
 *  3.  gives a summary of all paths matching the input path
 *	including the final parameters of the solids at the bottom
 *	of the matching paths
 *
 * Usage:
 *        procname (WDB_LISTEVAL|paths) args(s)
 */
HIDDEN int
wdb_pathsum_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	int i, flag, pos_in;

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
	curr_interp = interp;

	/* pos_in = first member of path entered
	 *
	 *	paths are matched up to last input member
	 *      ANY path the same up to this point is considered as matching
	 */
	wdb_prflag = 0;

	/* find out which command was entered */
	if (strcmp(argv[0], "paths") == 0) {
		/* want to list all matching paths */
		flag = WDB_LISTPATH;
	}
	if (strcmp(argv[0], "WDB_LISTEVAL") == 0) {
		/* want to list evaluated solid[s] */
		flag = WDB_LISTEVAL;
	}

	pos_in = 1;

	if (argc == 2 && strchr(argv[1], '/')) {
		char *tok;
		wdb_objpos = 0;

		tok = strtok( argv[1], "/" );
		while (tok) {
			if ((wdb_objects[wdb_objpos++] = db_lookup(wdbop->wdb_wp->dbip, tok, LOOKUP_NOISY)) == DIR_NULL)
				return TCL_ERROR;
			tok = strtok( (char *)NULL, "/" );
		}
	} else {
		wdb_objpos = argc-1;

		/* build directory pointer array for desired path */
		for (i=0; i<wdb_objpos; i++) {
			if ((wdb_objects[i] = db_lookup(wdbop->wdb_wp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == DIR_NULL)
				return TCL_ERROR;
		}
	}

	bn_mat_idn( wdb_xform );

	wdb_trace(wdbop->wdb_wp->dbip, wdb_objects[0], 0, bn_mat_identity, flag);

	if (wdb_prflag == 0) {
		/* path not found */
		Tcl_AppendResult(interp, "PATH:  ", (char *)NULL);
		for (i=0; i<wdb_objpos; i++)
			Tcl_AppendResult(interp, "/", wdb_objects[i]->d_namep, (char *)NULL);

		Tcl_AppendResult(interp, "  NOT FOUND\n", (char *)NULL);
	}

	return TCL_OK;
}

HIDDEN void
dgo_scrape_escapes_AppendResult(interp, str)
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
HIDDEN int
wdb_expand_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
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
			dgo_scrape_escapes_AppendResult(interp, argv[whicharg]);
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
			for (dp = wdbop->wdb_wp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
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
			dgo_scrape_escapes_AppendResult(interp, argv[whicharg]);
		}
	}

	return TCL_OK;
}

/*
 * Usage:
 *        procname kill arg(s)
 */
HIDDEN int
wdb_kill_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	register struct directory *dp;
	register int i;
	register struct dm_list *dmlp;
	register struct dm_list *save_dmlp;
	int	is_phony;
	int	verbose = LOOKUP_NOISY;

	if (wdbop->wdb_wp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!\n", (char *)NULL);
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
		if ((dp = db_lookup(wdbop->wdb_wp->dbip,  argv[i], verbose)) != DIR_NULL) {
			is_phony = (dp->d_addr == RT_DIR_PHONY_ADDR);

			/* don't worry about phony objects */
			if (is_phony)
				continue;

			/* notify drawable geometry objects associated with this database object */
			dgo_eraseobjall_callback(interp, wdbop->wdb_wp->dbip, dp);

			if (db_delete(wdbop->wdb_wp->dbip, dp) < 0 ||
			    db_dirdelete(wdbop->wdb_wp->dbip, dp) < 0) {
				/* Abort kill processing on first error */
				Tcl_AppendResult(interp,
						 "an error occurred while deleting ",
						 argv[i], "\n", (char *)NULL);
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
HIDDEN int
wdb_killall_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;

	register int	i,k;
	register struct directory *dp;
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	int			ret;

	if (wdbop->wdb_wp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!\n", (char *)NULL);
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
		for (dp = wdbop->wdb_wp->dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw) {
			if (!(dp->d_flags & DIR_COMB))
				continue;

			if (rt_db_get_internal(&intern, dp, wdbop->wdb_wp->dbip, (fastf_t *)NULL) < 0) {
				Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
					") failure", (char *)NULL );
				ret = TCL_ERROR;
				continue;
			}
			comb = (struct rt_comb_internal *)intern.idb_ptr;
			RT_CK_COMB(comb);

			for (k=2; k<argc; k++) {
				int	code;

				code = db_tree_del_dbleaf(&(comb->tree), argv[k]);
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

			if (rt_db_put_internal(dp, wdbop->wdb_wp->dbip, &intern) < 0) {
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
HIDDEN int
wdb_killtree_tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
	register struct directory *dp;
	register int i;

	if (wdbop->wdb_wp->dbip->dbi_read_only) {
		Tcl_AppendResult(interp, "Database is read-only!\n", (char *)NULL);
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

	curr_interp = interp;

	/* skip past procname */
	argc--;
	argv++;

	for (i=1; i<argc; i++) {
		if ((dp = db_lookup(wdbop->wdb_wp->dbip, argv[i], LOOKUP_NOISY)) == DIR_NULL)
			continue;
		db_functree(wdbop->wdb_wp->dbip, dp, wdb_killtree_callback, wdb_killtree_callback);
	}

	return TCL_OK;
}

/*
 *			K I L L T R E E
 */
HIDDEN void
wdb_killtree_callback(dbip, dp)
struct db_i	*dbip;
register struct directory *dp;
{
	if (dbip == DBI_NULL)
		return;

	Tcl_AppendResult(curr_interp, "KILL ", (dp->d_flags & DIR_COMB) ? "COMB" : "Solid",
			 ":  ", dp->d_namep, "\n", (char *)NULL);

	/* notify drawable geometry objects associated with this database object */
	dgo_eraseobjall_callback(curr_interp, dbip, dp);

	if (db_delete(dbip, dp) < 0 || db_dirdelete(dbip, dp) < 0) {
		Tcl_AppendResult(curr_interp,
				 "an error occurred while deleting ",
				 dp->d_namep, "\n", (char *)NULL);
	}
}

#if 0
/*
 * Usage:
 *        procname 
 */
HIDDEN int
wdb__tcl(clientData, interp, argc, argv)
     ClientData clientData;
     Tcl_Interp *interp;
     int     argc;
     char    **argv;
{
	struct wdb_obj *wdbop = (struct wdb_obj *)clientData;
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

/*
 *			D B O _ V L S _ C O L _ P R 4 V
 *
 *  Given a pointer to a list of pointers to names and the number of names
 *  in that list, sort and print that list in column order over four columns.
 *  This routine was lifted from mged/columns.c.
 */
HIDDEN void
wdb_vls_col_pr4v(vls, list_of_names, num_in_list)
     struct bu_vls *vls;
     struct directory **list_of_names;
     int num_in_list;
{
	int lines, i, j, namelen, this_one;

	qsort( (genptr_t)list_of_names,
	       (unsigned)num_in_list, (unsigned)sizeof(struct directory *),
	       (int (*)())wdb_cmpdirname);

	/*
	 * For the number of (full and partial) lines that will be needed,
	 * print in vertical format.
	 */
	lines = (num_in_list + 3) / 4;
	for( i=0; i < lines; i++) {
		for( j=0; j < 4; j++) {
			this_one = j * lines + i;
			/* Restrict the print to 16 chars per spec. */
			bu_vls_printf(vls,  "%.16s", list_of_names[this_one]->d_namep);
			namelen = strlen( list_of_names[this_one]->d_namep);
			if( namelen > 16)
				namelen = 16;
			/*
			 * Region and ident checks here....  Since the code
			 * has been modified to push and sort on pointers,
			 * the printing of the region and ident flags must
			 * be delayed until now.  There is no way to make the
			 * decision on where to place them before now.
			 */
			if(list_of_names[this_one]->d_flags & DIR_COMB) {
				bu_vls_putc(vls, '/');
				namelen++;
			}
			if(list_of_names[this_one]->d_flags & DIR_REGION) {
				bu_vls_putc(vls, 'R');
				namelen++;
			}
			/*
			 * Size check (partial lines), and line termination.
			 * Note that this will catch the end of the lines
			 * that are full too.
			 */
			if( this_one + lines >= num_in_list) {
				bu_vls_putc(vls, '\n');
				break;
			} else {
				/*
				 * Pad to next boundary as there will be
				 * another entry to the right of this one. 
				 */
				while( namelen++ < 20)
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
HIDDEN void
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
		    !cflag && !rflag && !sflag ||
		    cflag && isComb ||
		    rflag && isRegion ||
		    sflag && isSolid) {
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
HIDDEN struct directory **
wdb_getspace(dbip, num_entries)
     struct db_i *dbip;
     register int num_entries;
{
	register struct directory *dp;
	register int i;
	register struct directory **dir_basep;

	if (num_entries < 0) {
		bu_log("wdb_getspace: was passed %d, used 0\n",
		       num_entries);
		num_entries = 0;
	}

	if (num_entries == 0) {
		/* Set num_entries to the number of entries */
		for (i = 0; i < RT_DBNHASH; i++)
			for (dp = dbip->dbi_Head[i]; dp != DIR_NULL; dp = dp->d_forw)
				num_entries++;
	}

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

	if (dbip == DBI_NULL)
		return;

	if ((id = rt_db_get_internal(&intern, dp, dbip, (fastf_t *)NULL)) < 0) {
		Tcl_AppendResult(interp, "rt_db_get_internal(", dp->d_namep,
				 ") failure\n", (char *)NULL);
		return;
	}

	bu_vls_printf(outstrp, "%s:  ", dp->d_namep);

	if (rt_functab[id].ft_describe(outstrp, &intern,
				       verbose, dbip->dbi_base2local) < 0)
		Tcl_AppendResult(interp, dp->d_namep, ": describe error\n", (char *)NULL);
	rt_functab[id].ft_ifree(&intern);
}
