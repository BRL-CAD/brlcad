/*
 *			D B 5 _ I O . C
 *
 *  Handle import/export and IO of v5 database objects.
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

const int db5_enc_len[4] = {
	1,
	2,
	4,
	8
};

/*
 *			D B 5 _ H E A D E R _ I S _ V A L I D
 *
 *  Verify that this is a valid header for a BRL-CAD v5 database.
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
	if( (odp->db5h_hflags & DB5HDR_HFLAGS_DLI_MASK) != DB5HDR_HFLAGS_DLI_HEADER_OBJECT )
		return 0;
	if( (odp->db5h_hflags & DB5HDR_HFLAGS_NAME_PRESENT) )  return 0;
	if( ((odp->db5h_hflags & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >> DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* aflags */
	if( (odp->db5h_aflags & DB5HDR_AFLAGS_ZZZ_MASK) != DB5HDR_ZZZ_UNCOMPRESSED )  return 0;
	if( odp->db5h_aflags & DB5HDR_AFLAGS_PRESENT )  return 0;
	if( ((odp->db5h_aflags & DB5HDR_AFLAGS_WIDTH_MASK) >> DB5HDR_AFLAGS_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* bflags */
	if( (odp->db5h_bflags & DB5HDR_BFLAGS_ZZZ_MASK) != DB5HDR_ZZZ_UNCOMPRESSED )  return 0;
	if( odp->db5h_bflags & DB5HDR_BFLAGS_PRESENT )  return 0;
	if( ((odp->db5h_bflags & DB5HDR_BFLAGS_WIDTH_MASK) >> DB5HDR_BFLAGS_WIDTH_SHIFT)
	    != DB5HDR_WIDTHCODE_8BIT )  return 0;

	/* major and minor type */
	if( odp->db5h_major_type != DB5HDR_MAJORTYPE_RESERVED )  return 0;
	if( odp->db5h_minor_type != 0 )  return 0;

	/* Check length, known to be 8-bit.  Header len=1 8-byte chunk. */
	if( hp[6] != 1 )  return 0;

	return 1;		/* valid */
}

/*
 *			D B 5 _ S E L E C T _ L E N G T H _ E N C O D I N G
 *
 *  Given a number to encode, decide which is the smallest encoding format
 *  which will contain it.
 */
int
db5_select_length_encoding( len )
long len;
{
	if( len <= 255 )  return DB5HDR_WIDTHCODE_8BIT;
	if( len <= 65535 )  return DB5HDR_WIDTHCODE_16BIT;
	if( len < 0x7ffffffe )  return DB5HDR_WIDTHCODE_32BIT;
	return DB5HDR_WIDTHCODE_64BIT;
}

/*
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
#if 0
		if( sizeof(long) >= 8 )  {
			*lenp = BU_GLONGLONG(cp);
			return 8;
		}
#endif
		bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
	}
	bu_bomb("db5_decode_length(): unknown width code\n");
	/* NOTREACHED */
}

/*
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
db5_decode_signed( lenp, cp, format )
long			*lenp;
CONST unsigned char	*cp;
int			format;
{
	switch( format )  {
	case DB5HDR_WIDTHCODE_8BIT:
		if( (*lenp = (*cp)) & 0x80 ) *lenp |= 0xFFFFFF00;
		return 1;
	case DB5HDR_WIDTHCODE_16BIT:
		if( (*lenp = BU_GSHORT(cp)) & 0x8000 )  *lenp |= 0xFFFF0000;
		return 2;
	case DB5HDR_WIDTHCODE_32BIT:
		*lenp = BU_GLONG(cp);
		return 4;
	case DB5HDR_WIDTHCODE_64BIT:
#if 0
		if( sizeof(long) >= 8 )  {
			*lenp = BU_GLONGLONG(cp);
			return 8;
		}
#endif
		bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
	}
	bu_bomb("db5_decode_length(): unknown width code\n");
	/* NOTREACHED */
}

/*
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
		bu_bomb("db5_encode_length(): encountered 64-bit length\n");
	}
	bu_bomb("db5_encode_length(): unknown width code\n");
	/* NOTREACHED */
}

/*
 *			D B 5 _ C R A C K _ D I S K _ H E A D E R
 *
 *  Returns -
 *	0 on success
 *	-1 on error
 */
int
db5_crack_disk_header( rip, cp )
struct db5_raw_internal		*rip;
CONST unsigned char		*cp;
{
	if( cp[0] != DB5HDR_MAGIC1 )  {
		bu_log("db5_crack_disk_header() bad magic1 -- database has become corrupted\n expected x%x, got x%x\n",
			DB5HDR_MAGIC1, cp[0]);
		if( cp[0] == 'I' )
			bu_log("It looks like a v4 database was concatenated onto a v5 database.\nConvert using g4-g5 before the 'cat', or use the MGED 'concat' command.\n");
		return 0;
	}

	/* hflags */
	rip->h_dli = (cp[1] & DB5HDR_HFLAGS_DLI_MASK);
	rip->h_object_width = (cp[1] & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >>
		DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT;
	rip->h_name_present = (cp[1] & DB5HDR_HFLAGS_NAME_PRESENT);
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

	if(rt_g.debug&DEBUG_DB) bu_log("db5_crack_disk_header()\n\
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

/*
 *			D B 5 _ G E T _ R A W _ I N T E R N A L _ P T R
 *
 *  Returns -
 *	on success, pointer to first unused byte
 *	NULL, on error
 */
const unsigned char *
db5_get_raw_internal_ptr( struct db5_raw_internal *rip, const unsigned char *ip)
{
	CONST unsigned char	*cp = ip;

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
		rip->name.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->name.ext_nbytes;
	}

	/* Point to attributes, if present */
	if( rip->a_present )  {
		cp += db5_decode_length( &rip->attributes.ext_nbytes,
			cp, rip->a_width );
		rip->attributes.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->attributes.ext_nbytes;
	}

	/* Point to body, if present */
	if( rip->b_present )  {
		cp += db5_decode_length( &rip->body.ext_nbytes,
			cp, rip->b_width );
		rip->body.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->body.ext_nbytes;
	}

	rip->buf = NULL;	/* no buffer needs freeing */

	return ip + rip->object_length;
}

/*
 *			D B 5 _ G E T _ R A W _ I N T E R N A L _ F P
 *
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
		rip->name.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->name.ext_nbytes;
	}

	/* Point to attributes, if present */
	if( rip->a_present )  {
		cp += db5_decode_length( &rip->attributes.ext_nbytes,
			cp, rip->a_width );
		rip->attributes.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->attributes.ext_nbytes;
	}

	/* Point to body, if present */
	if( rip->b_present )  {
		cp += db5_decode_length( &rip->body.ext_nbytes,
			cp, rip->b_width );
		rip->body.ext_buf = (genptr_t)cp;	/* discard CONST */
		cp += rip->body.ext_nbytes;
	}

	return 0;		/* success */
}

/*
 *			D B 5 _ E X P O R T _ O B J E C T 3
 *
 *  A routine for merging together the three optional
 *  parts of an object into the final on-disk format.
 *  Results in extra data copies, but serves as a starting point for testing.
 *  Any of name, attrib, and body may be null.
 */
void
db5_export_object3( out, dli, name, attrib, body, major, minor, zzz )
struct bu_external		*out;			/* output */
int				dli;
CONST char			*name;
CONST struct bu_external	*attrib;
CONST struct bu_external	*body;
int				major;
int				minor;
int				zzz;		/* compression, someday */
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

	/* aflags */
	odp->db5h_aflags = a_width << DB5HDR_AFLAGS_WIDTH_SHIFT;
	if( attrib )  odp->db5h_aflags |= DB5HDR_AFLAGS_PRESENT;
	odp->db5h_aflags |= zzz & DB5HDR_AFLAGS_ZZZ_MASK;

	/* bflags */
	odp->db5h_bflags = b_width << DB5HDR_BFLAGS_WIDTH_SHIFT;
	if( body )  odp->db5h_bflags |= DB5HDR_BFLAGS_PRESENT;
	odp->db5h_bflags |= zzz & DB5HDR_BFLAGS_ZZZ_MASK;

	if( zzz )  bu_bomb("db5_export_object3: compression not supported yet\n");

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
BU_ASSERT_PTR( attrib->ext_nbytes, >=, 5 );
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
if(getuid()==53) bu_hexdump_external(stderr, out, "v5 ext");
}

/*
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
 *  The input *avs should not have been previously initialized.
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

	BU_ASSERT_LONG( ap->ext_nbytes, >=, 5 );

	/* First pass -- count number of attributes */
	cp = (const char *)ap->ext_buf;
	ep = (const char *)ap->ext_buf+ap->ext_nbytes;

	/* Null "name" string indicates end of attribute list */
	while( *cp != '\0' )  {
		if( cp >= ep )  {
			bu_log("db5_import_attributes() ran off end of buffer, database is corrupted\n");
			return -1;
		}
		cp += strlen(cp)+1;	/* name */
		cp += strlen(cp)+1;	/* value */
		count++;
	}
	/* Ensure we're exactly at the end */
	BU_ASSERT_PTR( cp+1, ==, ep );

	bu_avs_init( avs, count+3, "db5_import_attributes" );

	/* Signal region of memory that input comes from */
	avs->readonly_min = ap->ext_buf;
	avs->readonly_max = avs->readonly_min + ap->ext_nbytes-1;

	/* Second pass -- populate attributes.  Peek inside struct. */
	cp = (const char *)ap->ext_buf;
	app = avs->avp;
	while( *cp != '\0' )  {
		app->name = cp;
		cp += strlen(cp)+1;	/* name */
		app->value = cp;
		cp += strlen(cp)+1;	/* value */
		app++;
		avs->count++;
	}
	BU_ASSERT_PTR( cp+1, ==, ep );
	BU_ASSERT_LONG( avs->count, <, avs->max );
	BU_ASSERT_LONG( avs->count, ==, count );

	app->name = NULL;
	app->value = NULL;

	return avs->count;
}

/*
 *			D B 5 _ E X P O R T _ A T T R I B U T E S
 *
 *  One attempt at encoding attribute-value information in the external
 *  format.
 *  This may not be the best or only way to do it, but it gets things
 *  started, for testing.
 *
 *  The on-disk encoding is:
 *
 *	aname1 NULL value1 NULL ... anameN NULL valueN NULL NULL
 */
void
db5_export_attributes( struct bu_external *ext, const struct bu_attribute_value_set *avs )
{
	int	need = 0;
	CONST struct bu_attribute_value_pair	*avpp;
	char	*cp;

	BU_CK_AVS( avs );
	avpp = avs->avp;

	BU_INIT_EXTERNAL(ext);
	if( avs->count <= 0 )  return;

	/* First pass -- determine how much space is required */
	for( avpp = avs->avp; avpp->name != NULL; avpp++ )  {
		need += strlen( avpp->name ) + strlen( avpp->value ) + 2;
	}
	if( need <= 0 )  return;
	need += 1;		/* for final null */

	ext->ext_nbytes = need;
	ext->ext_buf = bu_malloc( need, "external attributes" );

	/* Second pass -- store in external form */
	cp = (char *)ext->ext_buf;
	for( avpp = avs->avp; avpp->name != NULL; avpp++ )  {
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

/*
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
 *  Second, an attribute-only object named "_GLOBAL" which
 *  contains the attributes "title=" and "units=".
 *
 * Returns -
 *	 0	Success
 *	-1	Fatal Error
 */
int
db5_fwrite_ident( fp, title, local2mm )
FILE		*fp;
CONST char	*title;
double		local2mm;
{
	struct bu_attribute_value_set avs;
	struct bu_vls		units;
	struct bu_external	out;
	struct bu_external	attr;

	/* First, write the header object */
	db5_export_object3( &out, DB5HDR_HFLAGS_DLI_HEADER_OBJECT,
		NULL, NULL, NULL,
		DB5HDR_MAJORTYPE_RESERVED, 0,
		DB5HDR_ZZZ_UNCOMPRESSED );
	bu_fwrite_external( fp, &out );
	bu_free_external( &out );

	/* Second, create the attribute-only object */
	bu_vls_init( &units );
	bu_vls_printf( &units, "%.25f", local2mm );

	bu_avs_init( &avs, 4, "db5_fwrite_ident" );
	bu_avs_add( &avs, "title", title );
	bu_avs_add( &avs, "units", bu_vls_addr(&units) );

	db5_export_attributes( &attr, &avs );
	db5_export_object3( &out, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		"_GLOBAL", &attr, NULL,
		DB5HDR_MAJORTYPE_ATTRIBUTE_ONLY, 0,
		DB5HDR_ZZZ_UNCOMPRESSED);
	bu_fwrite_external( fp, &out );
	bu_free_external( &out );
	bu_free_external( &attr );
	bu_avs_free( &avs );

	bu_vls_free( &units );

	return 0;
}

/*
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
	struct db_i *dbip)
{
	struct bu_external	attributes;
	struct bu_external	body;
	int			major, minor;

	RT_CK_DB_INTERNAL( ip );
	if(dbip) RT_CK_DBI(dbip);	/* may be null */

	/* Scale change on export is 1.0 -- no change */
	if( ip->idb_meth->ft_export5( &body, ip, conv2mm, dbip ) < 0 )  {
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

	major = DB5HDR_MAJORTYPE_BRLCAD;
	minor = ip->idb_type;	/* XXX not necessarily v5 numbers. */

	db5_export_object3( ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		name, &attributes, &body,
		major, minor,
		DB5HDR_ZZZ_UNCOMPRESSED);
	BU_CK_EXTERNAL( ext );
	bu_free_external( &body );
	bu_free_external( &attributes );

	return 0;		/* OK */
}

/*
 *			R T _ D B _ P U T _ I N T E R N A L 5
 *
 *  Convert the internal representation of a solid to the external one,
 *  and write it into the database.
 *
 *  The internal representation is always freed.
 *
 *  Returns -
 *	<0	error
 *	 0	success
 */
int
rt_db_put_internal5( dp, dbip, ip )
struct directory	*dp;
struct db_i		*dbip;
struct rt_db_internal	*ip;
{
	struct bu_external	ext;
	int			major, minor;
	int			ret;

	RT_CK_DIR(dp);
	RT_CK_DBI(dbip);
	RT_CK_DB_INTERNAL( ip );

	if( rt_db_cvt_to_external5( &ext, dp->d_namep, ip, 1.0, dbip ) < 0 )  {
		bu_log("rt_db_put_internal5(%s):  export failure\n",
			dp->d_namep);
		bu_free_external( &ext );
		rt_db_free_internal( ip );
		return -1;		/* FAIL */
	}
	BU_CK_EXTERNAL( &ext );

	if( db_put_external( &ext, dp, dbip ) < 0 )  {
		bu_free_external( &ext );
		rt_db_free_internal( ip );
		return -2;		/* FAIL */
	}

	bu_free_external( &ext );
	rt_db_free_internal( ip );
	return 0;			/* OK */
}

/*
 *			R T _ F W R I T E _ I N T E R N A L 5
 *
 *  Put an object in internal format out onto a file in external format.
 *  Used by LIBWDB.
 *
 *  The internal representation is always freed.
 *
 *  Can't really require a dbip parameter, as many callers won't have one.
 *
 *  Returns -
 *	0	OK
 *	<0	error
 */
int
rt_fwrite_internal5( fp, name, ip, conv2mm )
FILE			*fp;
CONST char		*name;
struct rt_db_internal	*ip;
double			conv2mm;
{
	struct bu_external	ext;
	int			major, minor;

	RT_CK_DB_INTERNAL(ip);
	RT_CK_FUNCTAB( ip->idb_meth );

	if( rt_db_cvt_to_external5( &ext, name, ip, conv2mm, NULL ) < 0 )  {
		bu_log("rt_fwrite_internal5(%s):  export failure\n",
			name);
		bu_free_external( &ext );
		rt_db_free_internal( ip );
		return -1;		/* FAIL */
	}
	BU_CK_EXTERNAL( &ext );

	if( bu_fwrite_external( fp, &ext ) < 0 )  {
		bu_log("rt_fwrite_internal5(%s): bu_fwrite_external() error\n",
			name );
		bu_free_external( &ext );
		rt_db_free_internal( ip );
		return -2;		/* FAIL */
	}
	bu_free_external( &ext );
	rt_db_free_internal( ip );
	return 0;

}

/*
 *			R T _ D B _ G E T _ I N T E R N A L 5
 *
 *  Get an object from the database, and convert it into it's internal
 *  representation.
 *
 *  Returns -
 *	<0	On error
 *	id	On success.
 */
int
rt_db_get_internal5( ip, dp, dbip, mat )
struct rt_db_internal	*ip;
CONST struct directory	*dp;
CONST struct db_i	*dbip;
CONST mat_t		mat;
{
	struct bu_external	ext;
	register int		id;
	struct db5_raw_internal	raw;

	BU_INIT_EXTERNAL(&ext);
	RT_INIT_DB_INTERNAL(ip);

	BU_ASSERT_LONG( dbip->dbi_version, ==, 5 );

	if( db_get_external( &ext, dp, dbip ) < 0 )
		return -2;		/* FAIL */

	if( db5_get_raw_internal_ptr( &raw, ext.ext_buf ) < 0 )  {
		bu_log("rt_db_get_internal5(%s):  import failure\n",
			dp->d_namep );
		bu_free_external( &ext );
		return -3;
	}

	if( raw.major_type == DB5HDR_MAJORTYPE_BRLCAD )  {
		id = raw.minor_type;
		/* As a convenience to older ft_import routines */
		if( mat == NULL )  mat = bn_mat_identity;
	} else {
		bu_log("rt_db_get_internal5(%s):  unable to import non-BRL-CAD object, major=%d\n",
			dp->d_namep, raw.major_type );
		bu_free_external( &ext );
		return -1;		/* FAIL */
	}

	/* If attributes are present in the object, make them available
	 * in the internal form.
	 */
	if( raw.attributes.ext_buf )  {
		if( db5_import_attributes( &ip->idb_avs, &raw.attributes ) < 0 )  {
			bu_log("rt_db_get_internal5(%s):  mal-formed attributes in database\n",
				dp->d_namep );
			return -8;
		}
	}

	if( !raw.body.ext_buf )  {
		bu_log("rt_db_get_internal5(%s):  object has no body\n",
			dp->d_namep );
		bu_free_external( &ext );
		return -4;
	}

	if( rt_functab[id].ft_import5( ip, &raw.body, mat, dbip ) < 0 )  {
		bu_log("rt_db_get_internal5(%s):  import failure\n",
			dp->d_namep );
		rt_db_free_internal( ip );
		bu_free_external( &ext );
		return -1;		/* FAIL */
	}
	bu_free_external( &ext );
	/* Don't free &raw.body */

	RT_CK_DB_INTERNAL( ip );
	ip->idb_type = id;
	ip->idb_meth = &rt_functab[id];
	return id;			/* OK */
}
