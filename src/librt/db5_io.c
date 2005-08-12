/*                        D B 5 _ I O . C
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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

/** \addtogroup db5 */

/*@{*/
/** @file db5_io.c
 *  Handle import/export and IO of v5 database objects.
 *
 *  Author -
 *	Michael John Muuss
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 */
/*@}*/

#ifndef lint
static const char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include "common.h"

#include <stdio.h>
#ifdef HAVE_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif

#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"

#include "mater.h"

#include "./debug.h"

/* Number of bytes used for each value of DB5HDR_WIDTHCODE_* */
const int db5_enc_len[4] = {
	1,
	2,
	4,
	8
};

/**
 *			D B 5 _ H E A D E R _ I S _ V A L I D
 *
 *  Verify that this is a valid header for a BRL-CAD v5 database.
 *
 *  Returns -
 *	0	Not valid v5 header
 *	1	Valid v5 header
 */
int
db5_header_is_valid(const unsigned char *hp)
{
	const struct db5_ondisk_header *odp = (const struct db5_ondisk_header *)hp;

	if( odp->db5h_magic1 != DB5HDR_MAGIC1 )  return 0;
	if( hp[7] != DB5HDR_MAGIC2 )  return 0;

	/* hflags */
	if( (odp->db5h_hflags & DB5HDR_HFLAGS_DLI_MASK) != DB5HDR_HFLAGS_DLI_HEADER_OBJECT )
		return 0;
	if( (odp->db5h_hflags & DB5HDR_HFLAGS_NAME_PRESENT) )  return 0;
	if( ((odp->db5h_hflags & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >> DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* aflags */
	if( (odp->db5h_aflags & DB5HDR_AFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED )  return 0;
	if( odp->db5h_aflags & DB5HDR_AFLAGS_PRESENT )  return 0;
	if( ((odp->db5h_aflags & DB5HDR_AFLAGS_WIDTH_MASK) >> DB5HDR_AFLAGS_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* bflags */
	if( (odp->db5h_bflags & DB5HDR_BFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED )  return 0;
	if( odp->db5h_bflags & DB5HDR_BFLAGS_PRESENT )  return 0;
	if( ((odp->db5h_bflags & DB5HDR_BFLAGS_WIDTH_MASK) >> DB5HDR_BFLAGS_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* major and minor type */
	if( odp->db5h_major_type != DB5_MAJORTYPE_RESERVED )  return 0;
	if( odp->db5h_minor_type != 0 )  return 0;

	/* Check length, known to be 8-bit.  Header len=1 8-byte chunk. */
	if( hp[6] != 1 )  return 0;

	return 1;		/* valid */
}

/**
 *			D B 5 _ S E L E C T _ L E N G T H _ E N C O D I N G
 *
 *  Given a number to encode, decide which is the smallest encoding format
 *  which will contain it.
 */
int
db5_select_length_encoding(long int len)
{
	if( len <= 255 )  return DB5HDR_WIDTHCODE_8BIT;
	if( len <= 65535 )  return DB5HDR_WIDTHCODE_16BIT;
	if( len < 0x7ffffffe )  return DB5HDR_WIDTHCODE_32BIT;
	return DB5HDR_WIDTHCODE_64BIT;
}

/**
 *			D B 5 _ D E C O D E _ L E N G T H
 *
 *  Given a variable-width length field in network order (XDR),
 *  store it in *lenp.
 *
 *  This routine processes unsigned values.
 *
 *  Returns -
 *	The number of bytes of input that were decoded.
 */
int
db5_decode_length(long int *lenp, const unsigned char *cp, int format)
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
#if defined(IRIX64)
		if( sizeof(long) >= 8 )  {
			*lenp = BU_GLONGLONG(cp);
			return 8;
		}
#endif
		bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
	}
	bu_bomb("db5_decode_length(): unknown width code\n");
	return 0;
}

/**
 *			D B 5 _ D E C O D E _ S I G N E D
 *
 *  Given a variable-width length field in network order (XDR),
 *  store it in *lenp.
 *
 *  This routine processes signed values.
 *
 *  Returns -
 *	The number of bytes of input that were decoded.
 */
int
db5_decode_signed(long int *lenp, const unsigned char *cp, int format)
{
	switch( format )  {
	case DB5HDR_WIDTHCODE_8BIT:
		if( (*lenp = (*cp)) & 0x80 ) *lenp |= (-1L ^ 0xFF);
		return 1;
	case DB5HDR_WIDTHCODE_16BIT:
		if( (*lenp = BU_GSHORT(cp)) & 0x8000 )  *lenp |= (-1L ^ 0xFFFF);
		return 2;
	case DB5HDR_WIDTHCODE_32BIT:
		if( (*lenp = BU_GLONG(cp)) & 0x80000000 )
			*lenp |= (-1L ^ 0xFFFFFFFF);
		return 4;
	case DB5HDR_WIDTHCODE_64BIT:
#if defined(IRIX64)
		if( sizeof(long) >= 8 )  {
			*lenp = BU_GLONGLONG(cp);
			return 8;
		}
#endif
		bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
	}
	bu_bomb("db5_decode_length(): unknown width code\n");
	return 0;
}

/**
 *			D B 5 _ E N C O D E _ L E N G T H
 *
 *  Given a value and a variable-width format spec,
 *  store it in network order (XDR).
 *
 *  Returns -
 *	pointer to next available byte.
 */
unsigned char *
db5_encode_length(
	unsigned char	*cp,
	long		val,
	int		format)
{
	switch( format )  {
	case DB5HDR_WIDTHCODE_8BIT:
		*cp = val & 0xFF;
		return cp+1;
	case DB5HDR_WIDTHCODE_16BIT:
		return bu_pshort( cp, (short)val );
	case DB5HDR_WIDTHCODE_32BIT:
		return bu_plong( cp, val );
	case DB5HDR_WIDTHCODE_64BIT:
#if defined(IRIX64)
#endif
		bu_bomb("db5_encode_length(): encountered 64-bit length\n");
	}
	bu_bomb("db5_encode_length(): unknown width code\n");
	return 0;
}

/**
 *			D B 5 _ C R A C K _ D I S K _ H E A D E R
 *
 *  Returns -
 *	0 on success
 *	-1 on error
 */
int
db5_crack_disk_header(struct db5_raw_internal *rip, const unsigned char *cp)
{
	if( cp[0] != DB5HDR_MAGIC1 )  {
		bu_log("db5_crack_disk_header() bad magic1 -- database has become corrupted\n expected x%x, got x%x\n",
			DB5HDR_MAGIC1, cp[0]);
		if( cp[0] == 'I' )
		  bu_log ("Concatenation of different database versions detected.\n");
		  bu_log ("Run 'dbupgrade' on all databases before concatenation (cat command).\n");
		
		return 0;
	}

	/* hflags */
	rip->h_dli = (cp[1] & DB5HDR_HFLAGS_DLI_MASK);
	rip->h_object_width = (cp[1] & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >>
		DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT;
	rip->h_name_present = (cp[1] & DB5HDR_HFLAGS_NAME_PRESENT);
	rip->h_name_hidden = (cp[1] & DB5HDR_HFLAGS_HIDDEN_OBJECT);
	rip->h_name_width = (cp[1] & DB5HDR_HFLAGS_NAME_WIDTH_MASK) >>
		DB5HDR_HFLAGS_NAME_WIDTH_SHIFT;

	/* aflags */
	rip->a_width = (cp[2] & DB5HDR_AFLAGS_WIDTH_MASK) >>
		DB5HDR_AFLAGS_WIDTH_SHIFT;
	rip->a_present = (cp[2] & DB5HDR_AFLAGS_PRESENT);
	rip->a_zzz = (cp[2] & DB5HDR_AFLAGS_ZZZ_MASK);

	/* bflags */
	rip->b_width = (cp[3] & DB5HDR_BFLAGS_WIDTH_MASK) >>
		DB5HDR_BFLAGS_WIDTH_SHIFT;
	rip->b_present = (cp[3] & DB5HDR_BFLAGS_PRESENT);
	rip->b_zzz = (cp[3] & DB5HDR_BFLAGS_ZZZ_MASK);

	rip->major_type = cp[4];
	rip->minor_type = cp[5];

	if(RT_G_DEBUG&DEBUG_DB) bu_log("db5_crack_disk_header()\n\
	h_dli=%d, h_object_width=%d, h_name_present=%d, h_name_width=%d,\n\
	a_width=%d, a_present=%d, a_zzz=%d,\n\
	b_width=%d, b_present=%d, b_zzz=%d, major=%d, minor=%d\n",
		rip->h_dli,
		rip->h_object_width,
		rip->h_name_present,
		rip->h_name_width,
		rip->a_width,
		rip->a_present,
		rip->a_zzz,
		rip->b_width,
		rip->b_present,
		rip->b_zzz,
		rip->major_type,
		rip->minor_type );

	return 0;
}

/**
 *			D B 5 _ G E T _ R A W _ I N T E R N A L _ P T R
 *
 *  Returns -
 *	on success, pointer to first unused byte
 *	NULL, on error
 */
const unsigned char *
db5_get_raw_internal_ptr( struct db5_raw_internal *rip, const unsigned char *ip)
{
	const unsigned char	*cp = ip;

	if( db5_crack_disk_header( rip, cp ) < 0 )  return NULL;
	cp += sizeof(struct db5_ondisk_header);

	cp += db5_decode_length( &rip->object_length, cp, rip->h_object_width );
	rip->object_length <<= 3;	/* cvt 8-byte chunks to byte count */

	if( rip->object_length < sizeof(struct db5_ondisk_header) )  {
		bu_log("db5_get_raw_internal_ptr(): object_length=%ld is too short, database is corrupted\n",
			rip->object_length);
		return NULL;
	}

	/* Verify trailing magic number */
	if( ip[rip->object_length-1] != DB5HDR_MAGIC2 )  {
		bu_log("db5_get_raw_internal_ptr() bad magic2 -- database has become corrupted.\n expected x%x, got x%x\n",
			DB5HDR_MAGIC2, ip[rip->object_length-1] );
		return NULL;
	}

	BU_INIT_EXTERNAL( &rip->name );
	BU_INIT_EXTERNAL( &rip->body );
	BU_INIT_EXTERNAL( &rip->attributes );

	/* Grab name, if present */
	if( rip->h_name_present )  {
		cp += db5_decode_length( &rip->name.ext_nbytes,
			cp, rip->h_name_width );
		rip->name.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->name.ext_nbytes;
	}

	/* Point to attributes, if present */
	if( rip->a_present )  {
		cp += db5_decode_length( &rip->attributes.ext_nbytes,
			cp, rip->a_width );
		rip->attributes.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->attributes.ext_nbytes;
	}

	/* Point to body, if present */
	if( rip->b_present )  {
		cp += db5_decode_length( &rip->body.ext_nbytes,
			cp, rip->b_width );
		rip->body.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->body.ext_nbytes;
	}

	rip->buf = NULL;	/* no buffer needs freeing */

	return ip + rip->object_length;
}

/**
 *			D B 5 _ G E T _ R A W _ I N T E R N A L _ F P
 *
 *  Returns -
 *	0 on success
 *	-1 on EOF
 *	-2 on error
 */
int
db5_get_raw_internal_fp(struct db5_raw_internal *rip, FILE *fp)
{
	struct db5_ondisk_header	header;
	unsigned char			lenbuf[8];
	int				count = 0;
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

	if( rip->object_length < sizeof(struct db5_ondisk_header) )  {
		bu_log("db5_get_raw_internal_fp(): object_length=%ld is too short, database is corrupted\n",
			rip->object_length);
		return -1;
	}

	/* Now that we finally know how large the object is, get it all */
	rip->buf = (unsigned char *)bu_malloc( rip->object_length, "raw v5 object" );

	*((struct db5_ondisk_header *)rip->buf) = header;	/* struct copy */
	bcopy( lenbuf, rip->buf+sizeof(header), count );

	cp = rip->buf+used;
	want = rip->object_length-used;
	BU_ASSERT_LONG( want, >, 0 );
	if( (got = fread( cp, 1, want, fp )) != want ) {
		bu_log("db5_get_raw_internal_fp(), want=%ld, got=%ld, database is too short\n",
			want, got );
		return -2;
	}

	/* Verify trailing magic number */
	if( rip->buf[rip->object_length-1] != DB5HDR_MAGIC2 )  {
		bu_log("db5_get_raw_internal_fp() bad magic2 -- database has become corrupted.\n expected x%x, got x%x\n",
			DB5HDR_MAGIC2, rip->buf[rip->object_length-1] );
		return -2;
	}

	BU_INIT_EXTERNAL( &rip->name );
	BU_INIT_EXTERNAL( &rip->body );
	BU_INIT_EXTERNAL( &rip->attributes );

	/* Grab name, if present */
	if( rip->h_name_present )  {
		cp += db5_decode_length( &rip->name.ext_nbytes,
			cp, rip->h_name_width );
		rip->name.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->name.ext_nbytes;
	}

	/* Point to attributes, if present */
	if( rip->a_present )  {
		cp += db5_decode_length( &rip->attributes.ext_nbytes,
			cp, rip->a_width );
		rip->attributes.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->attributes.ext_nbytes;
	}

	/* Point to body, if present */
	if( rip->b_present )  {
		cp += db5_decode_length( &rip->body.ext_nbytes,
			cp, rip->b_width );
		rip->body.ext_buf = (genptr_t)cp;	/* discard const */
		cp += rip->body.ext_nbytes;
	}

	return 0;		/* success */
}

/**
 *			D B 5 _ E X P O R T _ O B J E C T 3
 *
 *  A routine for merging together the three optional
 *  parts of an object into the final on-disk format.
 *  Results in extra data copies, but serves as a starting point for testing.
 *  Any of name, attrib, and body may be null.
 */
void
db5_export_object3(
	struct bu_external *out,
	int dli,
	const char *name,
	const unsigned char hidden,
	const struct bu_external *attrib,
	const struct bu_external *body,
	int major,
	int minor,
	int a_zzz,
	int b_zzz )
{
	struct db5_ondisk_header *odp;
	register unsigned char	*cp;
	long	namelen = 0;
	long	need;
	int	h_width, n_width, a_width, b_width;
	long	togo;

	/*
	 *  First, compute an upper bound on the size buffer needed.
	 *  Over-estimate on the length fields just to keep it simple.
	 */
	need = sizeof(struct db5_ondisk_header);
	need += 8;	/* for object_length */
	if( name )  {
		namelen = strlen(name) + 1;	/* includes null */
		if( namelen > 1 )  {
			n_width = db5_select_length_encoding(namelen);
			need += namelen + db5_enc_len[n_width];
		} else {
			name = NULL;
			namelen = 0;
			n_width = 0;
		}
	} else {
		n_width = 0;
	}
	if( attrib )  {
		BU_CK_EXTERNAL(attrib);
		if( attrib->ext_nbytes > 0 )  {
			a_width = db5_select_length_encoding(attrib->ext_nbytes);
			need += attrib->ext_nbytes + db5_enc_len[a_width];
		} else {
			attrib = NULL;
			a_width = 0;
		}
	} else {
		a_width = 0;
	}
	if( body )  {
		BU_CK_EXTERNAL(body);
		if( body->ext_nbytes > 0 )  {
			b_width = db5_select_length_encoding(body->ext_nbytes);
			need += body->ext_nbytes + db5_enc_len[b_width];
		} else {
			body = NULL;
			b_width = 0;
		}
	} else {
		b_width = 0;
	}
	need += 8;	/* pad and magic2 */

	/* Allocate the buffer for the combined external representation */
	out->ext_magic = BU_EXTERNAL_MAGIC;
	out->ext_buf = bu_malloc( need, "external object3" );
	out->ext_nbytes = need;		/* will be trimmed, below */

	/* Determine encoding for the header length field */
	h_width = db5_select_length_encoding( (need+7)>>3 );

	/* prepare combined external object */
	odp = (struct db5_ondisk_header *)out->ext_buf;
	odp->db5h_magic1 = DB5HDR_MAGIC1;

	/* hflags */
	odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
			(dli & DB5HDR_HFLAGS_DLI_MASK);
	if( name )  {
		odp->db5h_hflags |= DB5HDR_HFLAGS_NAME_PRESENT |
			(n_width << DB5HDR_HFLAGS_NAME_WIDTH_SHIFT);
		
	}
	if( hidden ) {
		odp->db5h_hflags |= DB5HDR_HFLAGS_HIDDEN_OBJECT;
	}

	/* aflags */
	odp->db5h_aflags = a_width << DB5HDR_AFLAGS_WIDTH_SHIFT;
	if( attrib )  odp->db5h_aflags |= DB5HDR_AFLAGS_PRESENT;
	odp->db5h_aflags |= a_zzz & DB5HDR_AFLAGS_ZZZ_MASK;

	/* bflags */
	odp->db5h_bflags = b_width << DB5HDR_BFLAGS_WIDTH_SHIFT;
	if( body )  odp->db5h_bflags |= DB5HDR_BFLAGS_PRESENT;
	odp->db5h_bflags |= b_zzz & DB5HDR_BFLAGS_ZZZ_MASK;

	if( a_zzz || b_zzz )  bu_bomb("db5_export_object3: compression not supported yet\n");

	/* Object_Type */
	odp->db5h_major_type = major;
	odp->db5h_minor_type = minor;

	/* Build up the rest of the record */
	cp = ((unsigned char *)out->ext_buf) + sizeof(struct db5_ondisk_header);
	cp = db5_encode_length( cp, 7L, h_width );	/* will be replaced below */

	if( name )  {
		cp = db5_encode_length( cp, namelen, n_width );
		bcopy( name, cp, namelen );	/* includes null */
		cp += namelen;
	}

	if( attrib )  {
		/* minimum buffer length is a one byte attribute name, followed by a NULL name termination,
		 * followed by no bytes (for an empty value), followed by a NULL value termination,
		 * followed by a NULL attribute-value termination. Minimum is 4 bytes
		 */
BU_ASSERT_PTR( attrib->ext_nbytes, >=, 4 );
		cp = db5_encode_length( cp, attrib->ext_nbytes, a_width );
		bcopy( attrib->ext_buf, cp, attrib->ext_nbytes );
		cp += attrib->ext_nbytes;
	}

	if( body )  {
		cp = db5_encode_length( cp, body->ext_nbytes, b_width );
		bcopy( body->ext_buf, cp, body->ext_nbytes );
		cp += body->ext_nbytes;
	}

	togo = cp - ((unsigned char *)out->ext_buf) + 1;
	togo &= 7;
	if( togo != 0 )  {
		togo = 8 - togo;
		while( togo-- > 0 )  *cp++ = '\0';
	}
	*cp++ = DB5HDR_MAGIC2;

	/* Verify multiple of 8 */
	togo = cp - ((unsigned char *)out->ext_buf);
	BU_ASSERT_LONG( togo&7, ==, 0 );

	/* Finally, go back to the header and write the actual object length */
	cp = ((unsigned char *)out->ext_buf) + sizeof(struct db5_ondisk_header);
	cp = db5_encode_length( cp, togo>>3, h_width );

	out->ext_nbytes = togo;
	BU_ASSERT_LONG( out->ext_nbytes, >=, 8 );
}

/**
 *			D B 5 _ M A K E _ F R E E _ O B J E C T _ H D R
 *
 *  Make only the front (header) portion of a free object.
 *  This is used when operating on very large contiguous free objects
 *  in the database (e.g. 50 MBytes).
 */
void
db5_make_free_object_hdr( struct bu_external *ep, long length )
{
	struct db5_ondisk_header *odp;
	int		h_width;
	unsigned char	*cp;

	BU_CK_EXTERNAL(ep);

	BU_ASSERT_LONG( length, >=, 8 );
	BU_ASSERT_LONG( length&7, ==, 0 );

	/* Reserve enough space to hold any free header, even w/64-bit len */
	ep->ext_nbytes = 8+8;
	ep->ext_buf = bu_calloc( 1, ep->ext_nbytes, "db5_make_free_object_hdr" );

	/* Determine encoding for the header length field */
	h_width = db5_select_length_encoding( length>>3 );

	/* prepare header of external object */
	odp = (struct db5_ondisk_header *)ep->ext_buf;
	odp->db5h_magic1 = DB5HDR_MAGIC1;
	odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
			DB5HDR_HFLAGS_DLI_FREE_STORAGE;

	cp = ((unsigned char *)ep->ext_buf) + sizeof(struct db5_ondisk_header);
	cp = db5_encode_length( cp, length>>3, h_width );
}

/**
 *			D B 5 _ M A K E _ F R E E _ O B J E C T
 *
 *  Make a complete, zero-filled, free object.
 *  Note that free objects can sometimes get quite large.
 */
void
db5_make_free_object( struct bu_external *ep, long length )
{
	struct db5_ondisk_header *odp;
	int		h_width;
	unsigned char	*cp;

	BU_CK_EXTERNAL(ep);

	BU_ASSERT_LONG( length, >=, 8 );
	BU_ASSERT_LONG( length&7, ==, 0 );

	ep->ext_buf = bu_calloc( 1, length, "db5_make_free_object" );
	ep->ext_nbytes = length;

	/* Determine encoding for the header length field */
	h_width = db5_select_length_encoding( length>>3 );

	/* prepare combined external object */
	odp = (struct db5_ondisk_header *)ep->ext_buf;
	odp->db5h_magic1 = DB5HDR_MAGIC1;
	odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
			DB5HDR_HFLAGS_DLI_FREE_STORAGE;

	cp = ((unsigned char *)ep->ext_buf) + sizeof(struct db5_ondisk_header);
	cp = db5_encode_length( cp, length>>3, h_width );

	cp = ((unsigned char *)ep->ext_buf) + length-1;
	*cp = DB5HDR_MAGIC2;
}

/**
 *			D B 5 _ I M P O R T _ A T T R I B U T E S
 *
 *  Convert the on-disk encoding into a handy easy-to-use
 *  bu_attribute_value_set structure.
 *  Take advantage of the readonly_min/readonly_max capability
 *  so that we don't have to bu_strdup() each string, but can
 *  simply point to it in the provided buffer *ap.
 *  Important implication:  don't free *ap until you're done with this avs.
 *
 *  The upshot of this is that bu_avs_add() and bu_avs_remove() can
 *  be safely used with this *avs.
 *
 *  Returns -
 *	>0	count of attributes successfully imported
 *	-1	Error, mal-formed input
 */
int
db5_import_attributes( struct bu_attribute_value_set *avs, const struct bu_external *ap )
{
	const char	*cp;
	const char	*ep;
	int		count = 0;
	struct bu_attribute_value_pair *app;

	BU_CK_EXTERNAL(ap);

	BU_ASSERT_LONG( ap->ext_nbytes, >=, 4 );

	/* First pass -- count number of attributes */
	cp = (const char *)ap->ext_buf;
	ep = (const char *)ap->ext_buf+ap->ext_nbytes;

	/* Null "name" string indicates end of attribute list */
	while( *cp != '\0' )  {
	    const char *name = cp;
	    if( cp >= ep )  {
		bu_log("db5_import_attributes() ran off end of buffer, database is probably corrupted\n");
		return -1;
	    }
	    cp += strlen(cp)+1;	/* value */
	    cp += strlen(cp)+1;	/* next name */
	    count++;
	}
	/* Ensure we're exactly at the end */
	BU_ASSERT_PTR( cp+1, ==, ep );

	/* not really needed for AVS_ADD since bu_avs_add will
	 * incrementally allocate as it needs it. but one alloc is
	 * better than many in case there are many attributes.
	 */
	bu_avs_init( avs, count, "db5_import_attributes" );

	/* Second pass -- populate attributes.  Peek inside struct for non AVS_ADD. */

/* Use AVS_ADD to copy the values from the external buffer instead of
 * using them directly without copying.  This presumes ap will not get
 * free'd before we're done with the avs.  Preference should be to
 * leave AVS_ADD undefined for performance reasons.
 */
#define AVS_ADD 1

#if AVS_ADD
	cp = (const char *)ap->ext_buf;
	while( *cp != '\0' )  {
	    const char *name = cp;  /* name */
	    cp += strlen(cp)+1; /* value */
	    bu_avs_add( avs, name, cp );
	    cp += strlen(cp)+1; /* next name */
	}
#else
	/* Conserve malloc/free activity -- use strings in input buffer */
	/* Signal region of memory that input comes from */
	cp = (const char *)ap->ext_buf;
	app = avs->avp;
	while( *cp != '\0' )  {
	    app->name = cp;  /* name */
	    cp += strlen(cp)+1;
	    app->value = cp;  /* value */
	    cp += strlen(cp)+1;
	    app++;
	    avs->count++;
	}

	/* expand the readonly section if necessary */
	if ( (!avs->readonly_min) || ((genptr_t)avs->readonly_min > (genptr_t)ap->ext_buf) ) {
	    avs->readonly_min = (genptr_t)ap->ext_buf;
	}
	if ( (!avs->readonly_max) || ((genptr_t)avs->readonly_max < (genptr_t)(avs->readonly_min + ap->ext_nbytes)) ) {
	    avs->readonly_max = (genptr_t)avs->readonly_min + ap->ext_nbytes;
	}
#endif

	BU_ASSERT_PTR( cp+1, ==, ep );
	BU_ASSERT_LONG( avs->count, <=, avs->max );
	BU_ASSERT_LONG( avs->count, ==, count );

	if(bu_debug & BU_DEBUG_AVS) {
	    bu_avs_print(avs, "db5_import_attributes");
	}
	return avs->count;
}

/**
 *			D B 5 _ E X P O R T _ A T T R I B U T E S
 *
 *  Encode the attribute-value pair information into the external
 *  on-disk format.
 *
 *  The on-disk encoding is:
 *
 *	aname1 NULL value1 NULL ... anameN NULL valueN NULL NULL
 *
 *  'ext' is initialized on behalf of the caller.
 */
void
db5_export_attributes( struct bu_external *ext, const struct bu_attribute_value_set *avs )
{
	int	need = 0;
	const struct bu_attribute_value_pair	*avpp;
	char	*cp;
	int	i;

	BU_CK_AVS( avs );
	avpp = avs->avp;

	BU_INIT_EXTERNAL(ext);
	if( avs->count <= 0 )  return;
if(bu_debug & BU_DEBUG_AVS)  bu_avs_print(avs, "db5_export_attributes");

	/* First pass -- determine how much space is required */
	for( i = 0; i < avs->count; i++, avpp++ )  {
		need += strlen( avpp->name ) + strlen( avpp->value ) + 2;
	}
	if( need <= 0 )  return;
	need += 1;		/* for final null */

	ext->ext_nbytes = need;
	ext->ext_buf = bu_malloc( need, "external attributes" );

	/* Second pass -- store in external form */
	cp = (char *)ext->ext_buf;
	avpp = avs->avp;
	for( i = 0; i < avs->count; i++, avpp++ )  {
		need = strlen( avpp->name ) + 1;
		bcopy( avpp->name, cp, need );
		cp += need;

		need = strlen( avpp->value ) + 1;
		bcopy( avpp->value, cp, need );
		cp += need;
	}
	*cp++ = '\0';
	need = cp - ((char *)ext->ext_buf);
	BU_ASSERT_LONG( need, ==, ext->ext_nbytes );
}



/**
 *			D B 5 _ R E P L A C E _ A T T R I B U T E S
 *
 *  Replace the attributes of a given database object.
 *  For efficiency, this is done without looking at the object body at all.
 *
 *  Contents of the bu_attribute_value_set are freed, but not the struct itself.
 *
 *  Returns -
 *	0 on success
 *	<0 on error
 */
int
db5_replace_attributes( struct directory *dp, struct bu_attribute_value_set *avsp, struct db_i *dbip )
{
	struct bu_external	ext;
	struct db5_raw_internal	raw;
	struct bu_external	attr;
	struct bu_external	ext2;
	int			ret;

	RT_CK_DIR(dp);
	BU_CK_AVS(avsp);
	RT_CK_DBI(dbip);

	if(RT_G_DEBUG&DEBUG_DB)  {
		bu_log("db5_replace_attributes(%s) dbip=x%x\n",
			dp->d_namep, dbip );
		bu_avs_print( avsp, "new attributes" );
	}

	if( dbip->dbi_read_only )  {
		bu_log("db5_replace_attributes(%s):  READ-ONLY file\n",
			dbip->dbi_filename);
		return -1;
	}

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_log("db5_replace_attributes(%s):  import failure\n",
			dp->d_namep );
		bu_free_external( &ext );
		return -3;
	}

	db5_export_attributes( &attr, avsp );
	BU_INIT_EXTERNAL(&ext2);
	db5_export_object3( &ext2,
		raw.h_dli,
		dp->d_namep,
		raw.h_name_hidden,
		&attr,
		&raw.body,
		raw.major_type, raw.minor_type,
		raw.a_zzz, raw.b_zzz );

	/* Write it */
	ret = db_put_external5( &ext2, dp, dbip );
	if( ret < 0 )  bu_log("db5_update_attributes(%s):  db_put_external5() failure\n",
		dp->d_namep );

	bu_free_external( &attr );
	bu_free_external( &ext2 );
	bu_free_external( &ext );		/* 'raw' is now invalid */
	bu_avs_free( avsp );

	return ret;
}

/**
 *			D B 5 _ U P D A T E _ A T T R I B U T E S
 *
 *  Update an arbitrary number of attributes on a given database object.
 *  For efficiency, this is done without looking at the object body at all.
 *
 *  Contents of the bu_attribute_value_set are freed, but not the struct itself.
 *
 *  Returns -
 *	0 on success
 *	<0 on error
 */
int
db5_update_attributes( struct directory *dp, struct bu_attribute_value_set *avsp, struct db_i *dbip )
{
	struct bu_external	ext;
	struct db5_raw_internal	raw;
	struct bu_attribute_value_set old_avs;
	struct bu_external	attr;
	struct bu_external	ext2;
	int			ret;

	RT_CK_DIR(dp);
	BU_CK_AVS(avsp);
	RT_CK_DBI(dbip);

	if(RT_G_DEBUG&DEBUG_DB)  {
		bu_log("db5_update_attributes(%s) dbip=x%x\n",
			dp->d_namep, dbip );
		bu_avs_print( avsp, "new attributes" );
	}

	if( dbip->dbi_read_only )  {
		bu_log("db5_update_attributes(%s):  READ-ONLY file\n",
			dbip->dbi_filename);
		return -1;
	}

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_log("db5_update_attributes(%s):  import failure\n",
			dp->d_namep );
		bu_free_external( &ext );
		return -3;
	}

	bu_avs_init( &old_avs, 4, "db5_update_attributes" );
	if( raw.attributes.ext_buf )  {
		if( db5_import_attributes( &old_avs, &raw.attributes ) < 0 )  {
			bu_log("db5_update_attributes(%s):  mal-formed attributes in database\n",
				dp->d_namep );
			bu_free_external( &ext );
			return -8;
		}
	}

	bu_avs_merge( &old_avs, avsp );

	db5_export_attributes( &attr, &old_avs );
	BU_INIT_EXTERNAL(&ext2);
	db5_export_object3( &ext2,
		raw.h_dli,
		dp->d_namep,
		raw.h_name_hidden,
		&attr,
		&raw.body,
		raw.major_type, raw.minor_type,
		raw.a_zzz, raw.b_zzz );

	/* Write it */
	ret = db_put_external5( &ext2, dp, dbip );
	if( ret < 0 )  bu_log("db5_update_attributes(%s):  db_put_external5() failure\n",
		dp->d_namep );

	bu_free_external( &attr );
	bu_free_external( &ext2 );
	bu_free_external( &ext );		/* 'raw' is now invalid */
	bu_avs_free( &old_avs );
	bu_avs_free( avsp );

	return ret;
}

/**
 *			D B 5 _ U P D A T E _ A T T R I B U T E
 *
 *  A convenience routine to update the value of a single attribute.
 *
 *  Returns -
 *	0 on success
 *	<0 on error
 */
int
db5_update_attribute( const char *obj_name, const char *aname, const char *value, struct db_i *dbip )
{
	struct directory	*dp;
	struct bu_attribute_value_set avs;

	RT_CK_DBI(dbip);
	if( (dp = db_lookup( dbip, obj_name, LOOKUP_NOISY )) == DIR_NULL )
		return -1;

	bu_avs_init( &avs, 2, "db5_update_attribute" );
	bu_avs_add( &avs, aname, value );

	return db5_update_attributes( dp, &avs, dbip );
}

/**
 *			D B 5 _ U P D A T E _ I D E N T
 *
 *  Update the _GLOBAL object, which in v5 serves the place of the
 *  "ident" header record in v4 as the place to stash global information.
 *  Since every database will have one of these things,
 *  it's no problem to update it.
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int db5_update_ident( struct db_i *dbip, const char *title, double local2mm )
{
	struct bu_attribute_value_set	avs;
	struct directory		*dp;
	struct bu_vls			units;
	int				ret;

	RT_CK_DBI(dbip);

	if( (dp = db_lookup( dbip, DB5_GLOBAL_OBJECT_NAME, LOOKUP_QUIET )) == DIR_NULL )  {
		struct bu_external	global;
		unsigned char		minor_type=0;

		bu_log("db5_update_ident() WARNING: %s object is missing, creating new one.\nYou may have lost important global state when you deleted this object.\n",
			DB5_GLOBAL_OBJECT_NAME );

		/* OK, make one.  It will be empty to start with, updated below. */
		db5_export_object3( &global,
			DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, NULL, NULL,
			DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0,
			DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );

		dp = db_diradd( dbip, DB5_GLOBAL_OBJECT_NAME, -1L, 0, 0, (genptr_t)&minor_type);
		dp->d_major_type = DB5_MAJORTYPE_ATTRIBUTE_ONLY;
		if( db_put_external( &global, dp, dbip ) < 0 )  {
			bu_log("db5_update_ident() unable to create replacement %s object!\n", DB5_GLOBAL_OBJECT_NAME );
			bu_free_external(&global);
			return -1;
		}
		bu_free_external(&global);
	}

	bu_vls_init( &units );
	bu_vls_printf( &units, "%.25e", local2mm );

	bu_avs_init( &avs, 4, "db5_update_ident" );
	bu_avs_add( &avs, "title", title );
	bu_avs_add( &avs, "units", bu_vls_addr(&units) );

	ret = db5_update_attributes( dp, &avs, dbip );
	bu_vls_free( &units );

	dbip->dbi_title = bu_strdup(title);

	return ret;
}

/**
 *			D B 5 _ F W R I T E _ I D E N T
 *
 *  Create a header for a v5 database.
 *  This routine has the same calling sequence as db_fwrite_ident()
 *  which makes a v4 database header.
 *
 *  In the v5 database, two database objects must be created to
 *  match the semantics of what was in the v4 header:
 *
 *  First, a database header object.
 *
 *  Second, create a specially named attribute-only object which
 *  contains the attributes "title=" and "units=".
 *
 *  This routine should only be used by db_create().
 *  Everyone else should use db5_update_ident().
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db5_fwrite_ident(FILE *fp, const char *title, double local2mm)
{
	struct bu_attribute_value_set avs;
	struct bu_vls		units;
	struct bu_external	out;
	struct bu_external	attr;

	if( local2mm <= 0 )  {
		bu_log("db5_fwrite_ident(%s, %g) local2mm <= 0\n",
			title, local2mm );
		return -1;
	}

	/* First, write the header object */
	db5_export_object3( &out, DB5HDR_HFLAGS_DLI_HEADER_OBJECT,
		NULL, 0, NULL, NULL,
		DB5_MAJORTYPE_RESERVED, 0,
		DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );
	if( bu_fwrite_external( fp, &out ) < 0 )  return -1;
	bu_free_external( &out );

	/* Second, create the attribute-only object */
	bu_vls_init( &units );
	bu_vls_printf( &units, "%.25e", local2mm );

	bu_avs_init( &avs, 4, "db5_fwrite_ident" );
	bu_avs_add( &avs, "title", title );
	bu_avs_add( &avs, "units", bu_vls_addr(&units) );

	db5_export_attributes( &attr, &avs );
	db5_export_object3( &out, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		DB5_GLOBAL_OBJECT_NAME, DB5HDR_HFLAGS_HIDDEN_OBJECT, &attr, NULL,
		DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0,
		DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );
	if( bu_fwrite_external( fp, &out ) < 0 )  return -1;
	bu_free_external( &out );
	bu_free_external( &attr );
	bu_avs_free( &avs );

	bu_vls_free( &units );

	return 0;
}

/**
 *			R T _ D B _ C V T _ T O _ E X T E R N A L 5
 *
 *  The attributes are taken from ip->idb_avs
 *
 *  If present, convert attributes to on-disk format.
 *  This must happen after exporting the body, in case the
 *  ft_export5() method happened to extend the attribute set.
 *  Combinations are one "solid" which does this.
 *
 *  The internal representation is NOT freed, that's the caller's job.
 *
 *  The 'ext' pointer is accepted in uninitialized form, and
 *  an initialized structure is always returned, so that
 *  the caller may free it even when an error return is given.
 *
 *  Returns -
 *	0	OK
 *	-1	FAIL
 */
int
rt_db_cvt_to_external5(
	struct bu_external *ext,
	const char *name,
	const struct rt_db_internal *ip,
	double conv2mm,
	struct db_i *dbip,
	struct resource *resp,
	const int major)
{
	struct bu_external	attributes;
	struct bu_external	body;
	int			minor;

	RT_CK_DB_INTERNAL( ip );
	if(dbip) RT_CK_DBI(dbip);	/* may be null */
	RT_CK_RESOURCE(resp);
	BU_INIT_EXTERNAL( &body );

	minor = ip->idb_type;	/* XXX not necessarily v5 numbers. */

	/* Scale change on export is 1.0 -- no change */
	if( ip->idb_meth->ft_export5( &body, ip, conv2mm, dbip, resp, minor ) < 0 )  {
		bu_log("rt_db_cvt_to_external5(%s):  ft_export5 failure\n",
			name);
		bu_free_external( &body );
		BU_INIT_EXTERNAL(ext);
		return -1;		/* FAIL */
	}
	BU_CK_EXTERNAL( &body );

	/* If present, convert attributes to on-disk format. */
	if( ip->idb_avs.magic == BU_AVS_MAGIC )  {
		db5_export_attributes( &attributes, &ip->idb_avs );
		BU_CK_EXTERNAL( &attributes );
	} else {
		BU_INIT_EXTERNAL(&attributes);
	}

	db5_export_object3( ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		name, 0, &attributes, &body,
		major, minor,
		DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED );
	BU_CK_EXTERNAL( ext );
	bu_free_external( &body );
	bu_free_external( &attributes );

	return 0;		/* OK */
}

/*
 *			D B _ W R A P _ V 5 _ E X T E R N A L
 *
 *  Modify name of external object, if necessary.
 */
int
db_wrap_v5_external( struct bu_external *ep, const char *name )
{
	struct db5_raw_internal	raw;
	struct bu_external	tmp;

	BU_CK_EXTERNAL(ep);

	/* Crack the external form into parts */
	if( db5_get_raw_internal_ptr( &raw, (unsigned char *)ep->ext_buf ) == NULL )  {
		bu_log("db_put_external5(%s) failure in db5_get_raw_internal_ptr()\n",
			name);
		return -1;
	}
	BU_ASSERT_LONG( raw.h_dli, ==, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT );

	/* See if name needs to be changed */
	if( raw.name.ext_buf == NULL || strcmp( name, raw.name.ext_buf ) != 0 )  {
		/* Name needs to be changed.  Create new external form.
		 * Make temporary copy so input isn't smashed
		 * as new external object is constructed.
		 */
		tmp = *ep;		/* struct copy */
		BU_INIT_EXTERNAL(ep);

		db5_export_object3( ep,
			DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			name,
			raw.h_name_hidden,
			&raw.attributes,
			&raw.body,
			raw.major_type, raw.minor_type,
			raw.a_zzz, raw.b_zzz );
		/* 'raw' is invalid now, 'ep' has new external form. */
		bu_free_external( &tmp );
		return 0;
	}

	/* No changes needed, input object is properly named */
	return 0;
}


/**
 *
 *			D B _ P U T _ E X T E R N A L 5
 *
 *  Given that caller already has an external representation of
 *  the database object,  update it to have a new name
 *  (taken from dp->d_namep) in that external representation,
 *  and write the new object into the database, obtaining different storage if
 *  the size has changed.
 *
 *  Changing the name on a v5 object is a relatively expensive operation.
 *
 *  Caller is responsible for freeing memory of external representation,
 *  using bu_free_external().
 *
 *  This routine is used to efficiently support MGED's "cp" and "keep"
 *  commands, which don't need to import and decompress
 *  objects just to rename and copy them.
 *
 *  Returns -
 *	-1	error
 *	 0	success
 */
int
db_put_external5(struct bu_external *ep, struct directory *dp, struct db_i *dbip)
{
	RT_CK_DBI(dbip);
	RT_CK_DIR(dp);
	BU_CK_EXTERNAL(ep);

	if(RT_G_DEBUG&DEBUG_DB) bu_log("db_put_external5(%s) ep=x%x, dbip=x%x, dp=x%x\n",
		dp->d_namep, ep, dbip, dp );

	if( dbip->dbi_read_only )  {
		bu_log("db_put_external5(%s):  READ-ONLY file\n",
			dbip->dbi_filename);
		return -1;
	}

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	/* First, change the name. */
	if( db_wrap_v5_external( ep, dp->d_namep ) < 0 )  {
		bu_log("db_put_external5(%s) failure in db_wrap_v5_external()\n",
			dp->d_namep);
		return -1;
	}

	/* Second, obtain storage for final object */
	if( ep->ext_nbytes != dp->d_len || dp->d_addr == -1L )  {
		if( db5_realloc( dbip, dp, ep ) < 0 )  {
			bu_log("db_put_external(%s) db_realloc5() failed\n", dp->d_namep);
			return -5;
		}
	}
	BU_ASSERT_LONG( ep->ext_nbytes, ==, dp->d_len );

	if( dp->d_flags & RT_DIR_INMEM )  {
		bcopy( (char *)ep->ext_buf, dp->d_un.ptr, ep->ext_nbytes );
		return 0;
	}

	if( db_write( dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr ) < 0 )  {
		return -1;
	}
	return 0;
}

/**
 *			R T _ D B _ P U T _ I N T E R N A L 5
 *
 *  Convert the internal representation of a solid to the external one,
 *  and write it into the database.
 *
 *  Applications and middleware shouldn't call this directly, they
 *  should use the version-generic interface "rt_db_put_internal()".
 *
 *  The internal representation is always freed.
 *  (Not the pointer, just the contents).
 *
 *  Returns -
 *	<0	error
 *	 0	success
 */
int
rt_db_put_internal5(
	struct directory	*dp,
	struct db_i		*dbip,
	struct rt_db_internal	*ip,
	struct resource		*resp,
	const int		major)
{
	struct bu_external	ext;

	RT_CK_DIR(dp);
	RT_CK_DBI(dbip);
	RT_CK_DB_INTERNAL( ip );
	RT_CK_RESOURCE(resp);

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if( rt_db_cvt_to_external5( &ext, dp->d_namep, ip, 1.0, dbip, resp, major ) < 0 )  {
		bu_log("rt_db_put_internal5(%s):  export failure\n",
			dp->d_namep);
		goto fail;
	}
	BU_CK_EXTERNAL( &ext );

	if( ext.ext_nbytes != dp->d_len || dp->d_addr == -1L )  {
		if( db5_realloc( dbip, dp, &ext ) < 0 )  {
			bu_log("rt_db_put_internal5(%s) db_realloc5() failed\n", dp->d_namep);
			goto fail;
		}
	}
	BU_ASSERT_LONG( ext.ext_nbytes, ==, dp->d_len );

	if( dp->d_flags & RT_DIR_INMEM )  {
		bcopy( (char *)ext.ext_buf, dp->d_un.ptr, ext.ext_nbytes );
		goto ok;
	}

	if( db_write( dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp->d_addr ) < 0 )  {
		goto fail;
	}
ok:
	bu_free_external( &ext );
	rt_db_free_internal( ip, resp );
	return 0;			/* OK */

fail:
	bu_free_external( &ext );
	rt_db_free_internal( ip, resp );
	return -2;		/* FAIL */
}

/**
 *			R T _ D B _ E X T E R N A L 5 _ T O _ I N T E R N A L 5
 *
 *  Given an object in external form, convert it to internal form.
 *  The caller is responsible for freeing the external form.
 *
 *  Returns -
 *	<0	On error
 *	id	On success.
 */
int
rt_db_external5_to_internal5(
	struct rt_db_internal		*ip,
	const struct bu_external	*ep,
	const char			*name,
	const struct db_i		*dbip,
	const mat_t			mat,
	struct resource			*resp)
{
	register int		id;
	struct db5_raw_internal	raw;

	BU_CK_EXTERNAL(ep);
	RT_CK_DB_INTERNAL(ip);
	RT_CK_DBI(dbip);

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if (db5_get_raw_internal_ptr(&raw, ep->ext_buf) == NULL) {
		bu_log("rt_db_external5_to_internal5(%s):  import failure\n",
			name );
		return -3;
	}

	if(( raw.major_type == DB5_MAJORTYPE_BRLCAD )
	 ||( raw.major_type == DB5_MAJORTYPE_BINARY_UNIF)) {
		/* As a convenience to older ft_import routines */
		if( mat == NULL )  mat = bn_mat_identity;
	} else {
		bu_log("rt_db_external5_to_internal5(%s):  unable to import non-BRL-CAD object, major=%d\n",
			name, raw.major_type );
		return -1;		/* FAIL */
	}

	if (ip->idb_avs.magic != BU_AVS_MAGIC) {
	    bu_avs_init_empty( &ip->idb_avs );
	}

	/* If attributes are present in the object, make them available
	 * in the internal form.
	 */
	if( raw.attributes.ext_buf )  {
		if( db5_import_attributes( &ip->idb_avs, &raw.attributes ) < 0 )  {
			bu_log("rt_db_external5_to_internal5(%s):  mal-formed attributes in database\n",
				name );
			return -8;
		}
	}

	if( !raw.body.ext_buf )  {
		bu_log("rt_db_external5_to_internal5(%s):  object has no body\n",
			name );
		return -4;
	}

	/*
	 *	XXX	This is a kludge, but it works for starters
	 */
	switch ( raw.major_type ) {
	    case DB5_MAJORTYPE_BRLCAD:
		id = raw.minor_type; break;
	    case DB5_MAJORTYPE_BINARY_UNIF:
		id = ID_BINUNIF; break;
	    default:
		bu_log("rt_db_external5_to_internal5(%s): don't yet handle major_type %d\n", name, raw.major_type);
		return -1;
	}
	/* ip has already been initialized, and should not be re-initted */
	if( rt_functab[id].ft_import5( ip, &raw.body, mat, dbip, resp, raw.minor_type ) < 0 )  {
		bu_log("rt_db_external5_to_internal5(%s):  import failure\n",
			name );
		rt_db_free_internal( ip, resp );
		return -1;		/* FAIL */
	}
	/* Don't free &raw.body */

	RT_CK_DB_INTERNAL( ip );
	ip->idb_major_type = raw.major_type;
	ip->idb_minor_type = raw.minor_type;
	ip->idb_meth = &rt_functab[id];

	return id;			/* OK */
}

/**
 *			R T _ D B _ G E T _ I N T E R N A L 5
 *
 *  Get an object from the database, and convert it into it's internal
 *  representation.
 *
 *  Applications and middleware shouldn't call this directly, they
 *  should use the generic interface "rt_db_get_internal()".
 *
 *  Returns -
 *	<0	On error
 *	id	On success.
 */
int
rt_db_get_internal5(
	struct rt_db_internal	*ip,
	const struct directory	*dp,
	const struct db_i	*dbip,
	const mat_t		mat,
	struct resource		*resp)
{
	struct bu_external	ext;
	int			ret;

	BU_INIT_EXTERNAL(&ext);
	RT_INIT_DB_INTERNAL(ip);

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	ret = rt_db_external5_to_internal5( ip, &ext, dp->d_namep, dbip, mat, resp );
	bu_free_external(&ext);
	return ret;
}

/*
 *  XXX The material head should be attached to the db_i, not global.
 */
void
db5_export_color_table( struct bu_vls *ostr, struct db_i *dbip )
{
	struct mater *mp;

	BU_CK_VLS(ostr);
	RT_CK_DBI(dbip);

	for( mp = rt_material_head; mp != MATER_NULL; mp = mp->mt_forw )  {
		bu_vls_printf(ostr,
			"{%d %d %d %d %d} ",
			mp->mt_low,
			mp->mt_high,
			mp->mt_r,
			mp->mt_g,
			mp->mt_b );
	}
}

/**
 *			D B 5 _ I M P O R T _ C O L O R _ T A B L E
 */
void
db5_import_color_table( char *cp )
{
	char	*sp = cp;
	int	low, high, r, g, b;

	while( (sp = strchr( sp, '{' )) != NULL )  {
		sp++;
		if( sscanf( sp, "%d %d %d %d %d", &low, &high, &r, &g, &b ) != 5 )  break;
		rt_color_addrec( low, high, r, g, b, -1L );
	}
}

/**
 *			D B 5 _ P U T _ C O L O R _ T A B L E
 *
 *  Put the old region-id-color-table into the global object.
 *  A null attribute is set if the material table is empty.
 *
 *  Returns -
 *	<0	error
 *	0	OK
 */
int
db5_put_color_table( struct db_i *dbip )
{
	struct bu_vls	str;
	int	ret;

	RT_CK_DBI(dbip);
	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	bu_vls_init(&str);
	db5_export_color_table( &str, dbip );

	ret = db5_update_attribute( DB5_GLOBAL_OBJECT_NAME,
		"regionid_colortable", bu_vls_addr(&str), dbip );

	bu_vls_free( &str );
	return ret;
}

/**			D B _ G E T _ A T T R I B U T E S
 *
 *	Get attributes for an object pointed to by *dp
 *
 *	returns:
 *		0 - all is well
 *		<0 - error
 */
int
db5_get_attributes( const struct db_i *dbip, struct bu_attribute_value_set *avs, const struct directory *dp )
{
	struct bu_external	ext;
	struct db5_raw_internal	raw;

	RT_CK_DBI( dbip );

	if( dbip->dbi_version < 5 )
		return 0;	/* not an error, just no attributes */

	RT_CK_DIR( dp );

	BU_INIT_EXTERNAL(&ext);

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -1;		/* FAIL */

	if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
		bu_free_external( &ext );
		return -2;
	}

	if( raw.attributes.ext_buf )  {
		if( db5_import_attributes( avs, &raw.attributes ) < 0 ) {
			bu_free_external( &ext );
			return -3;
		}
	}

	bu_free_external( &ext );
	return 0;
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
