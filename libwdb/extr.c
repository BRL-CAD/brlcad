/*
 *			E X T R . C
 *
 * Support for extrusion solids
 *
 *  Author -
 *	John Anderson
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
 *	This software is Copyright (C) 2000 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static const char extr_RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "machine.h"
#include "bu.h"
#include "db.h"
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"

int
mk_extrusion( fp, name, sketch_name, V, h, u_vec, v_vec, keypoint )
FILE *fp;
char *name, *sketch_name;
point_t V;
vect_t h, u_vec, v_vec;
{
	struct rt_db_internal intern;
	struct rt_extrude_internal *extr;

	BU_GETSTRUCT( extr, rt_extrude_internal );
	extr->magic = RT_EXTRUDE_INTERNAL_MAGIC;
	NAMEMOVE( sketch_name, extr->sketch_name );
	VMOVE( extr->V, V );
	VMOVE( extr->h, h );
	VMOVE( extr->u_vec, u_vec );
	VMOVE( extr->v_vec, v_vec );
	extr->keypoint = keypoint;
	extr->skt = (struct rt_sketch_internal *)NULL;

	RT_INIT_DB_INTERNAL( &intern );
	intern.idb_ptr = (genptr_t)extr;
	intern.idb_type = ID_EXTRUDE;
	intern.idb_meth = &rt_functab[ID_EXTRUDE];

	return mk_fwrite_internal( fp, name, &intern );
}
