/*
 *			D B 5 _ S C A N . C
 *
 *  Scan a v5 database, sending each object off to a handler.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "conf.h"

#include <stdio.h>
#ifdef USE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"

#include "./debug.h"

/*
 *			D B 5 _ H E A D E R _ I S _ V A L I D
 *
 *  Returns -
 *	0	Not valid v5 header
 *	1	Valid v5 header
 */
int
db5_header_is_valid( hp )
CONST unsigned char *hp;
{
	CONST struct db5_ondisk_header *odp = (CONST struct db5_ondisk_header *)hp;

	if( odp->db5h_magic1 != DB5HDR_MAGIC1 )  return 0;
	if( hp[7] != DB5HDR_MAGIC2 )  return 0;

	/* hflags */
	if( (odp->db5h_hflags & DB5HDR_HFLAGS_NAME_PRESENT) )  return 0;
	if( ((odp->db5h_hflags & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >> DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* iflags */
	if( (odp->db5h_iflags & DB5HDR_IFLAGS_ZZZ_MASK) != DB5HDR_IFLAGS_ZZZ_UNCOMPRESSED )  return 0;
	if( odp->db5h_iflags & DB5HDR_IFLAGS_ATTRIBUTES_PRESENT )  return 0;
	if( odp->db5h_iflags & DB5HDR_IFLAGS_BODY_PRESENT )  return 0;
	if( ((odp->db5h_iflags & DB5HDR_IFLAGS_INTERIOR_WIDTH_MASK) >> DB5HDR_IFLAGS_INTERIOR_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* major and minor type */
	if( odp->db5h_major_type != DB5HDR_MAJORTYPE_DLI )  return 0;
	if( odp->db5h_minor_type != DB5HDR_MINORTYPE_DLI_HEADER )  return 0;

	/* Check length, known to be 8-bit.  Header len=1 8-byte chunk. */
	if( hp[5] != 1 )  return 0;

	/* Ensure pad is zero */
	if( hp[6] != 0 )  return 0;

}

/*
 *			D B 5 _ D E C O D E _ L E N G T H
 *
 *  Given a variable-width length field in network order (XDR),
 *  store it in *lenp.
 *
 *  Note that for object_length the returned number needs to be
 *  multiplied by 8, while for the other lengths, it is already a byte count.
 *
 *  Returns -
 *	The number of bytes of input that were decoded.
 */
int
db5_decode_length( lenp, cp, format )
long			*lenp;
CONST unsigned char	*cp;
int			format;
{
	switch( format )  {
	case DB5HDR_WIDTHCODE_8BIT:
		*lenp = (*cp);
		return 1;
	case DB5HDR_WIDTHCODE_16BIT:
		*lenp = BU_GSHORT(cp);
		return 2;
	case DB5HDR_WIDTHCODE_32BIT:
		*lenp = BU_GLONG(cp);
		return 4;
	case DB5HDR_WIDTHCODE_64BIT:
		if( sizeof(long) >= 8 )  {
			*lenp = BU_GLONGLONG(cp);
			return 8;
		}
		bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
	}
	bu_bomb("db5_decode_length(): unknown width code\n");
	/* NOTREACHED */
}

/*
 *  Returns -
 *	0 on success
 *	-1 on error
 */
int
db5_crack_disk_header( rip, cp )
struct db5_raw_internal		*rip;
CONST unsigned char		*cp;
{
	if( cp[0] != DB5HDR_MAGIC1 )  return 0;

	/* hflags */
	rip->h_object_width = (cp[1] & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >>
		DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT;
	rip->h_name_present = (cp[1] & DB5HDR_HFLAGS_NAME_PRESENT);

	/* iflags */
	rip->i_object_width = (cp[2] & DB5HDR_IFLAGS_INTERIOR_WIDTH_MASK) >>
		DB5HDR_IFLAGS_INTERIOR_WIDTH_SHIFT;
	rip->i_attributes_present = (cp[2] & DB5HDR_IFLAGS_ATTRIBUTES_PRESENT);
	rip->i_body_present = (cp[2] & DB5HDR_IFLAGS_BODY_PRESENT);
	rip->i_zzz = (cp[2] & DB5HDR_IFLAGS_ZZZ_MASK);

	rip->major_type = cp[3];
	rip->minor_type = cp[4];

	return 0;
}

/*
 *  Returns -
 *	on success, pointer to first unused byte
 *	NULL, on error
 */
CONST unsigned char *
db5_get_raw_internal_ptr( rip, ip )
struct db5_raw_internal		*rip;
CONST unsigned char		*ip;
{
	CONST unsigned char	*cp = ip;

	if( db5_crack_disk_header( rip, cp ) < 0 )  return NULL;
	cp += sizeof(struct db5_ondisk_header);

	cp += db5_decode_length( &rip->object_length, cp, rip->h_object_width );
	rip->object_length <<= 3;	/* cvt 8-byte chunks to byte count */

	/* Verify trailing magic number */
	if( ip[rip->object_length-1] != DB5HDR_MAGIC2 )  {
		bu_log("db5_get_raw_internal_ptr() bad magic2\n");
		return NULL;
	}

	/* Grab name, if present */
	if( rip->h_name_present )  {
		cp += db5_decode_length( &rip->name_length, cp, rip->i_object_width );
		rip->name = (unsigned char *)cp;	/* discard CONST */
		cp += rip->name_length;
	} else {
		rip->name_length = 0;
		rip->name = NULL;
	}

	/* Point to object interior, if present */
	if( rip->i_attributes_present || rip->i_body_present )  {
		/* interior_length will include any pad bytes but not magic2 */
		/* Because it may be compressed, we don't know exact len yet */
		rip->interior_length = cp - rip->buf - 1;
		rip->interior = (unsigned char *)cp;	/* discart CONST */
	} else {
		rip->interior_length = 0;
		rip->interior = NULL;
	}

	return ip + rip->object_length;
}

/*
 *  Returns -
 *	0 on success
 *	-1 on EOF
 *	-2 on error
 */
int
db5_get_raw_internal_fp( rip, fp )
struct db5_raw_internal	*rip;
FILE			*fp;
{
	struct db5_ondisk_header	header;
	unsigned char			lenbuf[8];
	int				count;
	int				used;
	long				want, got;
	unsigned char			*cp;

	if( fread( (unsigned char *)&header, sizeof header, 1, fp ) != 1  )  {
		if( feof(fp) )  return -1;
		bu_log("db5_get_raw_internal_fp(): fread header error\n");
		return -2;
	}
	if( db5_crack_disk_header( rip, (unsigned char *)&header ) < 0 )
		return -2;
	used = sizeof(header);

	switch( rip->h_object_width )  {
	case DB5HDR_WIDTHCODE_8BIT:
		count = 1;
		break;
	case DB5HDR_WIDTHCODE_16BIT:
		count = 2;
		break;
	case DB5HDR_WIDTHCODE_32BIT:
		count = 4;
		break;
	case DB5HDR_WIDTHCODE_64BIT:
		count = 8;
	}
	if( fread( lenbuf, count, 1, fp )  != 1 )  {
		bu_log("db5_get_raw_internal_fp(): fread lenbuf error\n");
		return -2;
	}
	used += db5_decode_length( &rip->object_length, lenbuf, rip->h_object_width );
	rip->object_length <<= 3;	/* cvt 8-byte chunks to byte count */

	/* Now that we finally know how large the object is, get it all */
	rip->buf = (unsigned char *)bu_malloc( rip->object_length, "raw v5 object" );

	*((struct db5_ondisk_header *)rip->buf) = header;	/* struct copy */
	bcopy( lenbuf, rip->buf+sizeof(header), count );

	cp = rip->buf+used;
	want = rip->object_length-used;
	BU_ASSERT_LONG( want, >, 0 );
	if( (got = fread( cp, want, 1, fp )) != want ) {
		bu_log("db5_get_raw_internal_fp(), want=%ld, got=%ld, database is too short\n",
			want, got );
		return -2;
	}

	/* Verify trailing magic number */
	if( rip->buf[rip->object_length-1] != DB5HDR_MAGIC2 )  {
		bu_log("db5_get_raw_internal_fp() bad magic2\n");
		return -2;
	}

	/* Grab name, if present */
	if( rip->h_name_present )  {
		cp += db5_decode_length( &rip->name_length, cp, rip->i_object_width );
		rip->name = cp;
		cp += rip->name_length;
	} else {
		rip->name_length = 0;
		rip->name = NULL;
	}

	/* Point to object interior, if present */
	if( rip->i_attributes_present || rip->i_body_present )  {
		/* interior_length will include any pad bytes but not magic2 */
		/* Because it may be compressed, we don't know exact len yet */
		rip->interior_length = cp - rip->buf - 1;
		rip->interior = cp;
	} else {
		rip->interior_length = 0;
		rip->interior = NULL;
	}

	return 0;		/* success */
}


/*
 *			D B 5 _ S C A N
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db5_scan( dbip, handler, client_data )
register struct db_i	*dbip;
void			(*handler)BU_ARGS((struct db_i *,
			CONST struct db5_raw_internal *,
			long addr, genptr_t client_data));
genptr_t		client_data;	/* argument for handler */
{
	unsigned char	header[8];
	struct db5_raw_internal	raw;
	int			got;
	long			nrec;
	long			addr;

	RT_CK_DBI(dbip);
	if(rt_g.debug&DEBUG_DB) bu_log("db5_scan( x%x, x%x )\n", dbip, handler);

	/* In a portable way, read the header (even if not rewound) */
	rewind( dbip->dbi_fp );
	if( fread( header, sizeof header, 1, dbip->dbi_fp ) != 1  ||
	    db5_header_is_valid( header ) == 0 )  {
		bu_log("db_scan ERROR:  %s is lacking a proper BRL-CAD v5 database header\n", dbip->dbi_filename);
	    	return(-1);
	}
	rewind( dbip->dbi_fp );

	addr = 0L;
	nrec = 0L;
	raw.magic = DB5_RAW_INTERNAL_MAGIC;
	while(1)  {
		addr = ftell( dbip->dbi_fp );
		if( (got = db5_get_raw_internal_fp( &raw, dbip->dbi_fp )) < 0 )  {
			if( got == -1 )  break;		/* EOF */
			return -1;			/* fatal error */
		}
		(*handler)(dbip, &raw, addr, client_data);
		nrec++;
	}

	dbip->dbi_nrec = nrec;			/* # obj in db, including header */
	dbip->dbi_eof = ftell( dbip->dbi_fp );
	rewind( dbip->dbi_fp );

	return 0;				/* success */
}
