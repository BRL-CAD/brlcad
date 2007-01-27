/*                           D I R . C
 * BRL-CAD
 *
 * Copyright (c) 1985-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup dbio */
/** @{ */
/** @file dir.c
 *
 * Ray Tracing program, GED database directory manager.
 *
 *  Functions -
 *	rt_dirbuild	Read GED database, build directory
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium, Bldg 394
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005
 *
 */

#ifndef lint
static const char RCSdir[] = "@(#)$Header$";
#endif

#include "common.h"


#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ D I R B U I L D
 *
 *  Builds a directory of the object names.
 *
 *  Allocate and initialize information for this
 *  instance of an RT model database.
 *
 * Returns -
 *	(struct rt_i *)	Success
 *	RTI_NULL	Fatal Error
 */
struct rt_i *
rt_dirbuild( const char *filename, char *buf, int len )
{
	register struct rt_i	*rtip;
	register struct db_i	*dbip;		/* Database instance ptr */

	if( rt_uniresource.re_magic == 0 )
		rt_init_resource( &rt_uniresource, 0, NULL );

	if( (dbip = db_open( filename, "r" )) == DBI_NULL )
		return( RTI_NULL );		/* FAIL */
	RT_CK_DBI(dbip);

	if( db_dirbuild( dbip ) < 0 )  {
		db_close( dbip );
		return RTI_NULL;		/* FAIL */
	}

	rtip = rt_new_rti( dbip );		/* clones dbip */
	db_close(dbip);				/* releases original dbip */

	if( buf != (char *)NULL )
		strncpy( buf, dbip->dbi_title, len );

	return( rtip );				/* OK */
}

/*
 *			R T _ D B _ G E T _ I N T E R N A L
 *
 *  Get an object from the database, and convert it into it's internal
 *  representation.
 *
 *  Returns -
 *	<0	On error
 *	id	On success.
 */
int
rt_db_get_internal(
	struct rt_db_internal	*ip,
	const struct directory	*dp,
	const struct db_i	*dbip,
	const mat_t		mat,
	struct resource		*resp)
{
	struct bu_external	ext;
	register int		id;

	BU_INIT_EXTERNAL(&ext);
	RT_INIT_DB_INTERNAL(ip);

	if( dbip->dbi_version > 4 )
		return  rt_db_get_internal5( ip, dp, dbip, mat, resp );

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	if( dp->d_flags & DIR_COMB )  {
		id = ID_COMBINATION;
	} else {
		/* As a convenience to older ft_import routines */
		if( mat == NULL )  mat = bn_mat_identity;
		id = rt_id_solid( &ext );
	}

	/* ip is already initialized and should not be re-initialized */
	if( rt_functab[id].ft_import( ip, &ext, mat, dbip, resp ) < 0 )  {
		bu_log("rt_db_get_internal(%s):  import failure\n",
			dp->d_namep );
		rt_db_free_internal( ip, resp );
		bu_free_external( &ext );
		return -1;		/* FAIL */
	}
	bu_free_external( &ext );
	RT_CK_DB_INTERNAL( ip );
	ip->idb_meth = &rt_functab[id];

	/* prior to version 5, there are no attributes */
	bu_avs_init_empty( &ip->idb_avs );

	return id;			/* OK */
}

/*
 *			R T _ D B _ P U T _ I N T E R N A L
 *
 *  Convert the internal representation of a solid to the external one,
 *  and write it into the database.
 *  On success only, the internal representation is freed.
 *
 *  Returns -
 *	<0	error
 *	 0	success
 */
int
rt_db_put_internal(
	struct directory	*dp,
	struct db_i		*dbip,
	struct rt_db_internal	*ip,
	struct resource		*resp)
{
	struct bu_external	ext;
	int			ret;

	BU_INIT_EXTERNAL(&ext);
	RT_CK_DB_INTERNAL( ip );

	if( dbip->dbi_version > 4 )
		return  rt_db_put_internal5( dp, dbip, ip, resp,
		    DB5_MAJORTYPE_BRLCAD );

	/* Scale change on export is 1.0 -- no change */
	ret = ip->idb_meth->ft_export( &ext, ip, 1.0, dbip, resp );
	if( ret < 0 )  {
		bu_log("rt_db_put_internal(%s):  solid export failure\n",
			dp->d_namep);
		rt_db_free_internal( ip, resp );
		bu_free_external( &ext );
		return -2;		/* FAIL */
	}
	rt_db_free_internal( ip, resp );

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		bu_free_external( &ext );
		return -1;		/* FAIL */
	}

	bu_free_external( &ext );
	return 0;			/* OK */
}

/*
 *			R T _ F W R I T E _ I N T E R N A L
 *
 *  Put an object in internal format out onto a file in external format.
 *  Used by LIBWDB.
 *
 *  Can't really require a dbip parameter, as many callers won't have one.
 *
 *  Returns -
 *	0	OK
 *	<0	error
 */
int
rt_fwrite_internal(
	FILE *fp,
	const char *name,
	const struct rt_db_internal *ip,
	double conv2mm )
{
	struct bu_external	ext;

	RT_CK_DB_INTERNAL(ip);
	RT_CK_FUNCTAB( ip->idb_meth );
	BU_INIT_EXTERNAL( &ext );

	if( ip->idb_meth->ft_export( &ext, ip, conv2mm, NULL /*dbip*/, &rt_uniresource ) < 0 )  {
		bu_log("rt_file_put_internal(%s): solid export failure\n",
			name );
		bu_free_external( &ext );
		return(-2);				/* FAIL */
	}
	BU_CK_EXTERNAL( &ext );

	if( db_fwrite_external( fp, name, &ext ) < 0 )  {
		bu_log("rt_fwrite_internal(%s): db_fwrite_external() error\n",
			name );
		bu_free_external( &ext );
		return(-3);
	}
	bu_free_external( &ext );
	return(0);

}

/*
 *			R T _ D B _ F R E E _ I N T E R N A L
 */
void
rt_db_free_internal( struct rt_db_internal *ip, struct resource *resp )
{
	RT_CK_DB_INTERNAL( ip );

	/* meth is not required since may be asked to free something
	 * that was never set.
	 */
	if (ip->idb_meth) {
	    RT_CK_FUNCTAB(ip->idb_meth);
	    if (ip->idb_ptr) {
		ip->idb_meth->ft_ifree(ip, resp);
	    }
	}

	/* resp is not checked, since most ifree's don't take/need it
	 * (only combinations use it) -- leave it up to ft_ifree to check it
	 */
	if( ip->idb_ptr )  {
	    ip->idb_ptr = NULL;		/* sanity.  Should be handled by INIT, below */
	}
	if( ip->idb_avs.magic == BU_AVS_MAGIC ) {
	    bu_avs_free(&ip->idb_avs);
	}
	RT_INIT_DB_INTERNAL(ip);
}

/*
 *		R T _ D B _ L O O K U P _ I N T E R N A L
 *
 *	    Convert an object name to a rt_db_internal pointer
 *
 *	Looks up the named object in the directory of the specified model,
 *	obtaining a directory pointer.  Then gets that object from the
 *	database and constructs its internal representation.  Returns
 *	ID_NULL on error, otherwise returns the type of the object.
 */
int
rt_db_lookup_internal (
	struct db_i *dbip,
	const char *obj_name,
	struct directory **dpp,
	struct rt_db_internal *ip,
	int noisy,
	struct resource *resp)
{
    struct directory		*dp;

    if (obj_name == (char *) 0)
    {
	if (noisy == LOOKUP_NOISY)
	    bu_log("rt_db_lookup_internal() No object specified\n");
	return ID_NULL;
    }
    if ((dp = db_lookup(dbip, obj_name, noisy)) == DIR_NULL)
	return ID_NULL;
    if (rt_db_get_internal(ip, dp, dbip, (matp_t) NULL, resp ) < 0 )
    {
	if (noisy == LOOKUP_NOISY)
	    bu_log("rt_db_lookup_internal() Failed to get internal form of object '%s'\n",
		dp -> d_namep);
	return ID_NULL;
    }

    *dpp = dp;
    return (ip -> idb_type);
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
