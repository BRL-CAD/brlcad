/*
 *			D I R . C
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
 *  Copyright Notice -
 *	This software is Copyright (C) 1985 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static char RCSdir[] = "@(#)$Header$";
#endif

#include "conf.h"

#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "./debug.h"

/*
 *			R T _ N E W _ R T I
 *
 *  Given a db_i database instance, create an rt_i instance.
 *
 *  XXX Perhaps the db_i structure should be reference counted?
 */
struct rt_i *
rt_new_rti( dbip )
struct db_i	*dbip;
{
	register struct rt_i	*rtip;
	register int		i;

	RT_CK_DBI( dbip );

	BU_GETSTRUCT( rtip, rt_i );
	rtip->rti_magic = RTI_MAGIC;
	for( i=0; i < RT_DBNHASH; i++ )  {
		BU_LIST_INIT( &(rtip->rti_solidheads[i]) );
	}
	rtip->rti_dbip = dbip;
	rtip->needprep = 1;

	VSETALL( rtip->mdl_min,  INFINITY );
	VSETALL( rtip->mdl_max, -INFINITY );
	VSETALL( rtip->rti_inf_box.bn.bn_min, -0.1 );
	VSETALL( rtip->rti_inf_box.bn.bn_max,  0.1 );
	rtip->rti_inf_box.bn.bn_type = CUT_BOXNODE;

	/* XXX These need to be improved */
	rtip->rti_tol.magic = BN_TOL_MAGIC;
	rtip->rti_tol.dist = 0.005;
	rtip->rti_tol.dist_sq = rtip->rti_tol.dist * rtip->rti_tol.dist;
	rtip->rti_tol.perp = 1e-6;
	rtip->rti_tol.para = 1 - rtip->rti_tol.perp;

	rtip->rti_space_partition = RT_PART_NUBSPT;
	rtip->rti_nugrid_dimlimit = 0;
	rtip->rti_nu_gfactor = RT_NU_GFACTOR_DEFAULT;

	return rtip;
}

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
rt_dirbuild(filename, buf, len)
char	*filename;
char	*buf;
int	len;
{
	register struct rt_i	*rtip;
	register struct db_i	*dbip;		/* Database instance ptr */

	if( BU_LIST_FIRST( rt_list, &rt_g.rtg_vlfree ) == 0 )  {
		BU_LIST_INIT( &rt_g.rtg_vlfree );
	}

	if( (dbip = db_open( filename, "r" )) == DBI_NULL )
	    	return( RTI_NULL );		/* FAIL */
	RT_CK_DBI(dbip);

	if( db_scan( dbip, (int (*)())db_diradd, 1 ) < 0 )
	    	return( RTI_NULL );		/* FAIL */

	rtip = rt_new_rti( dbip );

	if( buf != (char *)NULL )
		strncpy( buf, dbip->dbi_title, len );

	return( rtip );				/* OK */
}

/*
 *			R T _ F R E E _ R T I
 *
 *  Release all the dynamic storage acquired by rt_dirbuild() and
 *  any subsequent ray-tracing operations.
 *
 *  Note that any PARALLEL resource structures have to be freed separately.
 *  Note that the rt_g structure needs to be cleaned separately.
 */
void
rt_free_rti( rtip )
struct rt_i	*rtip;
{
	RT_CK_RTI(rtip);

	rt_clean( rtip );
	db_close( rtip->rti_dbip );
	rtip->rti_dbip = (struct db_i *)NULL;
	rt_free( (char *)rtip, "struct rt_i" );
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
rt_db_get_internal( ip, dp, dbip, mat )
struct rt_db_internal	*ip;
struct directory	*dp;
struct db_i		*dbip;
CONST mat_t		mat;
{
	struct bu_external	ext;
	register int		id;

	BU_INIT_EXTERNAL(&ext);
	RT_INIT_DB_INTERNAL(ip);
	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	if( dp->d_flags & DIR_COMB )  {
		id = ID_COMBINATION;
	} else {
		/* As a convenience to older ft_import routines */
		if( mat == NULL )  mat = bn_mat_identity;
		id = rt_id_solid( &ext );
	}

	if( rt_functab[id].ft_import( ip, &ext, mat, dbip ) < 0 )  {
		bu_log("rt_db_get_internal(%s):  import failure\n",
			dp->d_namep );
	    	if( ip->idb_ptr )  rt_functab[id].ft_ifree( ip );
		db_free_external( &ext );
		return -1;		/* FAIL */
	}
	db_free_external( &ext );
	RT_CK_DB_INTERNAL( ip );
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
rt_db_put_internal( dp, dbip, ip )
struct rt_db_internal	*ip;
struct directory	*dp;
struct db_i		*dbip;
{
	struct bu_external	ext;
	int			ret;

	BU_INIT_EXTERNAL(&ext);
	RT_CK_DB_INTERNAL( ip );

	/* Scale change on export is 1.0 -- no change */
	ret = rt_functab[ip->idb_type].ft_export( &ext, ip, 1.0, dbip );
	if( ret < 0 )  {
		bu_log("rt_db_put_internal(%s):  solid export failure\n",
			dp->d_namep);
		db_free_external( &ext );
		return -2;		/* FAIL */
	}

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		db_free_external( &ext );
		return -1;		/* FAIL */
	}

    	if( ip->idb_ptr )  rt_functab[ip->idb_type].ft_ifree( ip );

	RT_INIT_DB_INTERNAL(ip);
	db_free_external( &ext );
	return 0;			/* OK */
}

/*
 *			R T _ D B _ F R E E _ I N T E R N A L
 */
void
rt_db_free_internal( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL( ip );
    	if( ip->idb_ptr )  rt_functab[ip->idb_type].ft_ifree( ip );
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
rt_db_lookup_internal (dbip, obj_name, dpp, ip, noisy)

struct db_i		*dbip;
char			*obj_name;
struct directory	**dpp;
struct rt_db_internal	*ip;
int			noisy;

{
    struct directory		*dp;

    if (obj_name == (char *) 0)
    {
	if (noisy == LOOKUP_NOISY)
	    bu_log("No object specified\n");
	return ID_NULL;
    }
    if ((dp = db_lookup(dbip, obj_name, noisy)) == DIR_NULL)
	return ID_NULL;
    if (rt_db_get_internal(ip, dp, dbip, (matp_t) NULL ) < 0 )
    {
	if (noisy == LOOKUP_NOISY)
	    bu_log("Failed to get internal form of object '%s'\n",
		dp -> d_namep);
	return ID_NULL;
    }

    *dpp = dp;
    return (ip -> idb_type);
}
