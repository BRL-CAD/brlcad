/*
 *			S T R S O L . C
 *
 *  Library for writing strsol solids to MGED databases.
 *  Assumes that some of the structure of such databases are known
 *  by the calling routines.
 *
 *
 *  Note that routines which are passed point_t or vect_t or mat_t
 *  parameters (which are call-by-address) must be VERY careful to
 *  leave those parameters unmodified (eg, by scaling), so that the
 *  calling routine is not surprised.
 *
 *  Return codes of 0 are OK, -1 signal an error.
 *
 *  Authors -
 *	John R. Anderson
 *  
 *  Source -
 *	BVLD/VMB Advanced Computer Systems Team
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Pacakge" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1994 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
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
#include "vmath.h"
#include "bn.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "wdb.h"
#include "db.h"

int
mk_dsp( fp, name, file, xdim, ydim, mat )
struct rt_wdb		*fp;
CONST char	*name;
CONST char	*file;		/* name of file containing elevation data */
int		xdim;		/* X dimension of file (w cells) */
int		ydim;		/* Y dimension of file (n cells) */
CONST matp_t	mat;		/* convert solid coords to model space */
{
	struct rt_dsp_internal *dsp;
	
	BU_GETSTRUCT( dsp, rt_dsp_internal );
	dsp->magic = RT_DSP_INTERNAL_MAGIC;
	bu_vls_init( &dsp->dsp_name );
	bu_vls_strcpy( &dsp->dsp_name, "file:");
	bu_vls_strcat( &dsp->dsp_name, file);

	dsp->dsp_xcnt = xdim;
	dsp->dsp_ycnt = ydim;
	mat_copy( dsp->dsp_stom, mat );

	return wdb_export( fp, name, (genptr_t)dsp, ID_DSP, mk_conv2mm );
}

/*
 *			M K _ E B M
 */
int
mk_ebm( fp, name, file, xdim, ydim, tallness, mat )
struct rt_wdb		*fp;
CONST char	*name;
CONST char	*file;		/* name of file containing bitmap */
int		xdim;		/* X dimansion of file (w cells) */
int		ydim;		/* Y dimension of file (n cells) */
fastf_t		tallness;	/* Z extrusion height (mm) */
CONST matp_t	mat;		/* convert local coords to model space */
{
	struct rt_ebm_internal	*ebm;

	BU_GETSTRUCT( ebm, rt_ebm_internal );
	ebm->magic = RT_EBM_INTERNAL_MAGIC;
	strncpy( ebm->file, file, RT_EBM_NAME_LEN );
	ebm->xdim = xdim;
	ebm->ydim = ydim;
	ebm->tallness = tallness;
	mat_copy( ebm->mat , mat );

	return wdb_export( fp, name, (genptr_t)ebm, ID_EBM, mk_conv2mm );
}

/*
 *			M K _ V O L
 */
int
mk_vol( fp, name, file, xdim, ydim, zdim, lo, hi, cellsize, mat )
struct rt_wdb		*fp;
CONST char	*name;
CONST char	*file;		/* name of file containing bitmap */
int		xdim;		/* X dimansion of file (w cells) */
int		ydim;		/* Y dimension of file (n cells) */
int		zdim;		/* Z dimension of file (d cells) */
int		lo;		/* Low threshold */
int		hi;		/* High threshold */
CONST vect_t	cellsize;	/* ideal coords: size of each cell */
CONST matp_t	mat;		/* convert local coords to model space */
{
	struct rt_vol_internal	*vol;

	BU_GETSTRUCT( vol, rt_vol_internal );
	vol->magic = RT_VOL_INTERNAL_MAGIC;
	strncpy( vol->file, file, RT_VOL_NAME_LEN );
	vol->xdim = xdim;
	vol->ydim = ydim;
	vol->zdim = zdim;
	vol->lo = lo;
	vol->hi = hi;
	VMOVE( vol->cellsize , cellsize );
	mat_copy( vol->mat , mat );

	return wdb_export( fp, name, (genptr_t)vol, ID_VOL, mk_conv2mm );
}

#if 0
/*
 *			M K _ S T R S O L
 *
 *  This routine is not intended for general use.
 *  It exists primarily to support the converter ASC2G and to
 *  permit the rapid development of new string solids.
 */
int
mk_strsol( fp, name, string_solid, string_arg )
FILE		*fp;
CONST char	*name;
CONST char	*string_solid;
CONST char	*string_arg;
{
	union record	rec[DB_SS_NGRAN];

	BU_ASSERT_LONG( mk_version, <=, 4 );

	bzero( (char *)rec, sizeof(rec) );
	rec[0].ss.ss_id = DBID_STRSOL;
	NAMEMOVE( name, rec[0].ss.ss_name );
	strncpy( rec[0].ss.ss_keyword, string_solid, sizeof(rec[0].ss.ss_keyword)-1 );
	strncpy( rec[0].ss.ss_args, string_arg, DB_SS_LEN-1 );

	if( fwrite( (char *)rec, sizeof(rec), 1, fp ) != 1 )
		return -1;
	return 0;
}
#endif

/*
 *			M K _ S U B M O D E L
 *
 *  Create a submodel solid.
 *  If file is NULL or "", the treetop refers to the current database.
 *  Treetop is the name of a single database object in 'file'.
 *  meth is 0 (RT_PART_NUBSPT) or 1 (RT_PART_NUGRID).
 *  method 0 is what is normally used.
 */
int
mk_submodel( fp, name, file, treetop, meth )
struct rt_wdb		*fp;
CONST char	*name;
CONST char	*file;
CONST char	*treetop;
int		meth;
{
	struct rt_submodel_internal *in;

	BU_GETSTRUCT( in, rt_submodel_internal );
	in->magic = RT_SUBMODEL_INTERNAL_MAGIC;
	bu_vls_init( &in->file );
	if( file )  bu_vls_strcpy( &in->file, file );
	bu_vls_init( &in->treetop );
	bu_vls_strcpy( &in->treetop, treetop );
	in->meth = meth;

	return wdb_export( fp, name, (genptr_t)in, ID_SUBMODEL, mk_conv2mm );
}
