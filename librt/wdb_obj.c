/*
 *				W D B _ O B J . C
 *
 * A database object contains the attributes and
 * methods for controlling a BRLCAD database.
 * 
 * Authors -
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

#include <fcntl.h>
#include <math.h>
#include <sys/errno.h>
#include "conf.h"
#include "tcl.h"
#include "machine.h"
#include "externs.h"
#include "cmd.h"		/* this includes bu.h */
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"

#include "./debug.h"

/* from librt/tcl.c */
extern int rt_tcl_rt();
extern int rt_tcl_import_from_path();
extern void rt_generic_make();

HIDDEN int wdb_open_tcl();
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
HIDDEN int wdb_close_tcl();

HIDDEN void wdb_deleteProc();
HIDDEN void wdb_deleteProc_rt();

HIDDEN int wdb_cmpdirname();
HIDDEN void wdb_vls_col_pr4v();
HIDDEN void wdb_vls_line_dpp();
HIDDEN struct directory ** wdb_getspace();

struct wdb_obj HeadWDBObj;	/* head of BRLCAD database object list */

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
	char		buf[32];

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

			wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK );
		} else {
			if (wdb_decode_dbip(interp, argv[3], &dbip) != TCL_OK)
				return TCL_ERROR;

			if (strcmp( argv[2], "disk" ) == 0)
				wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK );
			else if (strcmp(argv[2], "disk_append") == 0)
				wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_DISK_APPEND_ONLY );
			else if (strcmp( argv[2], "inmem" ) == 0)
				wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM );
			else if (strcmp( argv[2], "inmem_append" ) == 0)
				wdbp = wdb_dbopen( dbip, RT_WDB_TYPE_DB_INMEM_APPEND_ONLY );
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
	Tcl_AppendResult( interp, argv[1], (char *)NULL );
	
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
		char line[128];

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
	char		buf[64];

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
 * The guts of this routine were lifted from mged/dir.c.
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
  dir_basep = (struct directory **) bu_malloc(
	      (num_entries+1) * sizeof(struct directory *),
	      "wdb_getspace *dir[]" );
  return(dir_basep);
}
