/*
 *			T C L . C
 *
 *  Tcl interfaces to LIBRT routines.
 *
 *  LIBRT routines are not for casual command-line use;
 *  as a result, the Tcl name for the function should be exactly
 *  the same as the C name for the underlying function.
 *  Typing "rt_" is no hardship when writing Tcl procs, and
 *  clarifies the origin of the routine.
 *
 *  Author -
 *	Michael John Muuss
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
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "tcl.h"

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "externs.h"

#define RT_CK_DBI_TCL(_p)	BU_CKMAG_TCL(interp,_p,DBI_MAGIC,"struct db_i")
#define RT_CK_RTI_TCL(_p)	BU_CKMAG_TCL(interp,_p,RTI_MAGIC,"struct rt_i")
#define RT_CK_WDB_TCL(_p)	BU_CKMAG_TCL(interp,_p,WDB_MAGIC,"struct rt_wdb")

#if defined(USE_PROTOTYPES)
Tcl_CmdProc rt_db;
#else
int rt_db();
#endif

BU_EXTERN(void	bu_badmagic_tcl, (Tcl_Interp *interp, CONST long *ptr,
				long magic, CONST char *str,
				CONST char *file, int line));

/*
 *			B U _ B A D M A G I C _ T C L
 */
void
bu_badmagic_tcl( interp, ptr, magic, str, file, line )
Tcl_Interp	*interp;
CONST long	*ptr;
long		magic;
CONST char	*str;
CONST char	*file;
int		line;
{
	char	buf[256];

	if( !(ptr) )  { 
		sprintf(buf, "ERROR: NULL %s pointer, file %s, line %d\n", 
			str, file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	if( *((long *)(ptr)) != (magic) )  { 
		sprintf(buf, "ERROR: bad pointer x%x: s/b %s(x%lx), was %s(x%lx), file %s, line %d\n", 
			ptr,
			str, magic,
			bu_identify_magic( *(ptr) ), *(ptr),
			file, line ); 
		Tcl_AppendResult(interp, buf, NULL);
		return;
	}
	Tcl_AppendResult(interp, "bu_badmagic_tcl() mysterious error condition, ", str, " pointer, ", file, "\n", NULL);
}

int num_dbih = 0, max_num_dbih;
struct db_i **dbip_list = 0;

/*
 *			D B _ T C L _ R E G I S T E R
 *
 *  Registers a DB_I pointer with the Tcl interface subsystem.
 *  Returns a handle that can be used by Tcl scripts to refer to this
 *  pointer.
 */

int
db_tcl_register( dbip )
struct db_i *dbip;
{
	int dbih = num_dbih;

	if( num_dbih == 0 ) {
		max_num_dbih = 8;
		dbip_list = (struct db_i **)bu_calloc( max_num_dbih,
						    sizeof(struct db_i *),
						    "first dbi pointer list" );
	} else if( num_dbih == max_num_dbih ) {
		struct db_i **old_dbip_list = dbip_list;
		dbip_list = (struct db_i **)bu_calloc( max_num_dbih*2,
						      sizeof(struct db_i *),
						      "new dbi pointer list" );
		bcopy( (char *)old_dbip_list, (char *)dbip_list,
		       max_num_dbih*sizeof(struct db_i *) );
		bu_free( old_dbip_list, "old dbi pointer list" );
		max_num_dbih *= 2;
	}

	++num_dbih;
	dbip_list[dbih] = dbip;

	return dbih + 1000;
}


/*
 *		D B _ T C L _ U N R E G I S T E R
 *
 *  Notifies the Tcl interface subsystem that the given dbi handle is
 *  no longer valid.
 */

void
db_tcl_unregister( dbih )
int	dbih;
{
	dbih -= 1000;

	if( dbih<0 || dbih>=num_dbih )
		rt_bomb( "db_tcl_unregister: handle out of range" );
	if( dbip_list[dbih] == 0 )
		rt_bomb( "db_tcl_unregister: handle unregistered twice" );
	dbip_list[dbih] = 0;
}


/*
 *		D B _ T C L _ C H A N G E _ R E G I S T E R E D
 *
 *  Changes the database instance to which the given handle points.
 */

void
db_tcl_change_registered( dbih, dbip )
int		dbih;
struct db_i    *dbip;
{
	dbih -= 1000;

	if( dbih<0 || dbih>=num_dbih )
		rt_bomb( "db_tcl_change_registered: handle out of range" );
	dbip_list[dbih] = dbip;
}
 

/*
 		D B _ T C L _ G E T _ R E G I S T E R E D
 *
 *  Looks for the dbip associated with a given handle and implicitly
 *  returns it. 
 *  If none exists, puts an appropriate error message in interp->result.
 *  For the exclusive use of the routines in this file.
 */

int
db_tcl_get_registered( interp, dbih, dbip_return )
Tcl_Interp	*interp;
int		 dbih;
struct db_i    **dbip_return;
{
	register int i;
	
	dbih -= 1000;
	
	if( dbih<0 || dbih>=num_dbih ) {
		Tcl_AppendResult( interp, "db handle out of range",
				  TCL_STATIC );
		return TCL_ERROR;
	}
	if( dbip_list[dbih] == 0 ) {
		Tcl_AppendResult( interp, "db handle no longer valid",
				  TCL_STATIC );
		return TCL_ERROR;
	}
	
	if( dbip_return ) *dbip_return = dbip_list[dbih];
	return TCL_OK;
}	


/*
 *			W D B _ O P E N
 *
 *  A TCL interface to wdb_fopen() and wdb_dbopen().
 *
 *  Implicit return -
 *	Creates a new TCL proc which responds to get/put/etc. arguments
 *	when invoked.  clientData of that proc will be wdb pointer
 *	for this instance of the database.
 *	Easily allows keeping track of multiple databases.
 *
 *  Explicit return -
 *	wdb pointer, for more traditional C-style interfacing.
 *
 *  Example -
 *	set wdbp [wdb_open .inmem inmem $dbip]
 *	.inmem get box.s
 */
int
wdb_open( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp	*interp;
int		argc;
char		**argv;
{
	struct rt_wdb	*wdb;
	char		buf[32];

	if( argc != 4 )  {
		Tcl_AppendResult(interp, "\
Usage: wdb_open widget_command file filename\n\
       wdb_open widget_command disk $dbip\n\
       wdb_open widget_command inmem $dbip\n\
       wdb_open widget_command inmem_append $dbip\n",
		NULL);
		return TCL_ERROR;
	}

	if( strcmp( argv[2], "file" ) == 0 )  {
		wdb = wdb_fopen( argv[3] );
	} else {
		struct db_i	*dbip;

		if( db_tcl_get_registered( interp,
					   atoi(argv[3]), &dbip ) != TCL_OK )
			return TCL_ERROR;

		/* This should always succeed */
		RT_CK_DBI_TCL(dbip);

		if( strcmp( argv[2], "disk" ) == 0 )  {
			wdb = wdb_dbopen( dbip, WDB_TYPE_DB_DISK );
		} else if( strcmp( argv[2], "inmem" ) == 0 )  {
			wdb = wdb_dbopen( dbip, WDB_TYPE_DB_INMEM );
			/* TEMPORARY: Prevent accidents in MGED */
			if( wdb && !dbip->dbi_read_only )  {
				Tcl_AppendResult(interp, "(database changed to read-only)\n", NULL);
				dbip->dbi_read_only = 1;
			}
		} else if( strcmp( argv[2], "inmem_append" ) == 0 )  {
			wdb = wdb_dbopen( dbip, WDB_TYPE_DB_INMEM_APPEND_ONLY );
		} else {
			Tcl_AppendResult(interp, "wdb_open ", argv[2],
				" target type not recognized\n", NULL);
			return TCL_ERROR;
		}
	}
	if( wdb == WDB_NULL )  {
		Tcl_AppendResult(interp, "wdb_open ", argv[1], " failed\n", NULL);
		return TCL_ERROR;
	}

	/* Instantiate the widget_command, with clientData of wdb */
	/* XXX should we see if it exists first? default=overwrite */
	/* XXX Should provide delete proc to free up wdb */
	/* Beware, returns a "token", not TCL_OK. */
	(void)Tcl_CreateCommand( interp, argv[1], rt_db,
				 (ClientData)wdb, (Tcl_CmdDeleteProc *)NULL );

#ifndef WHY_WOULD_YOU_WANT_TO_DO_THIS_QUESTION_MARK
	sprintf(buf, "%d", wdb);
	Tcl_AppendResult(interp, buf, NULL);
#else
	Tcl_AppendResult( interp, argv[1], (char *)NULL );
#endif	
	
	return TCL_OK;
}

/*
 *			R T _ T C L _ G E T _ C O M B
 */
struct rt_comb_internal *
rt_tcl_get_comb(ip, interp, wdbp_str, name, wdbpp)
struct rt_db_internal	*ip;
Tcl_Interp		*interp;
CONST char		*wdbp_str;
CONST char		*name;
struct rt_wdb		**wdbpp;
{
	struct directory	*dp;
	struct db_i		*dbip;
	struct rt_comb_internal	*comb;

	*wdbpp = (struct rt_wdb *)atoi(wdbp_str);
	/* RT_CK_WDB_TCL */
	if( !(*wdbpp) || *((long *)(*wdbpp)) != WDB_MAGIC )  {
		bu_badmagic_tcl(interp, (long *)(*wdbpp), WDB_MAGIC, "struct rt_wdb", __FILE__, __LINE__);
		return NULL;
	}

	dbip = (*wdbpp)->dbip;
	RT_CK_DBI(dbip);

	if( (dp = db_lookup( dbip, name, LOOKUP_NOISY)) == DIR_NULL )
		return NULL;
	if( (dp->d_flags & DIR_COMB) == 0 )  {
		Tcl_AppendResult(interp, dp->d_namep, ": not a combination\n", (char *)NULL);
		return NULL;
	}

	if( rt_db_get_internal( ip, dp, dbip, (mat_t *)NULL ) < 0 )  {
		Tcl_AppendResult(interp, "rt_db_get_internal ", dp->d_namep, " failure\n", NULL);
		return NULL;
	}
	comb = (struct rt_comb_internal *)ip->idb_ptr;
	RT_CK_COMB(comb);
	return comb;
}

/*
 *			R T _ W D B _ I N M E M _ R G B
 *
 *  XXX A hack until "db adjust" works on combinations.
 *  XXX Bad name, no longer restricted to inmem databases.
 */
int
rt_wdb_inmem_rgb( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	struct rt_wdb		*wdbp;

	if( argc != 6 )  {
		Tcl_AppendResult(interp, "Usage: rt_wdb_inmem_rgb $wdbp comb r g b\n", NULL);
		return TCL_ERROR;
	}

	if( !(comb = rt_tcl_get_comb( &intern, interp, argv[1], argv[2], &wdbp )) )
		return TCL_ERROR;

	/* Make mods to comb here */
	comb->rgb[0] = atoi(argv[3+0]);
	comb->rgb[1] = atoi(argv[3+1]);
	comb->rgb[2] = atoi(argv[3+2]);

	if( wdb_export( wdbp, argv[2], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult(interp, "wdb_export ", argv[2], " failure\n", NULL);
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}
	rt_db_free_internal( &intern );
	return TCL_OK;
}

/*
 *			R T _ W D B _ I N M E M _ S H A D E R
 */
rt_wdb_inmem_shader( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;
	struct rt_wdb		*wdbp;

	if( argc < 4 )  {
		Tcl_AppendResult(interp, "Usage: rt_wdb_inmem_shader $wdbp comb shader [params]\n", NULL);
		return TCL_ERROR;
	}

	if( !(comb = rt_tcl_get_comb( &intern, interp, argv[1], argv[2], &wdbp )) )
		return TCL_ERROR;

	/* Make mods to comb here */
	bu_vls_trunc( &comb->shader, 0 );
	bu_vls_from_argv( &comb->shader, argc-3, &argv[3] );

	if( wdb_export( wdbp, argv[2], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult(interp, "wdb_export ", argv[2], " failure\n", NULL);
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}
	rt_db_free_internal( &intern );
	return TCL_OK;
}

/*
 *                     D B . C
 *
 * Source file for primitive database manipulation routines.
 *
 * Rather than being run directly (like the mged "db" command),
 * will be run as a widget_command instantiation of the "database class".
 * clientData will have wdb pointer.
 *
 * Author -
 *      Glenn Durfee
 */

#if defined(USE_PROTOTYPES)
Tcl_CmdProc rt_db_match, rt_db_get, rt_db_put, rt_db_adjust, rt_db_form;
#else
int rt_db_match(), rt_db_get(), rt_db_put(), rt_db_adjust(), rt_db_form();
#endif

struct dbcmdstruct {
	char *cmdname;
	int (*cmdfunc)();
} rt_db_cmds[] = {
	"match",	rt_db_match,
	"get",		rt_db_get,
	"put",		rt_db_put,
	"adjust",	rt_db_adjust,
	"form",		rt_db_form,
	(char *)0,	(int (*)())0
};

/*
 *			R T _ D B
 *
 * Generic interface for the database_class manipulation routines.
 * Usage:
 *        widget_command dbCmdName ?args?
 * Returns: result of cmdName database command.
 */

int
rt_db( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct dbcmdstruct	*dbcmd;
	struct rt_wdb		*wdb = (struct rt_wdb *)clientData;

	if( argc < 2 ) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " command [args...]\"",
				  (char *)NULL );
		return TCL_ERROR;
	}

	/* Could core dump */
	RT_CK_WDB_TCL(wdb);

	Tcl_AppendResult( interp, "unknown database command; must be one of:",
			  (char *)NULL );
	for( dbcmd = rt_db_cmds; dbcmd->cmdname != NULL; dbcmd++ ) {
		if( strcmp(dbcmd->cmdname, argv[1]) == 0 ) {
			/* hack: dispose of error msg if OK */
			Tcl_ResetResult( interp );
			return (*dbcmd->cmdfunc)( clientData, interp,
						  argc-1, argv+1 );
		}
		Tcl_AppendResult( interp, " ", dbcmd->cmdname, (char *)NULL );
	}

	return TCL_ERROR;
}



/*
 *			R T _ D B _ M A T C H
 *
 * Returns (in interp->result) a list (possibly empty) of all matches to
 * the (possibly wildcard-containing) arguments given.
 * Does *NOT* return tokens that do not match anything, unlike the
 * "expand" command.
 */

int
rt_db_match( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	struct rt_wdb  *wdb = (struct rt_wdb *)clientData;
	struct bu_vls	matches;

	RT_CK_WDB_TCL(wdb);
	
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

extern struct bu_structparse rt_tor_parse[];
extern struct bu_structparse rt_tgc_parse[];
extern struct bu_structparse rt_ell_parse[];
extern struct bu_structparse rt_arb8_parse[];
/* ARS -- not supported yet */
extern struct bu_structparse rt_half_parse[];
/* REC -- subsumed by TGC */
/* POLY -- not supported yet */
/* BSPLINE -- not supported yet */
/* SPH -- not supported yet */
/* NMG -- not supported yet */
extern struct bu_structparse rt_ebm_parse[];
extern struct bu_structparse rt_vol_parse[];
/* ARBN -- not supported yet */
/* PIPE -- not supported yet */
extern struct bu_structparse rt_part_parse[];
extern struct bu_structparse rt_rpc_parse[];
extern struct bu_structparse rt_rhc_parse[];
extern struct bu_structparse rt_epa_parse[];
extern struct bu_structparse rt_ehy_parse[];
extern struct bu_structparse rt_eto_parse[];
/* GRIP -- not supported yet */
/* JOINT -- not supported yet */
extern struct bu_structparse rt_hf_parse[];
extern struct bu_structparse rt_dsp_parse[];
/* COMB -- supported below */

struct rt_solid_type_lookup {
	char			id;
	size_t			db_internal_size;
	long			magic;
	char		       *label;
	struct bu_structparse  *parsetab;
} rt_solid_type_lookup[] = {
	{ ID_TOR,     sizeof(struct rt_tor_internal), (long)RT_TOR_INTERNAL_MAGIC, "tor", rt_tor_parse },
	{ ID_TGC,     sizeof(struct rt_tgc_internal), (long)RT_TGC_INTERNAL_MAGIC, "tgc", rt_tgc_parse },
	{ ID_ELL,     sizeof(struct rt_ell_internal), (long)RT_ELL_INTERNAL_MAGIC, "ell", rt_ell_parse },
	{ ID_ARB8,    sizeof(struct rt_arb_internal), (long)RT_ARB_INTERNAL_MAGIC, "arb8",rt_arb8_parse },
	{ ID_HALF,    sizeof(struct rt_half_internal),(long)RT_HALF_INTERNAL_MAGIC,"half",rt_half_parse },
	{ ID_REC,     sizeof(struct rt_tgc_internal), (long)RT_TGC_INTERNAL_MAGIC, "rec", rt_tgc_parse },
	{ ID_SPH,     sizeof(struct rt_ell_internal), (long)RT_ELL_INTERNAL_MAGIC, "sph", rt_ell_parse },
	{ ID_EBM,     sizeof(struct rt_ebm_internal), (long)RT_EBM_INTERNAL_MAGIC, "ebm", rt_ebm_parse },
	{ ID_VOL,     sizeof(struct rt_vol_internal), (long)RT_VOL_INTERNAL_MAGIC, "vol", rt_vol_parse },
	{ ID_PARTICLE,sizeof(struct rt_part_internal),(long)RT_PART_INTERNAL_MAGIC,"part",rt_part_parse },
	{ ID_RPC,     sizeof(struct rt_rpc_internal), (long)RT_RPC_INTERNAL_MAGIC, "rpc", rt_rpc_parse },
	{ ID_RHC,     sizeof(struct rt_rhc_internal), (long)RT_RHC_INTERNAL_MAGIC, "rhc", rt_rhc_parse },
	{ ID_EPA,     sizeof(struct rt_epa_internal), (long)RT_EPA_INTERNAL_MAGIC, "epa", rt_epa_parse },
	{ ID_EHY,     sizeof(struct rt_ehy_internal), (long)RT_EHY_INTERNAL_MAGIC, "ehy", rt_ehy_parse },
	{ ID_ETO,     sizeof(struct rt_eto_internal), (long)RT_ETO_INTERNAL_MAGIC, "eto", rt_eto_parse },
	{ ID_HF,      sizeof(struct rt_hf_internal),  (long)RT_HF_INTERNAL_MAGIC,  "hf",  rt_hf_parse },
	{ ID_DSP,     sizeof(struct rt_dsp_internal), (long)RT_DSP_INTERNAL_MAGIC, "dsp", rt_dsp_parse },
	{ 0, 0, 0, 0 }
};

struct rt_solid_type_lookup *
rt_get_parsetab_by_id( s_id )
int s_id;
{
	register struct rt_solid_type_lookup   *stlp;
	static struct rt_solid_type_lookup	solid_type;

	for( stlp = rt_solid_type_lookup; stlp->id != 0; stlp++ )
		if( stlp->id == s_id )
			return stlp;

	return NULL;
}

struct rt_solid_type_lookup *
rt_get_parsetab_by_name( s_type )
char *s_type;
{
	register int				i;
	register struct rt_solid_type_lookup   *stlp;
	char				       type[16];

	for( i = 0; s_type[i] != 0 && i < 16; i++ )
		type[i] = isupper(s_type[i]) ? tolower(s_type[i]) : s_type[i];
	type[i] = 0;

	for( stlp = rt_solid_type_lookup; stlp->id != 0; stlp++ )
		if( strcmp(type, stlp->label) == 0 )
			return stlp;

	return NULL;
}

/*
 *			R T _ D B _ G E T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument the name
 ** of an object in the database.  If only one argument is given, this routine
 ** then fills the result string with the (minimal) attributes of the item.
 ** If a second, optional, argument is provided, this function looks up the
 ** property with that name of the item given, and returns it as the result
 ** string.
 **/

int
rt_db_get( clientData, interp, argc, argv )
ClientData	clientData;
Tcl_Interp     *interp;
int		argc;
char	      **argv;
{
	register struct directory	       *dp;
	register struct bu_structparse	       *sp = NULL;
	register struct rt_solid_type_lookup   *stlp;
	int			id, status;
	struct rt_db_internal	intern;
	struct bu_vls		str;
	mat_t			idn;
	char			objecttype;
	char		       *objname;
	struct rt_wdb	       *wdb = (struct rt_wdb *)clientData;

	if( argc < 2 || argc > 3) {
		Tcl_AppendResult( interp,
				  "wrong # args: should be \"", argv[0],
				  " objName ?attr?\"", (char *)NULL );
		return TCL_ERROR;
	}

	bu_vls_init( &str );

	/* XXX Verify that this wdb supports lookup operations
	       (non-null dbip) */

	dp = db_lookup( wdb->dbip, argv[1], LOOKUP_QUIET );
	if( dp == NULL ) {
		Tcl_AppendResult( interp, argv[1], ": not found\n",
				  (char *)NULL );
		return TCL_ERROR;
	}

	status = rt_db_get_internal( &intern, dp, wdb->dbip, (matp_t)NULL );
	if( status < 0 ) {
		Tcl_AppendResult( interp, "rt_db_get_internal failure: ",
				  argv[1], (char *)NULL );
		return TCL_ERROR;
	}
	RT_CK_DB_INTERNAL( &intern );

       /* Find out what type of object we are dealing with and report on it. */
	id = intern.idb_type;
	stlp = rt_get_parsetab_by_id( id );
	if( stlp != NULL ) {
		bu_vls_strcat( &str, stlp->label );
		sp = stlp->parsetab;
		if( argc == 2 ) while( sp->sp_name != NULL ) {
			bu_vls_printf( &str, " %s ", sp->sp_name );
			if( sp->sp_count > 1 ) {
				bu_vls_putc( &str, '{' );
				bu_vls_struct_item( &str, sp,
						 (char *)intern.idb_ptr, ' ' );
				bu_vls_putc( &str, '}' );
			} else {
				bu_vls_struct_item( &str, sp,
						 (char *)intern.idb_ptr, ' ' );
			}
			++sp;
		} else {
			bu_vls_trunc( &str, 0 );
			if( bu_vls_struct_item_named( &str, sp, argv[2],
					 (char *)intern.idb_ptr, ' ') < 0 ) {
				Tcl_AppendResult( interp, "no such attribute",
						  (char *)NULL );
				rt_functab[id].ft_ifree( &intern );
				goto error;
			}
		}
	} else {
		if( id == ID_COMBINATION ) {
			bu_vls_free( &str );
			(void)db_tcl_comb_describe( interp,
		          (struct rt_comb_internal *)intern.idb_ptr, argv[2] );
			rt_functab[id].ft_ifree( &intern );
			return TCL_OK;
		}
	}

	rt_functab[id].ft_ifree( &intern );

	if( bu_vls_strlen(&str)==0 ) {
		Tcl_AppendResult( interp,
	 "an output routine for this data type has not yet been implemented",
				  (char *)NULL );
		goto error;
	}


	Tcl_AppendResult( interp, bu_vls_addr(&str), (char *)NULL );
	bu_vls_free( &str );
	return TCL_OK;

 error:
	bu_vls_free( &str );
	return TCL_ERROR;
}

/*
 *			B U _ S T R U C T P A R S E _ A R G V
 *
 * Support routine for db adjust and db put.  Much like bu_structparse routine,
 * but takes the arguments as lists, a more Tcl-friendly method.
 * Also knows about the Tcl result string, so it can make more informative
 * error messages.
 * XXX move to libbu/bu_tcl.c
 */

int
bu_structparse_argv( interp, argc, argv, desc, base )
Tcl_Interp			*interp;
int				 argc;
char			       **argv;
CONST struct bu_structparse	*desc;		/* structure description */
char				*base;		/* base addr of users struct */
{
	register char				*cp, *loc;
	register CONST struct bu_structparse	*sdp;
	register int				 i, j;
	struct bu_vls				 str;

	if( desc == (struct bu_structparse *)NULL ) {
		bu_log( "bu_structparse_argv: NULL desc pointer\n" );
		Tcl_AppendResult( interp, "NULL desc pointer", (char *)NULL );
		return TCL_ERROR;
	}

	/* Run through each of the attributes and their arguments. */

	bu_vls_init( &str );
	while( argc > 0 ) {
		/* Find the attribute which matches this argument. */
		for( sdp = desc; sdp->sp_name != NULL; sdp++ ) {
			if( strcmp(sdp->sp_name, *argv) != 0 )
				continue;

			/* if we get this far, we've got a name match
			 * with a name in the structure description
			 */

#if CRAY && !__STDC__
			loc = (char *)(base+((int)sdp->sp_offset*sizeof(int)));
#else
			loc = (char *)(base+((int)sdp->sp_offset));
#endif
			if( sdp->sp_fmt[0] != '%' ) {
				bu_log( "bu_structparse_argv: unknown format\n" );
				bu_vls_free( &str );
				Tcl_AppendResult( interp, "unknown format",
						  (char *)NULL );
				return TCL_ERROR;
			}

			--argc;
			++argv;

			switch( sdp->sp_fmt[1] )  {
			case 'c':
			case 's':
				/* copy the string, converting escaped
				 * double quotes to just double quotes
				 */
				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
			 "not enough values for \"%s\" argument: should be %d",
						       sdp->sp_name,
						       sdp->sp_count );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}
				for( i = j = 0;
				     j < sdp->sp_count && argv[0][i] != '\0';
				     loc[j++] = argv[0][i++] )
					;
				if( sdp->sp_count > 1 ) {
					loc[sdp->sp_count-1] = '\0';
					Tcl_AppendResult( interp,
							  sdp->sp_name, " ",
							  loc, " ",
							  (char *)NULL );
				} else {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str, "%s %c ",
						       sdp->sp_name, *loc );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
				}
				--argc;
				++argv;
				break;
			case 'i':
				bu_log(
			 "Error: %%i not implemented. Contact developers.\n" );
				Tcl_AppendResult( interp,
						  "%%i not implemented yet",
						  (char *)NULL );
				bu_vls_free( &str );
				return TCL_ERROR;
			case 'd': {
				register int *ip = (int *)loc;
				register int tmpi;
				register char CONST *cp;

				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
      "not enough values for \"%s\" argument: should have %d, only %d given",
						       sdp->sp_name,
						       sdp->sp_count, i );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}

				Tcl_AppendResult( interp, sdp->sp_name, " ",
						  (char *)NULL );

				/* Special case:  '=!' toggles a boolean */
				if( argv[0][0] == '!' ) {
					*ip = *ip ? 0 : 1;
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str, "%d ", *ip );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					++argv;
					--argc;
					break;
				}
				/* Normal case: an integer */
				cp = *argv;
				for( i = 0; i < sdp->sp_count; ++i ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
		      "not enough values for \"%s\" argument: should have %d",
							       sdp->sp_name,
							       sdp->sp_count );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}

					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;
			
					tmpi = atoi( cp );
					if( *cp && (*cp == '+' || *cp == '-') )
						cp++;
					while( *cp && isdigit(*cp) )
						cp++; 
					/* make sure we actually had an
					 * integer out there
					 */

					if( cp == *argv ||
					    (cp == *argv+1 &&
					     (argv[0][0] == '+' ||
					      argv[0][0] == '-')) ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
			       "value \"%s\" to argument %s isn't an integer",
							       argv,
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					} else {
						*(ip++) = tmpi;
					}
					/* Skip the separator(s) */
					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp ) 
						++cp;
				}
				Tcl_AppendResult( interp,
						  sdp->sp_count > 1 ? "{" : "",
						  argv[0],
						  sdp->sp_count > 1 ? "}" : "",
						  " ", (char *)NULL);
				--argc;
				++argv;
				break; }
			case 'f': {
				int		dot_seen;
				double		tmp_double;
				register double *dp;
				char		*numstart;

				dp = (double *)loc;

				if( argc < 1 ) {
					bu_vls_trunc( &str, 0 );
					bu_vls_printf( &str,
       "not enough values for \"%s\" argument: should have %d, only %d given",
						       sdp->sp_name,
						       sdp->sp_count, argc );
					Tcl_AppendResult( interp,
							  bu_vls_addr(&str),
							  (char *)NULL );
					bu_vls_free( &str );
					return TCL_ERROR;
				}

				Tcl_AppendResult( interp, sdp->sp_name, " ",
						  (char *)NULL );

				cp = *argv;
				for( i = 0; i < sdp->sp_count; i++ ) {
					if( *cp == '\0' ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str,
       "not enough values for \"%s\" argument: should have %d, only %d given",
							       sdp->sp_name,
							       sdp->sp_count,
							       i );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}
					
					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;

					numstart = cp;
					if( *cp == '-' || *cp == '+' ) cp++;

					/* skip matissa */
					dot_seen = 0;
					for( ; *cp ; cp++ ) {
						if( *cp == '.' && !dot_seen ) {
							dot_seen = 1;
							continue;
						}
						if( !isdigit(*cp) )
							break;
					}

					/* If no mantissa seen,
					   then there is no float here */
					if( cp == (numstart + dot_seen) ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
	                           "value \"%s\" to argument %s isn't a float",
							       argv[0],
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}

					/* there was a mantissa,
					   so we may have an exponent */
					if( *cp == 'E' || *cp == 'e' ) {
						cp++;

						/* skip exponent sign */
						if (*cp == '+' || *cp == '-')
							cp++;
						while( isdigit(*cp) )
							cp++;
					}

					bu_vls_trunc( &str, 0 );
					bu_vls_strcpy( &str, numstart );
					bu_vls_trunc( &str, cp-numstart );
					if( sscanf(bu_vls_addr(&str),
						   "%lf", &tmp_double) != 1 ) {
						bu_vls_trunc( &str, 0 );
						bu_vls_printf( &str, 
				  "value \"%s\" to argument %s isn't a float",
							       numstart,
							       sdp->sp_name );
						Tcl_AppendResult( interp,
							    bu_vls_addr(&str),
							    (char *)NULL );
						bu_vls_free( &str );
						return TCL_ERROR;
					}
					
					*dp++ = tmp_double;

					while( (*cp == ' ' || *cp == '\n' ||
						*cp == '\t') && *cp )
						++cp;
				}
				Tcl_AppendResult( interp,
						  sdp->sp_count > 1 ? "{" : "",
						  argv[0],
						  sdp->sp_count > 1 ? "}" : "",
						  " ", (char *)NULL );
				--argc;
				++argv;
				break; }
			default:
				Tcl_AppendResult( interp, "unknown format",
						  (char *)NULL );
				return TCL_ERROR;
			}
			break;
		}
		
		if( sdp->sp_name == NULL ) {
			bu_vls_trunc( &str, 0 );
			bu_vls_printf( &str, "invalid attribute %s", argv[0] );
			Tcl_AppendResult( interp, bu_vls_addr(&str),
					  (char *)NULL );
			bu_vls_free( &str );
			return TCL_ERROR;
		}
	}
	return TCL_OK;
}

/*
 *			R T _ D B _ P U T
 **
 ** Creates an object and stuffs it into the databse.
 ** All arguments must be specified.  Object cannot already exist.
 **/

int
rt_db_put(clientData, interp, argc, argv)
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    struct rt_db_internal intern;
    register struct rt_solid_type_lookup *stlp;
    register struct directory *dp;
    int status, ngran, id;
    char *newargv[3];
    struct rt_wdb	*wdb = (struct rt_wdb *)clientData;
    
    if (argc < 2) {
	Tcl_AppendResult(interp,
		      "wrong # args: should be db put objName objType attrs",
		      (char *)NULL);
	return TCL_ERROR;
    }
    
	/* XXX Verify that this wdb supports lookup operations (non-null dbip) */
	/* If not, just skip the lookup test and write the object */

    if (db_lookup(wdb->dbip, argv[1], LOOKUP_QUIET) != DIR_NULL) {
	Tcl_AppendResult(interp, argv[1], " already exists", (char *)NULL);
	return TCL_ERROR;
    }

    RT_INIT_DB_INTERNAL(&intern);

    stlp = rt_get_parsetab_by_name(argv[2]);
    if (stlp == NULL) {
	Tcl_AppendResult(interp, "unknown object type", (char *)NULL);
	return TCL_ERROR;
    }

    id = intern.idb_type = stlp->id;
    intern.idb_ptr = bu_malloc(stlp->db_internal_size, "rt_db_put");
    *((long *)intern.idb_ptr) = stlp->magic;
    if (bu_structparse_argv(interp, argc-3, argv+3, stlp->parsetab,
			 (char *)intern.idb_ptr) == TCL_ERROR) {
	rt_db_free_internal(&intern);
	return TCL_ERROR;
    }

	if( wdb_export( wdb, argv[1], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult(interp, "wdb_export(", argv[1], ") failure\n", (char *)NULL);
		rt_db_free_internal(&intern);
		return TCL_ERROR;
	}
	rt_db_free_internal(&intern);
	return TCL_OK;
}

/*
 *			R T _ D B _ A D J U S T
 *
 **
 ** For use with Tcl, this routine accepts as its first argument, an item in
 ** the database; as its remaining arguments it takes the properties that
 ** need to be changed and their values.
 **/

int
rt_db_adjust( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    register struct directory *dp;
    register struct bu_structparse *sp = NULL;
    int id, status;
    register int i;
    struct rt_db_internal intern;
    mat_t idn;
    char objecttype;
    struct rt_wdb	*wdb = (struct rt_wdb *)clientData;

    if( argc < 4 ) {
	Tcl_AppendResult(interp,
                 "wrong # args: should be \"db adjust objName attr value...\"",
		 (char *)NULL);
	return TCL_ERROR;
    }

	/* XXX Verify that this wdb supports lookup operations (non-null dbip) */

    dp = db_lookup(wdb->dbip, argv[1], LOOKUP_QUIET);
    if (dp == DIR_NULL) {
	Tcl_AppendResult(interp, argv[1], ": not found\n", (char *)NULL);
	return TCL_ERROR;
    }

    status = rt_db_get_internal(&intern, dp, wdb->dbip, (matp_t)NULL );
    if (status < 0) {
	Tcl_AppendResult(interp, "rt_db_get_internal(", argv[1], ") failure\n", (char *)NULL);
	return TCL_ERROR;
    }
    RT_CK_DB_INTERNAL(&intern);

    /* Find out what type of object we are dealing with and tweak it. */
    id = intern.idb_type;

    if (rt_get_parsetab_by_id(id) == NULL ||
	(sp = rt_get_parsetab_by_id(id)->parsetab) == NULL) {
	    Tcl_AppendResult(interp, "manipulation routines for this type \
			     have not yet been implemented", (char *)NULL);
	    rt_db_free_internal(&intern);
	    return TCL_ERROR;
    } else {
	    /* If we were able to find an entry in on the "cheat sheet", just
	       use the handy parse functions to return the object. */

	    if (bu_structparse_argv(interp, argc-2, argv+2, sp,
				    (char *)intern.idb_ptr) == TCL_ERROR) {
		    rt_db_free_internal(&intern);
		    return TCL_ERROR;
	    }

	    if( wdb_export( wdb, argv[1], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
		    Tcl_AppendResult(interp, "wdb_export(", argv[1], ") failure\n", (char *)NULL);
		    rt_db_free_internal(&intern);
		    return TCL_ERROR;
	    }
	    rt_db_free_internal(&intern);
	    return TCL_OK;
    }
}

/*
 *			R T _ D B _ F O R M
 */
int
rt_db_form( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
    register struct bu_structparse *sp = NULL;
    register struct rt_solid_type_lookup *stlp;
    register int i;
    struct bu_vls str;
    register char *cp;

    if (argc != 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"db form objType\"",
		      (char *)NULL);
	return TCL_ERROR;
    }

    bu_vls_init(&str);
    sp = rt_get_parsetab_by_name(argv[1]) == NULL ? NULL :
	    rt_get_parsetab_by_name(argv[1])->parsetab;
    
    if (sp != NULL)
	while (sp->sp_name != NULL) {
	    Tcl_AppendElement(interp, sp->sp_name);
	    bu_vls_trunc(&str, 0);
	    if (strcmp(sp->sp_fmt, "%c") == 0 ||
		strcmp(sp->sp_fmt, "%s") == 0) {
		if (sp->sp_count > 1)
		    bu_vls_printf(&str, "%%%ds", sp->sp_count);
		else
		    bu_vls_printf(&str, "%%c");
	    } else {
		bu_vls_printf(&str, "%s", sp->sp_fmt);
		for (i = 1; i < sp->sp_count; i++)
		    bu_vls_printf(&str, " %s", sp->sp_fmt);
	    }
	    Tcl_AppendElement(interp, bu_vls_addr(&str));
	    ++sp;
	}
    else {
	Tcl_AppendResult(interp, "a form routine for this data type has not \
yet been implemented", (char *)NULL);
	goto error;
    }

    bu_vls_free(&str);
    return TCL_OK;

 error:
    bu_vls_free(&str);
    return TCL_ERROR;
}




/*
 *			R T _ T C L _ S E T U P
 *
 *  Add all the supported Tcl interfaces to LIBRT routines to
 *  the list of commands known by the given interpreter.
 */
void
rt_tcl_setup(interp)
Tcl_Interp *interp;
{
	/* XXX All these are temporary hacks, don't get dependent on
	 * XXX them just yet.  -Mike.
	 */

	(void)Tcl_CreateCommand(interp, "wdb_open", wdb_open,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	(void)Tcl_CreateCommand(interp, "rt_wdb_inmem_rgb", rt_wdb_inmem_rgb,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
 	(void)Tcl_CreateCommand(interp, "rt_wdb_inmem_shader", rt_wdb_inmem_shader,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_SetVar(interp, "rt_version", (char *)rt_version+5, TCL_GLOBAL_ONLY);
}
