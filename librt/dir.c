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
CONST char	*filename;
char		*buf;
int		len;
{
	register struct rt_i	*rtip;
	register struct db_i	*dbip;		/* Database instance ptr */

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
rt_db_get_internal( ip, dp, dbip, mat )
struct rt_db_internal	*ip;
CONST struct directory	*dp;
CONST struct db_i	*dbip;
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
	    	if( ip->idb_ptr )  ip->idb_meth->ft_ifree( ip );
		db_free_external( &ext );
		return -1;		/* FAIL */
	}
	db_free_external( &ext );
	RT_CK_DB_INTERNAL( ip );
	ip->idb_meth = &rt_functab[id];
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
struct directory	*dp;
struct db_i		*dbip;
struct rt_db_internal	*ip;
{
	struct bu_external	ext;
	int			ret;

	BU_INIT_EXTERNAL(&ext);
	RT_CK_DB_INTERNAL( ip );

	/* Scale change on export is 1.0 -- no change */
	ret = ip->idb_meth->ft_export( &ext, ip, 1.0, dbip );
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

    	if( ip->idb_ptr )  ip->idb_meth->ft_ifree( ip );

	RT_INIT_DB_INTERNAL(ip);
	db_free_external( &ext );
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
rt_fwrite_internal( fp, name, ip, conv2mm )
FILE				*fp;
CONST char			*name;
CONST struct rt_db_internal	*ip;
double				conv2mm;
{
	struct bu_external	ext;

	RT_CK_DB_INTERNAL(ip);
	RT_CK_FUNCTAB( ip->idb_meth );

	if( ip->idb_meth->ft_export( &ext, ip, conv2mm, NULL /*dbip*/ ) < 0 )  {
		bu_log("rt_file_put_internal(%s): solid export failure\n",
			name );
		db_free_external( &ext );
		return(-2);				/* FAIL */
	}
	BU_CK_EXTERNAL( &ext );

	if( db_fwrite_external( fp, name, &ext ) < 0 )  {
		bu_log("rt_fwrite_internal(%s): db_fwrite_external() error\n",
			name );
		db_free_external( &ext );
		return(-3);
	}
	db_free_external( &ext );
	return(0);

}

/*
 *			R T _ D B _ F R E E _ I N T E R N A L
 */
void
rt_db_free_internal( ip )
struct rt_db_internal	*ip;
{
	RT_CK_DB_INTERNAL( ip );
	RT_CK_FUNCTAB( ip->idb_meth );
    	if( ip->idb_ptr )  ip->idb_meth->ft_ifree( ip );
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
