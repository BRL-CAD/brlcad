/*                       D B 5 _ B I N . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

/** @addtogroup db5 */

/*@{*/
/** @file db5_bin.c
 *	Handle bulk binary objects
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *
 */
/*@}*/

#ifndef lint
static const char RCSell[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "nmg.h"
#include "rtgeom.h"
#include "raytrace.h"
#include "nurb.h"
#include "./debug.h"


/* this array depends on the values of the definitions of the DB5_MINORTYPE_BINU_... in db5.h */
const char *binu_types[]={
	NULL,
	NULL,
	"binary(float)",
	"binary(double)",
	"binary(u_8bit_int)",
	"binary(u_16bit_int)",
	"binary(u_32bit_int)",
	"binary(u_64bit_int)",
	NULL,
	NULL,
	NULL,
	NULL,
	"binary(8bit_int)",
	"binary(16bit_int)",
	"binary(32bit_int)",
	"binary(64bit_int)"
};

/* size of each element (in bytes) for the different BINUNIF types */
/* this array depends on the values of the definitions of the DB5_MINORTYPE_BINU_... in db5.h */
const int binu_sizes[]={
	0,
	0,
	SIZEOF_NETWORK_FLOAT,
	SIZEOF_NETWORK_DOUBLE,
	1,
	2,
	4,
	8,
	0,
	0,
	0,
	0,
	1,
	2,
	4,
	8
};
/*
 * XXX these are the interface routines needed for table.c
 */
int
rt_bin_expm_export5(struct bu_external *ep,
			const struct rt_db_internal *ip,
			double local2mm,
			const struct db_i *dbip,
			struct resource *resp)
{
	bu_log("rt_bin_expm_export5() not implemented\n");
	return -1;
}

int
rt_bin_unif_export5(struct bu_external *ep,
			const struct rt_db_internal *ip,
			double local2mm,
			const struct db_i *dbip,
			struct resource *resp)
{
	bu_log("rt_bin_unif_export5() not implemented\n");
	return -1;
}
int
rt_bin_unif_import5(struct rt_db_internal * ip,
 			const struct bu_external *ep,
 			const mat_t mat,
			const struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_unif_import5() not implemented\n");
	return -1;
}
int
rt_bin_expm_import5(struct rt_db_internal * ip,
 			const struct bu_external *ep,
 			const mat_t mat,
			const struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_expm_import5() not implemented\n");
	return -1;
}

int
rt_bin_mime_import5(struct rt_db_internal * ip,
 			const struct bu_external *ep,
 			const mat_t mat,
			const struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_mime_import5() not implemented\n");
	return -1;
}

/**
 *			R T _ B I N U N I F _ I M P O R T 5
 *
 *  Import a uniform-array binary object from the database format to
 *  the internal structure.
 */
int
rt_binunif_import5( struct rt_db_internal	*ip,
		    const struct bu_external	*ep,
		    const mat_t			mat,
		    const struct db_i		*dbip,
		    struct resource		*resp,
		    const int			minor_type)
{
	struct rt_binunif_internal	*bip;
	int				i;
	unsigned char			*srcp;
	unsigned long			*ldestp;
	int				in_cookie, out_cookie;
	int				gotten;

	BU_CK_EXTERNAL( ep );

	/*
	 *	There's no particular size to expect
	 *
	 * BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4 );
	 */

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
	ip->idb_minor_type = minor_type;
	ip->idb_meth = &rt_functab[ID_BINUNIF];
	ip->idb_ptr = bu_malloc( sizeof(struct rt_binunif_internal),
	    "rt_binunif_internal");

	bip = (struct rt_binunif_internal *)ip->idb_ptr;
	bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
	bip->type = minor_type;

	/*
	 * Convert from database (network) to internal (host) format
	 */
	switch (bip->type) {
	    case DB5_MINORTYPE_BINU_FLOAT:
		bip->count = ep->ext_nbytes/SIZEOF_NETWORK_FLOAT;
		bip->u.uint8 = (unsigned char *) bu_malloc( bip->count * sizeof(float),
		    "rt_binunif_internal" );
		ntohf( (unsigned char *) bip->u.uint8,
			ep->ext_buf, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_DOUBLE:
		bip->count = ep->ext_nbytes/SIZEOF_NETWORK_DOUBLE;
		bip->u.uint8 = (unsigned char *) bu_malloc( bip->count * sizeof(double),
		    "rt_binunif_internal" );
		ntohd( (unsigned char *) bip->u.uint8,
			ep->ext_buf, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_8BITINT:
	    case DB5_MINORTYPE_BINU_8BITINT_U:
		bip->count = ep->ext_nbytes;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
		bcopy( (char *) ep->ext_buf, (char *) bip->u.uint8, ep->ext_nbytes );
		break;
	    case DB5_MINORTYPE_BINU_16BITINT:
	    case DB5_MINORTYPE_BINU_16BITINT_U:
		bip->count = ep->ext_nbytes/2;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
#if 0
		srcp = (unsigned char *) ep->ext_buf;
		sdestp = (unsigned short *) bip->u.uint8;
		for (i = 0; i < bip->count; ++i, ++sdestp, srcp += 2) {
		    *sdestp = bu_gshort( srcp );
		    bu_log("Just got %d", *sdestp);
		}
#endif
		in_cookie = bu_cv_cookie("nus");
		out_cookie = bu_cv_cookie("hus");
		if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
		    gotten =
		    bu_cv_w_cookie((genptr_t)bip->u.uint8, out_cookie,
				   ep->ext_nbytes,
				   ep->ext_buf, in_cookie, bip->count);
		    if (gotten != bip->count) {
			bu_log("%s:%d: Tried to convert %d, did %d",
			    __FILE__, __LINE__, bip->count, gotten);
			bu_bomb("\n");
		    }
		} else
		    bcopy( (char *) ep->ext_buf, (char *) bip->u.uint8,
			ep->ext_nbytes );
		break;
	    case DB5_MINORTYPE_BINU_32BITINT:
	    case DB5_MINORTYPE_BINU_32BITINT_U:
		bip->count = ep->ext_nbytes/4;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
		srcp = (unsigned char *) ep->ext_buf;
		ldestp = (unsigned long *) bip->u.uint8;
		for (i = 0; i < bip->count; ++i, ++ldestp, srcp += 4) {
		    *ldestp = bu_glong( srcp );
		    bu_log("Just got %ld", *ldestp);
		}
		break;
	    case DB5_MINORTYPE_BINU_64BITINT:
	    case DB5_MINORTYPE_BINU_64BITINT_U:
		bu_log("rt_binunif_import5() Can't handle 64-bit integers yet\n");
		return -1;
	}

	return 0;		/* OK */
}

/**
 *			R T _ B I N U N I F _ D U M P
 *
 *  Diagnostic routine
 */
void
rt_binunif_dump( struct rt_binunif_internal *bip) {
    RT_CK_BINUNIF(bip);
    bu_log("rt_bin_unif_internal <%x>...\n", bip);
    bu_log("  type = x%x = %d", bip -> type, bip -> type);
    bu_log("  count = %ld  first = 0x%02x", bip -> count,
	   bip->u.uint8[0] & 0x0ff);
    bu_log("- - - - -\n");
}


/**
 *			R T _ B I N E X P M _ I M P O R T 5
 *
 *  Import an experimental binary object from the database format to
 *  the internal structure.
 */
int
rt_binexpm_import5( struct rt_db_internal	*ip,
		    const unsigned char		minor_type,
		    const struct bu_external	*ep,
		    const struct db_i		*dbip )
{
	bu_log("rt_binexpm_import5() not implemented yet\n");
	return -1;
}


/**
 *			R T _ B I N M I M E _ I M P O R T 5
 *
 *  Import a MIME-typed binary object from the database format to
 *  the internal structure.
 */
int
rt_binmime_import5( struct rt_db_internal	*ip,
		    const unsigned char		minor_type,
		    const struct bu_external	*ep,
		    const struct db_i		*dbip )
{
	bu_log("rt_binmime_import5() not implemented yet\n");
	return -1;
}


/**
 *			R T _ B I N _ I M P O R T 5
 *
 *  Wrapper for importing binary objects from the database format to
 *  the internal structure.
 */
int
rt_bin_import5( struct rt_db_internal		*ip,
		const unsigned char		major_type,
		const unsigned char		minor_type,
		const struct bu_external	*ep,
		const struct db_i		*dbip )
{
    RT_CK_DB_INTERNAL(ip);

    switch (major_type) {
	case DB5_MAJORTYPE_BINARY_EXPM:
	    return rt_binexpm_import5( ip, minor_type, ep, dbip );
	case DB5_MAJORTYPE_BINARY_UNIF:
	    return rt_binunif_import5( ip, ep, 0, dbip, 0, minor_type );
	case DB5_MAJORTYPE_BINARY_MIME:
	    return rt_binmime_import5( ip, minor_type, ep, dbip );
    }
    return -1;
}

/**
 *			R T _ B I N U N I F _ E X P O R T 5
 *
 *	Create the "body" portion of external form
 */
int
rt_binunif_export5( struct bu_external		*ep,
		    const struct rt_db_internal	*ip,
		    double			local2mm,	/* we ignore */
		    const struct db_i		*dbip,
		    struct resource		*resp,
		    const int			minor_type )
{
	struct rt_binunif_internal	*bip;
	int				i;
	unsigned char			*destp;
	unsigned long			*lsrcp;
	int				in_cookie, out_cookie;
	int				gotten;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_minor_type != minor_type ) {
		bu_log("ip->idb_minor_type(%d) != minor_type(%d)\n",
		       ip->idb_minor_type, minor_type );
		return -1;
	}
	bip = (struct rt_binunif_internal *)ip->idb_ptr;
	RT_CK_BINUNIF(bip);
	if( bip->type != minor_type ) {
		bu_log("bip->type(%d) != minor_type(%d)\n",
		       bip->type, minor_type );
		return -1;
	}

	BU_INIT_EXTERNAL(ep);

	/*
	 * Convert from internal (host) to database (network) format
	 */
	switch (bip->type) {
	    case DB5_MINORTYPE_BINU_FLOAT:
		ep->ext_nbytes = bip->count * SIZEOF_NETWORK_FLOAT;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes,
		    "binunif external");
		htonf( ep->ext_buf, (unsigned char *) bip->u.uint8, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_DOUBLE:
		ep->ext_nbytes = bip->count * SIZEOF_NETWORK_DOUBLE;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes,
		    "binunif external");
		htond( ep->ext_buf, (unsigned char *) bip->u.uint8, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_8BITINT:
	    case DB5_MINORTYPE_BINU_8BITINT_U:
		ep->ext_nbytes = bip->count;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes,
		    "binunif external");
		bcopy( (char *) bip->u.uint8, (char *) ep->ext_buf, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_16BITINT:
	    case DB5_MINORTYPE_BINU_16BITINT_U:
		ep->ext_nbytes = bip->count * 2;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "binunif external");
		in_cookie = bu_cv_cookie("hus");
		out_cookie = bu_cv_cookie("nus");
		if (bu_cv_optimize(in_cookie) != bu_cv_optimize(out_cookie)) {
		    gotten =
			    bu_cv_w_cookie(ep->ext_buf, out_cookie,
					   ep->ext_nbytes,
					   (genptr_t) bip->u.uint8, in_cookie,
					   bip->count);

		    if (gotten != bip->count) {
			bu_log("%s:%d: Tried to convert %d, did %d",
			    __FILE__, __LINE__, bip->count, gotten);
			bu_bomb("\n");
		    }
		} else {
		    bcopy( (char *) bip->u.uint8, (char *) ep->ext_buf,
			ep->ext_nbytes );
		}
		break;
	    case DB5_MINORTYPE_BINU_32BITINT:
	    case DB5_MINORTYPE_BINU_32BITINT_U:
		ep->ext_nbytes = bip->count * 4;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes, "binunif external");

		lsrcp = (unsigned long *) bip->u.uint8;
		destp = (unsigned char *) ep->ext_buf;
		for (i = 0; i < bip->count; ++i, ++destp, ++lsrcp) {
		    (void) bu_plong( destp, *lsrcp );
		}
		break;
	    case DB5_MINORTYPE_BINU_64BITINT:
	    case DB5_MINORTYPE_BINU_64BITINT_U:
		bu_log("rt_binunif_export5() Can't handle 64-bit integers yet\n");
		return -1;
	}

	return 0;
}

/**
 *			R T _ B I N U N I F _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this object.
 *  First line describes type of object.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_binunif_describe( struct bu_vls		*str,
		    const struct rt_db_internal	*ip,
		    int				verbose,
		    double			mm2local )
{
	register struct rt_binunif_internal	*bip;
	char					buf[256];
	unsigned short				wid;

	bip = (struct rt_binunif_internal *) ip->idb_ptr;
	RT_CK_BINUNIF(bip);
	bu_vls_strcat( str, "uniform-array binary object (BINUNIF)\n");
	wid = (bip->type & DB5_MINORTYPE_BINU_WID_MASK) >> 4;
	switch (wid) {
	    case 0:
		sprintf( buf, "%ld ", bip->count ); break;
	    case 1:
		sprintf( buf, "%ld pairs of ", bip->count / 2 ); break;
	    case 2:
		sprintf( buf, "%ld triples of ", bip->count / 3 ); break;
	    case 3:
		sprintf( buf, "%ld quadruples of ", bip->count / 4 ); break;
	}
	bu_vls_strcat( str, buf );
	switch (bip->type & DB5_MINORTYPE_BINU_ATM_MASK) {
	    case DB5_MINORTYPE_BINU_FLOAT:
		bu_vls_strcat( str, "floats\n"); break;
	    case DB5_MINORTYPE_BINU_DOUBLE:
		bu_vls_strcat( str, "doubles\n"); break;
	    case DB5_MINORTYPE_BINU_8BITINT:
		bu_vls_strcat( str, "8-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_16BITINT:
		bu_vls_strcat( str, "16-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_32BITINT:
		bu_vls_strcat( str, "32-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_64BITINT:
		bu_vls_strcat( str, "64-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_8BITINT_U:
		bu_vls_strcat( str, "unsigned 8-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_16BITINT_U:
		bu_vls_strcat( str, "unsigned 16-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_32BITINT_U:
		bu_vls_strcat( str, "unsigned 32-bit ints\n"); break;
	    case DB5_MINORTYPE_BINU_64BITINT_U:
		bu_vls_strcat( str, "unsigned 64-bit ints\n"); break;
	    default:
		bu_log("%s:%d: This shouldn't happen", __FILE__, __LINE__);
		return(1);
	}

	return(0);
}

/**
 *		R T _ B I N U N I F _ F R E E
 *
 *	Free the storage associated with a binunif_internal object
 */
void
rt_binunif_free( struct rt_binunif_internal *bip) {
	RT_CK_BINUNIF(bip);
	bu_free( (genptr_t) bip->u.uint8, "binunif free uint8" );
	bu_free( bip, "binunif free");
	bip = GENPTR_NULL; /* sanity */
}

/**
 *			R T _ B I N U N I F _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this thing.
 */
void
rt_binunif_ifree( struct rt_db_internal	*ip )
{
	struct rt_binunif_internal	*bip;

	RT_CK_DB_INTERNAL(ip);
	bip = (struct rt_binunif_internal *)ip->idb_ptr;
	RT_CK_BINUNIF(bip);
	bu_free( (genptr_t) bip->u.uint8, "binunif ifree" );
	bu_free( ip->idb_ptr, "binunif ifree" );
	ip->idb_ptr = GENPTR_NULL;
}


int
rt_retrieve_binunif(struct rt_db_internal *intern,
		    struct db_i	*dbip,
		    char *name)
{
	register struct directory	*dp;
	struct rt_binunif_internal	*bip;
	struct bu_external		ext;
	struct db5_raw_internal		raw;
	char				*tmp;

	/*
	 *	Find the guy we're told to write
	 */
	if( (dp = db_lookup( dbip, name, LOOKUP_NOISY)) == DIR_NULL )
		return -1;

	RT_INIT_DB_INTERNAL(intern);
	if ( rt_db_get_internal5( intern, dp, dbip, NULL, &rt_uniresource)
	     != ID_BINUNIF     || db_get_external( &ext, dp, dbip ) < 0 )
		return -1;

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	    bu_log("%s:%d\n", __FILE__, __LINE__);
		bu_free_external( &ext );
		return -1;
	}
	if (db5_type_descrip_from_codes(&tmp, raw.major_type, raw.minor_type))
		tmp = 0;

	if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("get_body() sees type (%d, %d)='%s'\n",
		       raw.major_type, raw.minor_type, tmp);

	if (raw.major_type != DB5_MAJORTYPE_BINARY_UNIF)
		return -1;

	bip = intern->idb_ptr;
	RT_CK_BINUNIF(bip);
	if (RT_G_DEBUG & DEBUG_HF)
		rt_binunif_dump(bip);

	if (RT_G_DEBUG & DEBUG_VOL)
		bu_log("cmd_export_body() thinks bip->count=%d\n",
		       bip->count);

	switch (bip -> type) {
	case DB5_MINORTYPE_BINU_FLOAT:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... float");
		break;
	case DB5_MINORTYPE_BINU_DOUBLE:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... double");
		break;
	case DB5_MINORTYPE_BINU_8BITINT:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 8bitint");
		break;
	case DB5_MINORTYPE_BINU_8BITINT_U:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 8bituint");
		break;
	case DB5_MINORTYPE_BINU_16BITINT:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 16bituint");
		break;
	case DB5_MINORTYPE_BINU_16BITINT_U:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 16bitint");
		break;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 32bitint");
		break;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
		if (RT_G_DEBUG & DEBUG_VOL)
			bu_log("bip->type switch... 64bitint");
		break;
	default:
		/* XXX	This shouln't happen!!    */
		bu_log("bip->type switch... default");
		break;
	}

	bu_free_external( &ext );

	return 0;
}

void
rt_binunif_make(const struct rt_functab *ftp, struct rt_db_internal *intern, double diameter)
{
	struct rt_binunif_internal *bip;

	intern->idb_type = DB5_MINORTYPE_BINU_8BITINT;
	intern->idb_major_type = DB5_MAJORTYPE_BINARY_UNIF;
	BU_ASSERT(&rt_functab[ID_BINUNIF] == ftp);

	intern->idb_meth = ftp;
	bip = (struct rt_binunif_internal *)bu_calloc( sizeof( struct rt_binunif_internal), 1,
						       "rt_binunif_make");
	intern->idb_ptr = (genptr_t) bip;
	bip->magic = RT_BINUNIF_INTERNAL_MAGIC;
	bip->type = DB5_MINORTYPE_BINU_8BITINT;
	bip->count = 0;
	bip->u.int8 = NULL;
}

int
rt_binunif_tclget(Tcl_Interp *interp, const struct rt_db_internal *intern, const char *attr )
{
	register struct rt_binunif_internal *bip=(struct rt_binunif_internal *)intern->idb_ptr;
	struct bu_external	ext;
	Tcl_DString		ds;
	struct bu_vls		vls;
	int			status=TCL_OK;
	int			i;
	unsigned char		*c;

	RT_CHECK_BINUNIF( bip );

	Tcl_DStringInit( &ds );
	bu_vls_init( &vls );

	if( attr == (char *)NULL )
	{
		/* export the object to get machine independent form */
		if( rt_binunif_export5( &ext, intern, 1.0, NULL, NULL, intern->idb_minor_type ) ) {
			bu_vls_strcpy( &vls, "Failed to export binary object!!\n" );
			status = TCL_ERROR;
		} else {
			bu_vls_strcpy( &vls, "binunif" );
			bu_vls_printf( &vls, " T %d D {", bip->type );
			c = ext.ext_buf;
			for( i=0 ; i<ext.ext_nbytes ; i++,c++ ) {
				if( i%40 == 0 ) bu_vls_strcat( &vls, "\n" );
				bu_vls_printf( &vls, "%2.2x", *c );
			}
			bu_vls_strcat( &vls, "}" );
			bu_free_external( &ext );
		}

	} else {
		if( !strcmp( attr, "T" ) ) {
			bu_vls_printf( &vls, "%d", bip->type );
		} else if( !strcmp( attr, "D" ) ) {
			/* export the object to get machine independent form */
			if( rt_binunif_export5( &ext, intern, 1.0, NULL, NULL,
						intern->idb_minor_type ) ) {
				bu_vls_strcpy( &vls, "Failed to export binary object!!\n" );
				status = TCL_ERROR;
			} else {
				c = ext.ext_buf;
				for( i=0 ; i<ext.ext_nbytes ; i++,c++ ) {
					if( i != 0 && i%40 == 0 ) bu_vls_strcat( &vls, "\n" );
					bu_vls_printf( &vls, "%2.2x", *c );
				}
				bu_free_external( &ext );
			}
		} else {
			bu_vls_printf( &vls, "Binary object has no attribute '%s'", attr );
			status = TCL_ERROR;
		}
	}

	Tcl_DStringAppend( &ds, bu_vls_addr( &vls ), -1 );
	Tcl_DStringResult( interp, &ds );
	Tcl_DStringFree( &ds );
	bu_vls_free( &vls );

	return( status );
}

int
rt_binunif_tcladjust( Tcl_Interp *interp, struct rt_db_internal *intern, int argc, char **argv )
{
	struct rt_binunif_internal *bip;
	int i;

	RT_CK_DB_INTERNAL( intern );
	bip = (struct rt_binunif_internal *)intern->idb_ptr;
	RT_CHECK_BINUNIF( bip );

	while( argc >= 2 ) {
		if( !strcmp( argv[0], "T" ) ) {
			int new_type=-1;
			char *c;
			int type_is_digit=1;

			c = argv[1];
			while( *c != '\0' ) {
				if( !isdigit( *c ) ) {
					type_is_digit = 0;
					break;
				}
				c++;
			}

			if( type_is_digit ) {
				new_type = atoi( argv[1] );
			} else {
				if( argv[1][1] != '\0' ) {
					Tcl_AppendResult( interp, "Illegal type: ",
					   argv[1],
					   ", must be 'f', 'd', 'c', 'i', 'l', 'C', 'S', 'I', or 'L'",
					   (char *)NULL );
					return TCL_ERROR;
				}
				switch( argv[1][0] ) {
					case 'f':
						new_type = 2;
						break;
					case 'd':
						new_type = 3;
						break;
					case 'c':
						new_type = 4;
						break;
					case 's':
						new_type = 5;
						break;
					case 'i':
						new_type = 6;
						break;
					case 'l':
						new_type = 7;
						break;
					case 'C':
						new_type = 12;
						break;
					case 'S':
						new_type = 13;
						break;
					case 'I':
						new_type = 14;
						break;
					case 'L':
						new_type = 15;
						break;
				}
			}
			if( new_type < 0 ||
			    new_type > DB5_MINORTYPE_BINU_64BITINT ||
			    binu_types[new_type] == NULL ) {
				/* Illegal value for type */
				Tcl_AppendResult( interp, "Illegal value for binary type: ", argv[1],
						  (char *)NULL );
				return TCL_ERROR;
			} else {
				if( bip->u.uint8 ) {
					int new_count;
					int old_byte_count, new_byte_count;
					int remainder;

					old_byte_count = bip->count * binu_sizes[bip->type];
					new_count = old_byte_count / binu_sizes[new_type];
					remainder = old_byte_count % binu_sizes[new_type];
					if( remainder ) {
						new_count++;
						new_byte_count = new_count * binu_sizes[new_type];
					} else {
						new_byte_count = old_byte_count;
					}

					if( new_byte_count != old_byte_count ) {
						bip->u.uint8 = bu_realloc( bip->u.uint8,
									   new_byte_count,
									   "new bytes for binunif" );
						/* zero out the new bytes */
						for( i=old_byte_count ; i<new_byte_count ; i++ ) {
							bip->u.uint8[i] = 0;
						}
					}
					bip->count = new_count;
				}
				bip->type = new_type;
				intern->idb_type = new_type;
			}
		} else if( !strcmp( argv[0], "D" ) ) {
			Tcl_Obj *obj, *list, **obj_array;
			int list_len;
			unsigned char *buf, *d;
			char *s;
			int hexlen;
			unsigned int h;

			obj = Tcl_NewStringObj( argv[1], -1 );
			list = Tcl_NewListObj( 0, NULL );
			Tcl_ListObjAppendList( interp, list, obj );
			(void)Tcl_ListObjGetElements( interp, list, &list_len, &obj_array );

			hexlen = 0;
			for( i=0 ; i<list_len ; i++ ) {
				hexlen += Tcl_GetCharLength( obj_array[i] );
			}

			if( hexlen % 2 ) {
				Tcl_AppendResult( interp,
				    "Hex form of binary data must have an even number of hex digits",
				    (char *)NULL );
				return TCL_ERROR;
			}

			buf = (unsigned char *)bu_malloc( hexlen / 2, "tcladjust binary data" );
			d = buf;
			for( i=0 ; i<list_len ; i++ ) {
				s = Tcl_GetString( obj_array[i] );
				while( *s ) {
					sscanf( s, "%2x", &h );
					*d++ = h;
					s += 2;
				}
			}
			Tcl_DecrRefCount( list );

			if( bip->u.uint8 ) {
				bu_free( bip->u.uint8, "binary data" );
			}
			bip->u.uint8 = buf;
			bip->count = hexlen / 2 / binu_sizes[bip->type];
		}

		argc -= 2;
		argv += 2;
	}

	return TCL_OK;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
