/*
 *			E X P O R T . C
 *
 *  Routines to allow libwdb to use librt's import/export interface,
 *  rather than having to know about the database formats directly.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimitied.
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"

/*
 *  Applies scalling (units change) as specified in mk_conv2mm.
 *  The caller must free "gp".
 *
 *  Returns -
 *	 0	OK
 *	<0	error
 */
int
mk_export_fwrite( fp, name, gp, id )
FILE		*fp;
CONST char	*name;
genptr_t	gp;
int		id;
{
	struct rt_db_internal	intern;

	if( (id <= 0 || id > ID_MAXIMUM) && id != ID_COMBINATION )  {
		bu_log("mk_export_fwrite(%s): id=%d bad\n",
			name, id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_type = id;
	intern.idb_meth = &rt_functab[id];
	intern.idb_ptr = gp;

	if( rt_fwrite_internal( fp, name, &intern, mk_conv2mm ) < 0 )  {
		bu_log("mk_export_fwrite(%s): rt_fwrite_internal failure\n",
			name );
		return(-2);				/* FAIL */
	}
	return 0;					/* OK */
}
