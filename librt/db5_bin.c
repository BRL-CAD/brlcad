/*
 *			D B 5 _ B I N . C
 *
 *  Purpose -
 *	Handle bulk binary objects
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 *  
 *  Copyright Notice -
 *	This software is Copyright (C) 2000 by the United States Army.
 *	All rights reserved.
 */
#ifndef lint
static const char RCSell[] = "@(#)$Header$ (BRL)";
#endif

#include "conf.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "nmg.h"
#include "raytrace.h"
#include "nurb.h"
#include "rtgeom.h"
#include "./debug.h"

/*
 * XXX these are the interface routines needed for table.c
 */
int
rt_bin_expm_export5(struct bu_external *ep,
			CONST struct rt_db_internal *ip,
			double local2mm,
			CONST struct db_i *dbip,
			struct resource *resp)
{
	bu_log("rt_bin_expm_export5() not implemented\n");
	return -1;
}

int
rt_bin_unif_export5(struct bu_external *ep,
			CONST struct rt_db_internal *ip,
			double local2mm,
			CONST struct db_i *dbip,
			struct resource *resp)
{
	bu_log("rt_bin_unif_export5() not implemented\n");
	return -1;
}
int
rt_bin_unif_import5(struct rt_db_internal * ip,
 			CONST struct bu_external *ep,
 			CONST mat_t mat,
			CONST struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_unif_import5() not implemented\n");
	return -1;
}
int
rt_bin_expm_import5(struct rt_db_internal * ip,
 			CONST struct bu_external *ep,
 			CONST mat_t mat,
			CONST struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_expm_import5() not implemented\n");
	return -1;
}

int
rt_bin_mime_import5(struct rt_db_internal * ip,
 			CONST struct bu_external *ep,
 			CONST mat_t mat,
			CONST struct db_i *dbip,
			      struct resource *resp)
{
	bu_log("rt_bin_mime_import5() not implemented\n");
	return -1;
}

/*
 *			R T _ B I N U N I F _ I M P O R T 5
 *
 *  Import a uniform-array binary object from the database format to
 *  the internal structure.
 */
int
rt_binunif_import5( struct rt_db_internal	*ip,
		    CONST struct bu_external	*ep,
		    CONST mat_t			mat,
		    CONST struct db_i		*dbip,
		    struct resource		*resp,
		    CONST int			minor_type)
{
	struct rt_binunif_internal	*bip;
	int				i;
	unsigned char			*srcp;
	unsigned long			*ldestp;
	unsigned short			*sdestp;

	BU_CK_EXTERNAL( ep );

	/*
	 *	There's no particular size to expect
	 *
	 * BU_ASSERT_LONG( ep->ext_nbytes, ==, SIZEOF_NETWORK_DOUBLE * 3*4 );
	 */

	RT_CK_DB_INTERNAL( ip );
	ip->idb_type = ID_BINUNIF;
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
			ep->ext_buf, ep->ext_nbytes/SIZEOF_NETWORK_FLOAT );
		break;
	    case DB5_MINORTYPE_BINU_DOUBLE:
		bip->count = ep->ext_nbytes/SIZEOF_NETWORK_DOUBLE;
		bip->u.uint8 = (unsigned char *) bu_malloc( bip->count * sizeof(float),
		    "rt_binunif_internal" );
		ntohd( (unsigned char *) bip->u.uint8,
			ep->ext_buf, ep->ext_nbytes/SIZEOF_NETWORK_DOUBLE );
		break;
	    case DB5_MINORTYPE_BINU_8BITINT:
	    case DB5_MINORTYPE_BINU_8BITINT_U:
		bip->count = ep->ext_nbytes;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
		bcopy( (char *) bip->u.uint8, (char *) ep->ext_buf, ep->ext_nbytes );
		break;
	    case DB5_MINORTYPE_BINU_16BITINT:
	    case DB5_MINORTYPE_BINU_16BITINT_U:
		bip->count = ep->ext_nbytes/2;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
		srcp = (unsigned char *) ep->ext_buf;
		sdestp = (unsigned short *) bip->u.uint8;
		for (i = 0; i < bip->count; ++i, ++sdestp, srcp += 2) {
		    *sdestp = bu_gshort( (unsigned char *) srcp );
		}
		break;
	    case DB5_MINORTYPE_BINU_32BITINT:
	    case DB5_MINORTYPE_BINU_32BITINT_U:
		bip->count = ep->ext_nbytes/4;
		bip->u.uint8 = (unsigned char *) bu_malloc( ep->ext_nbytes,
		    "rt_binunif_internal" );
		srcp = (unsigned char *) ep->ext_buf;
		ldestp = (unsigned long *) bip->u.uint8;
		for (i = 0; i < bip->count; ++i, ++ldestp, srcp += 4) {
		    *ldestp = bu_glong( (unsigned char *) srcp );
		}
		break;
	    case DB5_MINORTYPE_BINU_64BITINT:
	    case DB5_MINORTYPE_BINU_64BITINT_U:
		bu_log("rt_binunif_import5() Can't handle 64-bit integers yet\n");
		return -1;
	}

	return 0;		/* OK */
}

/*
 *			R T _ B I N U N I F _ D U M P
 *
 *  Diagnostic routine
 */
void
rt_binunif_dump( struct rt_binunif_internal *bip) {
    RT_CK_BINUNIF(bip);
    bu_log("rt_bin_unif_internal <%x>...", bip);
    bu_log("  type = x%x = %d", bip -> type, bip -> type);
    bu_log("  count = %ld", bip -> count);
    bu_log("- - - - -");
}


/*
 *			R T _ B I N E X P M _ I M P O R T 5
 *
 *  Import an experimental binary object from the database format to
 *  the internal structure.
 */
int
rt_binexpm_import5( struct rt_db_internal	*ip,
		    CONST unsigned char		minor_type,
		    CONST struct bu_external	*ep,
		    CONST struct db_i		*dbip )
{
	bu_log("rt_binexpm_import5() not implemented yet\n");
	return -1;
}


/*
 *			R T _ B I N M I M E _ I M P O R T 5
 *
 *  Import a MIME-typed binary object from the database format to
 *  the internal structure.
 */
int
rt_binmime_import5( struct rt_db_internal	*ip,
		    CONST unsigned char		minor_type,
		    CONST struct bu_external	*ep,
		    CONST struct db_i		*dbip )
{
	bu_log("rt_binmime_import5() not implemented yet\n");
	return -1;
}


/*
 *			R T _ B I N _ I M P O R T 5
 *
 *  Wrapper for importing binary objects from the database format to
 *  the internal structure.
 */
int
rt_bin_import5( struct rt_db_internal		*ip,
		CONST unsigned char		major_type,
		CONST unsigned char		minor_type,
		CONST struct bu_external	*ep,
		CONST struct db_i		*dbip )
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

/*
 *			R T _ B I N U N I F _ E X P O R T 5
 */
int
rt_binunif_export5( struct bu_external		*ep,
		    CONST struct rt_db_internal	*ip,
		    double			local2mm,	/* we ignore */
		    CONST struct db_i		*dbip,
		    struct resource		*resp,
		    CONST int			minor_type )
{
	struct rt_binunif_internal	*bip;
	int				i;
	unsigned char			*destp;
	unsigned long			*lsrcp;
	unsigned short			*ssrcp;

	RT_CK_DB_INTERNAL(ip);
	if( ip->idb_type != minor_type ) {
		bu_log("ip->idb_type(%d) != minor_type(%d)\n",
		       ip->idb_type, minor_type );
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
		bcopy( (char *) ep->ext_buf, (char *) bip->u.uint8, bip->count );
		break;
	    case DB5_MINORTYPE_BINU_16BITINT:
	    case DB5_MINORTYPE_BINU_16BITINT_U:
		ep->ext_nbytes = bip->count * 2;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes,
		    "binunif external");

		ssrcp = (unsigned short *) bip->u.uint8;
		destp = (unsigned char *) ep->ext_buf;
		for (i = 0; i < bip->count; ++i, ++destp, ssrcp += 2)
		    (void) bu_pshort( destp, *ssrcp );
		break;
	    case DB5_MINORTYPE_BINU_32BITINT:
	    case DB5_MINORTYPE_BINU_32BITINT_U:
		ep->ext_nbytes = bip->count * 4;
		ep->ext_buf = (genptr_t)bu_malloc( ep->ext_nbytes,
		    "binunif external");

		lsrcp = (unsigned long *) bip->u.uint8;
		destp = (unsigned char *) ep->ext_buf;
		for (i = 0; i < bip->count; ++i, ++destp, lsrcp += 4)
		    (void) bu_plong( destp, *lsrcp );
		break;
	    case DB5_MINORTYPE_BINU_64BITINT:
	    case DB5_MINORTYPE_BINU_64BITINT_U:
		bu_log("rt_binunif_export5() Can't handle 64-bit integers yet\n");
		return -1;
	}

	return 0;
}

/*
 *			R T _ B I N U N I F _ D E S C R I B E
 *
 *  Make human-readable formatted presentation of this object.
 *  First line describes type of object.
 *  Additional lines are indented one tab, and give parameter values.
 */
int
rt_binunif_describe( struct bu_vls		*str,
		    CONST struct rt_db_internal	*ip,
		    int				verbose,
		    double			mm2local )
{
	register struct rt_binunif_internal	*bip;
	char					buf[256];
	unsigned short				wid;

	bu_made_it();
	bip = (struct rt_binunif_internal *) ip->idb_ptr;
	RT_CK_BINUNIF(bip);
	rt_binunif_dump(bip);
	bu_vls_strcat( str, "uniform-array binary object (BINUNIF)\n");
	wid = (bip->type & DB5_MINORTYPE_BINU_WID_MASK) >> 4;
	bu_made_it();
	bu_log("bip->count is %d\n", bip->count);
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
	bu_made_it();
	bu_vls_strcat( str, buf );
	bu_made_it();
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
	bu_made_it();
	bu_log("str contains: '%s'\n", bu_vls_addr(str));

	return(0);
}

/*
 *			R T _ B I N U N I F _ I F R E E
 *
 *  Free the storage associated with the rt_db_internal version of this solid.
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
