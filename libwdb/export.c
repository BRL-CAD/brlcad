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
static char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
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
char		*name;
genptr_t	gp;
int		id;
{
	struct rt_db_internal	intern;
	struct rt_external	ext;
	union record		*rec;

	if( (id <= 0 || id > ID_MAXIMUM) && id != ID_COMBINATION )  {
		rt_log("mk_export_fwrite(%s): id=%d bad\n",
			name, id );
		return(-1);
	}

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_type = id;
	intern.idb_ptr = gp;

	/* Scale change on export is from global "mk_conv2mm" */
	if( rt_functab[id].ft_export( &ext, &intern, mk_conv2mm ) < 0 )  {
		rt_log("mk_export_fwrite(%s): solid export failure\n",
			name );
		db_free_external( &ext );
		return(-2);				/* FAIL */
	}
	RT_CK_EXTERNAL( &ext );

	/* Depends on solid names always being in the same place */
	rec = (union record *)ext.ext_buf;
	NAMEMOVE( name, rec->s.s_name );

	if( fwrite( ext.ext_buf, ext.ext_nbytes, 1, fp ) != 1 )  {
		rt_log("mk_export_fwrite(%s): fwrite error\n",
			name );
		db_free_external( &ext );
		return(-3);
	}
	db_free_external( &ext );
	return(0);
}
