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
#include "raytrace.h"
#include "externs.h"

#define RT_CK_DBI_TCL(_p)	BU_CKMAG_TCL(interp,_p,DBI_MAGIC,"struct db_i")


/*
 *  Macros to check and validate a structure pointer, given that
 *  the first entry in the structure is a magic number.
 */
#define BU_CKMAG_TCL(_interp, _ptr, _magic, _str)	\
	if( !(_ptr) || *((long *)(_ptr)) != (_magic) )  { \
		bu_badmagic_tcl( (_interp), (long *)(_ptr), _magic, _str, __FILE__, __LINE__ ); \
		return TCL_ERROR; \
	}

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

static struct rt_wdb *mike_wdb;

#define WDB_TYPE_DB_INMEM		3
extern struct rt_wdb *wdb_dbopen();

/*
 *			R T _ T C L _ G E T _ C O M B
 */
struct rt_comb_internal *
rt_tcl_get_comb(ip, interp, dbi_str, name)
struct rt_db_internal	*ip;
Tcl_Interp		*interp;
CONST char		*dbi_str;
CONST char		*name;
{
	struct directory	*dp;
	struct db_i		*dbip;
	struct rt_comb_internal	*comb;

	sscanf( dbi_str, "%d", &dbip );
	/* This can still dump core if it's an unmapped address */
	/* RT_CK_DBI_TCL(dbip) */
	if( !dbip || *((long *)dbip) != DBI_MAGIC )  {
		bu_badmagic_tcl(interp, (long *)dbip, DBI_MAGIC, "struct db_i", __FILE__, __LINE__);
		return NULL;
	}

	if( !mike_wdb )  {
		if( !(mike_wdb = wdb_dbopen( dbip, WDB_TYPE_DB_INMEM )) )  {
			Tcl_AppendResult(interp, "wdb_dbopen failed\n", NULL);
			return NULL;
		}
		/* Prevent accidents */
		if( !dbip->dbi_read_only )  {
			Tcl_AppendResult(interp, "(database changed to read-only)\n", NULL);
			dbip->dbi_read_only = 1;
		}
	}
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
 *			R T _ D I R E C T C H A N G E _ R G B
 *
 *  Go poke the rgb values of a region, on the fly.
 *  This does not update the inmemory database, and will vanish on next re-prep.
 */
rt_directchange_rgb( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	if( argc != 6 )  {
		Tcl_AppendResult(interp, "Usage: rt_directchange_rgb $rtip comb r g b\n", NULL);
		return TCL_ERROR;
	}

	/* Validate rtip */
	/* Ensure rti has been prepped */
	/* Find all region names which match /comb/ pattern */
	/* Modify their color */

#if 0
	/* Make mods to comb here */
	comb->rgb[0] = atoi(argv[3+0]);
	comb->rgb[1] = atoi(argv[3+1]);
	comb->rgb[2] = atoi(argv[3+2]);
#endif

	return TCL_OK;
}

/*
 *			R T _ W D B _ I N M E M _ R G B
 */
rt_wdb_inmem_rgb( clientData, interp, argc, argv )
ClientData clientData;
Tcl_Interp *interp;
int argc;
char **argv;
{
	struct rt_db_internal	intern;
	struct rt_comb_internal	*comb;

	if( argc != 6 )  {
		Tcl_AppendResult(interp, "Usage: rt_wdb_inmem_rgb $dbip comb r g b\n", NULL);
		return TCL_ERROR;
	}

	if( !(comb = rt_tcl_get_comb( &intern, interp, argv[1], argv[2] )) )
		return TCL_ERROR;

	/* Make mods to comb here */
	comb->rgb[0] = atoi(argv[3+0]);
	comb->rgb[1] = atoi(argv[3+1]);
	comb->rgb[2] = atoi(argv[3+2]);

	if( wdb_export( mike_wdb, argv[2], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
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

	if( argc < 4 )  {
		Tcl_AppendResult(interp, "Usage: rt_wdb_inmem_shader $dbip comb shader [params]\n", NULL);
		return TCL_ERROR;
	}

	if( !(comb = rt_tcl_get_comb( &intern, interp, argv[1], argv[2] )) )
		return TCL_ERROR;

	/* Make mods to comb here */
	bu_vls_trunc( &comb->shader, 0 );
	bu_vls_from_argv( &comb->shader, argc-3, &argv[3] );

	if( wdb_export( mike_wdb, argv[2], intern.idb_ptr, intern.idb_type, 1.0 ) < 0 )  {
		Tcl_AppendResult(interp, "wdb_export ", argv[2], " failure\n", NULL);
		rt_db_free_internal( &intern );
		return TCL_ERROR;
	}
	rt_db_free_internal( &intern );
	return TCL_OK;
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
	(void)Tcl_CreateCommand(interp, "rt_directchange_rgb", rt_directchange_rgb,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "rt_wdb_inmem_rgb", rt_wdb_inmem_rgb,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);
	(void)Tcl_CreateCommand(interp, "rt_wdb_inmem_shader", rt_wdb_inmem_shader,
		(ClientData)NULL, (Tcl_CmdDeleteProc *)NULL);

	Tcl_SetVar(interp, "rt_version", (char *)rt_version+5, TCL_GLOBAL_ONLY);
}
