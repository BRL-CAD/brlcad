/*                        D B 5 _ I O . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @addtogroup db5 */
/** @{ */
/** @file librt/db5_io.c
 *
 * Handle import/export and IO of v5 database objects.
 *
 */

#include "common.h"

#include <stdio.h>
#include <string.h>
#include "bin.h"

#include "bu.h"
#include "vmath.h"
#include "bn.h"
#include "db5.h"
#include "raytrace.h"
#include "mater.h"


/**
 * D B 5 _ H E A D E R _ I S _ V A L I D
 *
 * Verify that this is a valid header for a BRL-CAD v5 database.
 *
 * Returns -
 * 0 Not valid v5 header
 * 1 Valid v5 header
 */
int
db5_header_is_valid(const unsigned char *hp)
{
    const struct db5_ondisk_header *odp = (const struct db5_ondisk_header *)hp;

    if (odp->db5h_magic1 != DB5HDR_MAGIC1) return 0;
    if (hp[7] != DB5HDR_MAGIC2) return 0;

    /* hflags */
    if ((odp->db5h_hflags & DB5HDR_HFLAGS_DLI_MASK) != DB5HDR_HFLAGS_DLI_HEADER_OBJECT)
	return 0;
    if ((odp->db5h_hflags & DB5HDR_HFLAGS_NAME_PRESENT)) return 0;
    if (((odp->db5h_hflags & DB5HDR_HFLAGS_OBJECT_WIDTH_MASK) >> DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT)
	!= DB5HDR_WIDTHCODE_8BIT) return 0;

    /* aflags */
    if ((odp->db5h_aflags & DB5HDR_AFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED) return 0;
    if (odp->db5h_aflags & DB5HDR_AFLAGS_PRESENT) return 0;
    if (((odp->db5h_aflags & DB5HDR_AFLAGS_WIDTH_MASK) >> DB5HDR_AFLAGS_WIDTH_SHIFT)
	!= DB5HDR_WIDTHCODE_8BIT) return 0;

    /* bflags */
    if ((odp->db5h_bflags & DB5HDR_BFLAGS_ZZZ_MASK) != DB5_ZZZ_UNCOMPRESSED) return 0;
    if (odp->db5h_bflags & DB5HDR_BFLAGS_PRESENT) return 0;
    if (((odp->db5h_bflags & DB5HDR_BFLAGS_WIDTH_MASK) >> DB5HDR_BFLAGS_WIDTH_SHIFT)
	!= DB5HDR_WIDTHCODE_8BIT) return 0;

    /* major and minor type */
    if (odp->db5h_major_type != DB5_MAJORTYPE_RESERVED) return 0;
    if (odp->db5h_minor_type != 0) return 0;

    /* Check length, known to be 8-bit.  Header len=1 8-byte chunk. */
    if (hp[6] != 1) return 0;

    return 1;		/* valid */
}


/**
 * D B 5 _ S E L E C T _ L E N G T H _ E N C O D I N G
 *
 * Given a number to encode, decide which is the smallest encoding
 * format which will contain it.
 */
int
db5_select_length_encoding(size_t len)
{
    if (len <= 255) return DB5HDR_WIDTHCODE_8BIT;
    if (len <= 65535) return DB5HDR_WIDTHCODE_16BIT;
    if (len < 0x7ffffffe) return DB5HDR_WIDTHCODE_32BIT;
    return DB5HDR_WIDTHCODE_64BIT;
}


/**
 * D B 5 _ D E C O D E _ L E N G T H
 *
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes unsigned values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
int
db5_decode_length(size_t *lenp, const unsigned char *cp, int format)
{
    switch (format) {
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
	    if (sizeof(size_t) >= 8) {
		*lenp = BU_GLONGLONG(cp);
		return 8;
	    }
	    bu_bomb("db5_decode_length(): encountered 64-bit length on non-64-bit machine\n");
    }
    bu_bomb("db5_decode_length(): unknown width code\n");
    return 0;
}


/**
 * D B 5 _ D E C O D E _ S I G N E D
 *
 * Given a variable-width length field in network order (XDR), store
 * it in *lenp.
 *
 * This routine processes signed values.
 *
 * Returns -
 * The number of bytes of input that were decoded.
 */
int
db5_decode_signed(size_t *lenp, const unsigned char *cp, int format)
{
    switch (format) {
	case DB5HDR_WIDTHCODE_8BIT:
	    if ((*lenp = (*cp)) & 0x80) *lenp |= (-1L ^ 0xFF);
	    return 1;
	case DB5HDR_WIDTHCODE_16BIT:
	    if ((*lenp = BU_GSHORT(cp)) & 0x8000)  *lenp |= (-1L ^ 0xFFFF);
	    return 2;
	case DB5HDR_WIDTHCODE_32BIT:
	    if ((*lenp = BU_GLONG(cp)) & 0x80000000)
		*lenp |= (-1L ^ 0xFFFFFFFF);
	    return 4;
	case DB5HDR_WIDTHCODE_64BIT:
	    if (sizeof(size_t) >= 8) {
		*lenp = BU_GLONGLONG(cp);
		return 8;
	    }
	    bu_bomb("db5_decode_length(): encountered 64-bit length on 32-bit machine\n");
    }
    bu_bomb("db5_decode_length(): unknown width code\n");
    return 0;
}


/**
 * D B 5 _ E N C O D E _ L E N G T H
 *
 * Given a value and a variable-width format spec, store it in network
 * order.
 *
 * Returns -
 * pointer to next available byte.
 */
unsigned char *
db5_encode_length(
    unsigned char *cp,
    size_t val,
    int format)
{
    switch (format) {
	case DB5HDR_WIDTHCODE_8BIT:
	    *cp = (unsigned char)val & 0xFF;
	    return cp + sizeof(unsigned char);
	case DB5HDR_WIDTHCODE_16BIT:
	    *(uint16_t *)&cp[0] = htons((uint16_t)val);
	    return cp + sizeof(uint16_t);
	case DB5HDR_WIDTHCODE_32BIT:
	    *(uint32_t *)&cp[0] = htonl((uint32_t)val);
	    return cp + sizeof(uint32_t);
	case DB5HDR_WIDTHCODE_64BIT:
	    *(uint64_t *)&cp[0] = htonll((uint64_t)val);
	    return cp + sizeof(uint64_t);
    }
    bu_bomb("db5_encode_length(): unknown width code\n");
    return NULL;
}


/**
 * D B 5 _ C R A C K _ D I S K _ H E A D E R
 *
 * Returns -
 * 0 on success
 * -1 on error
 */
int
db5_crack_disk_header(struct db5_raw_internal *rip, const unsigned char *cp)
{
    if (cp[0] != DB5HDR_MAGIC1) {
	bu_log("db5_crack_disk_header() bad magic1 -- database has become corrupted\n expected x%x, got x%x\n",
	       DB5HDR_MAGIC1, cp[0]);
	if (cp[0] == 'I') {
	    bu_log ("Concatenation of different database versions detected.\n");
	    bu_log ("Run 'dbupgrade' on all databases before concatenation (cat command).\n");
	}
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

    if (RT_G_DEBUG&DEBUG_DB) bu_log("db5_crack_disk_header()\n\
	h_dli=%d, h_object_width=%d, h_name_present=%d, h_name_width=%d, \n\
	a_width=%d, a_present=%d, a_zzz=%d, \n\
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
				    rip->minor_type);

    return 0;
}


/**
 * D B 5 _ G E T _ R A W _ I N T E R N A L _ P T R
 *
 * Returns -
 * on success, pointer to first unused byte
 * NULL, on error
 */
const unsigned char *
db5_get_raw_internal_ptr(struct db5_raw_internal *rip, const unsigned char *ip)
{
    const unsigned char *cp = ip;

    if (db5_crack_disk_header(rip, cp) < 0) return NULL;
    cp += sizeof(struct db5_ondisk_header);

    cp += db5_decode_length(&rip->object_length, cp, rip->h_object_width);
    rip->object_length <<= 3;	/* cvt 8-byte chunks to byte count */

    if ((size_t)rip->object_length < sizeof(struct db5_ondisk_header)) {
	bu_log("db5_get_raw_internal_ptr(): object_length=%ld is too short, database is corrupted\n",
	       rip->object_length);
	return NULL;
    }

    /* Verify trailing magic number */
    if (ip[rip->object_length-1] != DB5HDR_MAGIC2) {
	bu_log("db5_get_raw_internal_ptr() bad magic2 -- database has become corrupted.\n expected x%x, got x%x\n",
	       DB5HDR_MAGIC2, ip[rip->object_length-1]);
	return NULL;
    }

    BU_EXTERNAL_INIT(&rip->name);
    BU_EXTERNAL_INIT(&rip->body);
    BU_EXTERNAL_INIT(&rip->attributes);

    /* Grab name, if present */
    if (rip->h_name_present) {
	cp += db5_decode_length(&rip->name.ext_nbytes,
				cp, rip->h_name_width);
	rip->name.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->name.ext_nbytes;
    }

    /* Point to attributes, if present */
    if (rip->a_present) {
	cp += db5_decode_length(&rip->attributes.ext_nbytes,
				cp, rip->a_width);
	rip->attributes.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->attributes.ext_nbytes;
    }

    /* Point to body, if present */
    if (rip->b_present) {
	cp += db5_decode_length(&rip->body.ext_nbytes,
				cp, rip->b_width);
	rip->body.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->body.ext_nbytes;
    }

    rip->buf = NULL;	/* no buffer needs freeing */

    return ip + rip->object_length;
}


/**
 * D B 5 _ G E T _ R A W _ I N T E R N A L _ F P
 *
 * Returns -
 * 0 on success
 * -1 on EOF
 * -2 on error
 */
int
db5_get_raw_internal_fp(struct db5_raw_internal *rip, FILE *fp)
{
    struct db5_ondisk_header header;
    unsigned char lenbuf[8];
    int count = 0;
    int used;
    size_t want, got;
    unsigned char *cp;

    if (fread((unsigned char *)&header, sizeof header, 1, fp) != 1) {
	if (feof(fp)) return -1;
	bu_log("db5_get_raw_internal_fp(): fread header error\n");
	return -2;
    }
    if (db5_crack_disk_header(rip, (unsigned char *)&header) < 0)
	return -2;
    used = sizeof(header);

    switch (rip->h_object_width) {
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
    if (fread(lenbuf, count, 1, fp)  != 1) {
	bu_log("db5_get_raw_internal_fp(): fread lenbuf error\n");
	return -2;
    }
    used += db5_decode_length(&rip->object_length, lenbuf, rip->h_object_width);
    rip->object_length <<= 3;	/* cvt 8-byte chunks to byte count */

    if ((size_t)rip->object_length < sizeof(struct db5_ondisk_header)) {
	bu_log("db5_get_raw_internal_fp(): object_length=%ld is too short, database is corrupted\n",
	       rip->object_length);
	return -1;
    }

    /* Now that we finally know how large the object is, get it all */
    rip->buf = (unsigned char *)bu_malloc(rip->object_length, "raw v5 object");

    *((struct db5_ondisk_header *)rip->buf) = header;	/* struct copy */
    memcpy(rip->buf+sizeof(header), lenbuf, count);

    cp = rip->buf+used;
    want = rip->object_length-used;
    BU_ASSERT_LONG(want, >, 0);
    if ((got = fread(cp, 1, want, fp)) != want) {
	bu_log("db5_get_raw_internal_fp(), want=%ld, got=%ld, database is too short\n",
	       want, got);
	return -2;
    }

    /* Verify trailing magic number */
    if (rip->buf[rip->object_length-1] != DB5HDR_MAGIC2) {
	bu_log("db5_get_raw_internal_fp() bad magic2 -- database has become corrupted.\n expected x%x, got x%x\n",
	       DB5HDR_MAGIC2, rip->buf[rip->object_length-1]);
	return -2;
    }

    BU_EXTERNAL_INIT(&rip->name);
    BU_EXTERNAL_INIT(&rip->body);
    BU_EXTERNAL_INIT(&rip->attributes);

    /* Grab name, if present */
    if (rip->h_name_present) {
	cp += db5_decode_length(&rip->name.ext_nbytes,
				cp, rip->h_name_width);
	rip->name.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->name.ext_nbytes;
    }

    /* Point to attributes, if present */
    if (rip->a_present) {
	cp += db5_decode_length(&rip->attributes.ext_nbytes,
				cp, rip->a_width);
	rip->attributes.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->attributes.ext_nbytes;
    }

    /* Point to body, if present */
    if (rip->b_present) {
	cp += db5_decode_length(&rip->body.ext_nbytes,
				cp, rip->b_width);
	rip->body.ext_buf = (genptr_t)cp;	/* discard const */
	cp += rip->body.ext_nbytes;
    }

    return 0;		/* success */
}


/**
 * D B 5 _ E X P O R T _ O B J E C T 3
 *
 * A routine for merging together the three optional parts of an
 * object into the final on-disk format.  Results in extra data
 * copies, but serves as a starting point for testing.  Any of name,
 * attrib, and body may be null.
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
    int b_zzz)
{
    struct db5_ondisk_header *odp;
    register unsigned char *cp;
    long namelen = 0;
    size_t need;
    int h_width, n_width, a_width, b_width;
    long togo;

    /*
     * First, compute an upper bound on the size buffer needed.
     * Over-estimate on the length fields just to keep it simple.
     */
    need = sizeof(struct db5_ondisk_header);
    need += 8;	/* for object_length */
    if (name) {
	namelen = (long)strlen(name) + 1;	/* includes null */
	if (namelen > 1) {
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
    if (attrib) {
	BU_CK_EXTERNAL(attrib);
	if (attrib->ext_nbytes > 0) {
	    a_width = db5_select_length_encoding(attrib->ext_nbytes);
	    need += attrib->ext_nbytes + db5_enc_len[a_width];
	} else {
	    attrib = NULL;
	    a_width = 0;
	}
    } else {
	a_width = 0;
    }
    if (body) {
	BU_CK_EXTERNAL(body);
	if (body->ext_nbytes > 0) {
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
    BU_EXTERNAL_INIT(out);
    out->ext_buf = bu_malloc(need, "external object3");
    out->ext_nbytes = need;		/* will be trimmed, below */

    /* Determine encoding for the header length field */
    h_width = db5_select_length_encoding((need+7)>>3);

    /* prepare combined external object */
    odp = (struct db5_ondisk_header *)out->ext_buf;
    odp->db5h_magic1 = DB5HDR_MAGIC1;

    /* hflags */
    odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
	(dli & DB5HDR_HFLAGS_DLI_MASK);
    if (name) {
	odp->db5h_hflags |= DB5HDR_HFLAGS_NAME_PRESENT |
	    (n_width << DB5HDR_HFLAGS_NAME_WIDTH_SHIFT);

    }
    if (hidden) {
	odp->db5h_hflags |= DB5HDR_HFLAGS_HIDDEN_OBJECT;
    }

    /* aflags */
    odp->db5h_aflags = a_width << DB5HDR_AFLAGS_WIDTH_SHIFT;
    if (attrib) odp->db5h_aflags |= DB5HDR_AFLAGS_PRESENT;
    odp->db5h_aflags |= a_zzz & DB5HDR_AFLAGS_ZZZ_MASK;

    /* bflags */
    odp->db5h_bflags = b_width << DB5HDR_BFLAGS_WIDTH_SHIFT;
    if (body) odp->db5h_bflags |= DB5HDR_BFLAGS_PRESENT;
    odp->db5h_bflags |= b_zzz & DB5HDR_BFLAGS_ZZZ_MASK;

    if (a_zzz || b_zzz) bu_bomb("db5_export_object3: compression not supported yet\n");

    /* Object_Type */
    odp->db5h_major_type = major;
    odp->db5h_minor_type = minor;

    /* Build up the rest of the record */
    cp = ((unsigned char *)out->ext_buf) + sizeof(struct db5_ondisk_header);
    cp = db5_encode_length(cp, 7L, h_width);	/* will be replaced below */

    if (name) {
	cp = db5_encode_length(cp, namelen, n_width);
	memcpy(cp, name, namelen);	/* includes null */
	cp += namelen;
    }

    if (attrib) {
	/* minimum buffer length is a one byte attribute name, followed by a NULL name termination,
	 * followed by no bytes (for an empty value), followed by a NULL value termination,
	 * followed by a NULL attribute-value termination. Minimum is 4 bytes
	 */
	BU_ASSERT_PTR(attrib->ext_nbytes, >=, 4);
	cp = db5_encode_length(cp, attrib->ext_nbytes, a_width);
	memcpy(cp, attrib->ext_buf, attrib->ext_nbytes);
	cp += attrib->ext_nbytes;
    }

    if (body) {
	cp = db5_encode_length(cp, body->ext_nbytes, b_width);
	memcpy(cp, body->ext_buf, body->ext_nbytes);
	cp += body->ext_nbytes;
    }

    togo = cp - ((unsigned char *)out->ext_buf) + 1;
    togo &= 7;
    if (togo != 0) {
	togo = 8 - togo;
	while (togo-- > 0)  *cp++ = '\0';
    }
    *cp++ = DB5HDR_MAGIC2;

    /* Verify multiple of 8 */
    togo = cp - ((unsigned char *)out->ext_buf);
    BU_ASSERT_LONG(togo&7, ==, 0);

    /* Finally, go back to the header and write the actual object length */
    cp = ((unsigned char *)out->ext_buf) + sizeof(struct db5_ondisk_header);
    cp = db5_encode_length(cp, togo>>3, h_width);

    out->ext_nbytes = togo;
    BU_ASSERT_LONG(out->ext_nbytes, >=, 8);
}


/**
 * D B 5 _ M A K E _ F R E E _ O B J E C T _ H D R
 *
 * Make only the front (header) portion of a free object.  This is
 * used when operating on very large contiguous free objects in the
 * database (e.g. 50 MBytes).
 */
void
db5_make_free_object_hdr(struct bu_external *ep, size_t length)
{
    struct db5_ondisk_header *odp;
    int h_width;
    unsigned char *cp;

    BU_CK_EXTERNAL(ep);

    BU_ASSERT_SIZE_T(length, >=, 8);
    BU_ASSERT_SIZE_T(length&7, ==, 0);

    /* Reserve enough space to hold any free header, even w/64-bit len */
    ep->ext_nbytes = 8+8;
    ep->ext_buf = bu_calloc(1, ep->ext_nbytes, "db5_make_free_object_hdr");

    /* Determine encoding for the header length field */
    h_width = db5_select_length_encoding(length>>3);

    /* prepare header of external object */
    odp = (struct db5_ondisk_header *)ep->ext_buf;
    odp->db5h_magic1 = DB5HDR_MAGIC1;
    odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
	DB5HDR_HFLAGS_DLI_FREE_STORAGE;

    cp = ((unsigned char *)ep->ext_buf) + sizeof(struct db5_ondisk_header);
    cp = db5_encode_length(cp, length>>3, h_width);
}


/**
 * D B 5 _ M A K E _ F R E E _ O B J E C T
 *
 * Make a complete, zero-filled, free object.  Note that free objects
 * can sometimes get quite large.
 */
void
db5_make_free_object(struct bu_external *ep, size_t length)
{
    struct db5_ondisk_header *odp;
    int h_width;
    unsigned char *cp;

    BU_CK_EXTERNAL(ep);

    BU_ASSERT_SIZE_T(length, >=, 8);
    BU_ASSERT_SIZE_T(length&7, ==, 0);

    ep->ext_buf = bu_calloc(1, length, "db5_make_free_object");
    ep->ext_nbytes = length;

    /* Determine encoding for the header length field */
    h_width = db5_select_length_encoding(length>>3);

    /* prepare combined external object */
    odp = (struct db5_ondisk_header *)ep->ext_buf;
    odp->db5h_magic1 = DB5HDR_MAGIC1;
    odp->db5h_hflags = (h_width << DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT) |
	DB5HDR_HFLAGS_DLI_FREE_STORAGE;

    cp = ((unsigned char *)ep->ext_buf) + sizeof(struct db5_ondisk_header);
    cp = db5_encode_length(cp, length>>3, h_width);

    cp = ((unsigned char *)ep->ext_buf) + length-1;
    *cp = DB5HDR_MAGIC2;
}


/**
 * R T _ D B _ C V T _ T O _ E X T E R N A L 5
 *
 * The attributes are taken from ip->idb_avs
 *
 * If present, convert attributes to on-disk format.  This must happen
 * after exporting the body, in case the ft_export5() method happened
 * to extend the attribute set.  Combinations are one "solid" which
 * does this.
 *
 * The internal representation is NOT freed, that's the caller's job.
 *
 * The 'ext' pointer is accepted in uninitialized form, and an
 * initialized structure is always returned, so that the caller may
 * free it even when an error return is given.
 *
 * Returns -
 * 0 OK
 * -1 FAIL
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
    struct bu_external attributes;
    struct bu_external body;
    int minor;
    int ret;

    /* check inputs */
    if (!name) {
	bu_log("rt_db_cvt_to_external5 expecting non-NULL name parameter\n");
	return -1;
    }
    RT_CK_DB_INTERNAL(ip);
    if (dbip) RT_CK_DBI(dbip);	/* may be null */
    RT_CK_RESOURCE(resp);
    
    /* prepare output */
    BU_EXTERNAL_INIT(ext);
    BU_EXTERNAL_INIT(&body);
    BU_EXTERNAL_INIT(&attributes);

    minor = ip->idb_type;	/* XXX not necessarily v5 numbers. */

    /* Scale change on export is 1.0 -- no change */
    ret = -1;
    if (ip->idb_meth && ip->idb_meth->ft_export5) {
	ret = ip->idb_meth->ft_export5(&body, ip, conv2mm, dbip, resp);
    }
    if (ret < 0) {
	bu_log("rt_db_cvt_to_external5(%s):  ft_export5 failure\n",
	       name);
	bu_free_external(&body);
	return -1;		/* FAIL */
    }
    BU_CK_EXTERNAL(&body);

    /* If present, convert attributes to on-disk format. */
    if (ip->idb_avs.magic == BU_AVS_MAGIC) {
	db5_export_attributes(&attributes, &ip->idb_avs);
	BU_CK_EXTERNAL(&attributes);
    }

    /* serialize the object with attributes */
    db5_export_object3(ext, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
		       name, 0, &attributes, &body,
		       major, minor,
		       DB5_ZZZ_UNCOMPRESSED, DB5_ZZZ_UNCOMPRESSED);
    BU_CK_EXTERNAL(ext);

    /* cleanup */
    bu_free_external(&body);
    bu_free_external(&attributes);

    return 0;		/* OK */
}


/*
 * D B _ W R A P _ V 5 _ E X T E R N A L
 *
 * Modify name of external object, if necessary.
 */
int
db_wrap_v5_external(struct bu_external *ep, const char *name)
{
    struct db5_raw_internal raw;
    struct bu_external tmp;

    BU_CK_EXTERNAL(ep);

    /* Crack the external form into parts */
    if (db5_get_raw_internal_ptr(&raw, (unsigned char *)ep->ext_buf) == NULL) {
	bu_log("db_put_external5(%s) failure in db5_get_raw_internal_ptr()\n",
	       name);
	return -1;
    }
    BU_ASSERT_LONG(raw.h_dli, ==, DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT);

    /* See if name needs to be changed */
    if (raw.name.ext_buf == NULL || !BU_STR_EQUAL(name, (const char *)raw.name.ext_buf)) {
	/* Name needs to be changed.  Create new external form.
	 * Make temporary copy so input isn't smashed
	 * as new external object is constructed.
	 */
	tmp = *ep;		/* struct copy */
	BU_EXTERNAL_INIT(ep);

	db5_export_object3(ep,
			   DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT,
			   name,
			   raw.h_name_hidden,
			   &raw.attributes,
			   &raw.body,
			   raw.major_type, raw.minor_type,
			   raw.a_zzz, raw.b_zzz);
	/* 'raw' is invalid now, 'ep' has new external form. */
	bu_free_external(&tmp);
	return 0;
    }

    /* No changes needed, input object is properly named */
    return 0;
}


/**
 *
 * D B _ P U T _ E X T E R N A L 5
 *
 * Given that caller already has an external representation of the
 * database object, update it to have a new name (taken from
 * dp->d_namep) in that external representation, and write the new
 * object into the database, obtaining different storage if the size
 * has changed.
 *
 * Changing the name on a v5 object is a relatively expensive
 * operation.
 *
 * Caller is responsible for freeing memory of external
 * representation, using bu_free_external().
 *
 * This routine is used to efficiently support MGED's "cp" and "keep"
 * commands, which don't need to import and decompress objects just to
 * rename and copy them.
 *
 * Returns -
 * -1 error
 * 0 success
 */
int
db_put_external5(struct bu_external *ep, struct directory *dp, struct db_i *dbip)
{
    RT_CK_DBI(dbip);
    RT_CK_DIR(dp);
    BU_CK_EXTERNAL(ep);

    if (RT_G_DEBUG&DEBUG_DB) bu_log("db_put_external5(%s) ep=x%x, dbip=x%x, dp=x%x\n",
				    dp->d_namep, ep, dbip, dp);

    if (dbip->dbi_read_only) {
	bu_log("db_put_external5(%s):  READ-ONLY file\n",
	       dbip->dbi_filename);
	return -1;
    }

    BU_ASSERT_LONG(dbip->dbi_version, ==, 5);

    /* First, change the name. */
    if (db_wrap_v5_external(ep, dp->d_namep) < 0) {
	bu_log("db_put_external5(%s) failure in db_wrap_v5_external()\n",
	       dp->d_namep);
	return -1;
    }

    /* Second, obtain storage for final object */
    if (ep->ext_nbytes != dp->d_len || dp->d_addr == RT_DIR_PHONY_ADDR) {
	if (db5_realloc(dbip, dp, ep) < 0) {
	    bu_log("db_put_external(%s) db_realloc5() failed\n", dp->d_namep);
	    return -5;
	}
    }
    BU_ASSERT_LONG(ep->ext_nbytes, ==, dp->d_len);

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy(dp->d_un.ptr, (char *)ep->ext_buf, ep->ext_nbytes);
	return 0;
    }

    if (db_write(dbip, (char *)ep->ext_buf, ep->ext_nbytes, dp->d_addr) < 0) {
	return -1;
    }
    return 0;
}


/**
 * R T _ D B _ P U T _ I N T E R N A L 5
 *
 * Convert the internal representation of a solid to the external one,
 * and write it into the database.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the version-generic interface "rt_db_put_internal()".
 *
 * The internal representation is always freed.  (Not the pointer,
 * just the contents).
 *
 * Returns -
 * <0 error
 * 0 success
 */
int
rt_db_put_internal5(
    struct directory *dp,
    struct db_i *dbip,
    struct rt_db_internal *ip,
    struct resource *resp,
    const int major)
{
    struct bu_external ext;

    RT_CK_DIR(dp);
    RT_CK_DBI(dbip);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_RESOURCE(resp);
    BU_ASSERT_LONG(dbip->dbi_version, ==, 5);

    BU_EXTERNAL_INIT(&ext);
    if (rt_db_cvt_to_external5(&ext, dp->d_namep, ip, 1.0, dbip, resp, major) < 0) {
	bu_log("rt_db_put_internal5(%s):  export failure\n",
	       dp->d_namep);
	goto fail;
    }
    BU_CK_EXTERNAL(&ext);

    if (ext.ext_nbytes != dp->d_len || dp->d_addr == RT_DIR_PHONY_ADDR) {
	if (db5_realloc(dbip, dp, &ext) < 0) {
	    bu_log("rt_db_put_internal5(%s) db_realloc5() failed\n", dp->d_namep);
	    goto fail;
	}
    }
    BU_ASSERT_LONG(ext.ext_nbytes, ==, dp->d_len);

    if (dp->d_flags & RT_DIR_INMEM) {
	memcpy(dp->d_un.ptr, ext.ext_buf, ext.ext_nbytes);
	goto ok;
    }

    if (db_write(dbip, (char *)ext.ext_buf, ext.ext_nbytes, dp->d_addr) < 0) {
	goto fail;
    }
ok:
    bu_free_external(&ext);
    rt_db_free_internal(ip);
    return 0;			/* OK */

fail:
    bu_free_external(&ext);
    rt_db_free_internal(ip);
    return -2;		/* FAIL */
}


/* FIXME: should have gone away with v6.  needed now to pass the minor_type down during read */
extern int rt_binunif_import5_minor_type(struct rt_db_internal *, const struct bu_external *, const mat_t, const struct db_i *, struct resource *, int);

/**
 * R T _ D B _ E X T E R N A L 5 _ T O _ I N T E R N A L 5
 *
 * Given an object in external form, convert it to internal form.  The
 * caller is responsible for freeing the external form.
 *
 * Returns -
 * <0 On error
 * id On success.
 */
int
rt_db_external5_to_internal5(
    struct rt_db_internal *ip,
    const struct bu_external *ep,
    const char *name,
    const struct db_i *dbip,
    const mat_t mat,
    struct resource *resp)
{
    register int id;
    struct db5_raw_internal raw;
    int ret;

    BU_CK_EXTERNAL(ep);
    RT_CK_DB_INTERNAL(ip);
    RT_CK_DBI(dbip);

    /* needed for call into functab */
    if (resp == RESOURCE_NULL)
	resp = &rt_uniresource;

    BU_ASSERT_LONG(dbip->dbi_version, ==, 5);

    if (db5_get_raw_internal_ptr(&raw, ep->ext_buf) == NULL) {
	bu_log("rt_db_external5_to_internal5(%s):  import failure\n",
	       name);
	return -3;
    }

    if ((raw.major_type == DB5_MAJORTYPE_BRLCAD)
	||(raw.major_type == DB5_MAJORTYPE_BINARY_UNIF)) {
	/* As a convenience to older ft_import routines */
	if (mat == NULL) mat = bn_mat_identity;
    } else {
	bu_log("rt_db_external5_to_internal5(%s):  unable to import non-BRL-CAD object, major=%d minor=%d\n",
	       name, raw.major_type, raw.minor_type);
	return -1;		/* FAIL */
    }

    if (ip->idb_avs.magic != BU_AVS_MAGIC) {
	bu_avs_init_empty(&ip->idb_avs);
    }

    /* If attributes are present in the object, make them available
     * in the internal form.
     */
    if (raw.attributes.ext_buf) {
	if (db5_import_attributes(&ip->idb_avs, &raw.attributes) < 0) {
	    bu_log("rt_db_external5_to_internal5(%s):  mal-formed attributes in database\n",
		   name);
	    return -8;
	}

	(void)db5_standardize_avs(&ip->idb_avs);
    }

    if (!raw.body.ext_buf) {
	bu_log("rt_db_external5_to_internal5(%s):  object has no body\n",
	       name);
	return -4;
    }

    /* FIXME: This is a temporary kludge accommodating dumb binunifs
     * that don't export their minor type or have table entries for
     * all their types. (this gets pushed up when a functab wrapper is
     * created)
     */
    switch (raw.major_type) {
	case DB5_MAJORTYPE_BRLCAD:
	    id = raw.minor_type; break;
	case DB5_MAJORTYPE_BINARY_UNIF:
	    id = ID_BINUNIF; break;
	default:
	    bu_log("rt_db_external5_to_internal5(%s): don't yet handle major_type %d\n", name, raw.major_type);
	    return -1;
    }

    /* ip has already been initialized, and should not be re-initted */
    ret = -1;
    if (id == ID_BINUNIF) {
	/* FIXME: binunif export needs to write out minor_type so
	 * this isn't needed, but breaks compatibility.  slate for
	 * v6.
	 */
	ret = rt_binunif_import5_minor_type(ip, &raw.body, mat, dbip, resp, raw.minor_type);
    } else if (rt_functab[id].ft_import5) {
	ret = rt_functab[id].ft_import5(ip, &raw.body, mat, dbip, resp);
    }
    if (ret < 0) {
	bu_log("rt_db_external5_to_internal5(%s):  import failure\n",
	       name);
	rt_db_free_internal(ip);
	return -1;		/* FAIL */
    }
    /* Don't free &raw.body */

    RT_CK_DB_INTERNAL(ip);
    ip->idb_major_type = raw.major_type;
    ip->idb_minor_type = raw.minor_type;
    ip->idb_meth = &rt_functab[id];

    return id;			/* OK */
}


/**
 * R T _ D B _ G E T _ I N T E R N A L 5
 *
 * Get an object from the database, and convert it into its internal
 * representation.
 *
 * Applications and middleware shouldn't call this directly, they
 * should use the generic interface "rt_db_get_internal()".
 *
 * Returns -
 * <0 On error
 * id On success.
 */
int
rt_db_get_internal5(
    struct rt_db_internal *ip,
    const struct directory *dp,
    const struct db_i *dbip,
    const mat_t mat,
    struct resource *resp)
{
    struct bu_external ext;
    int ret;

    BU_EXTERNAL_INIT(&ext);
    RT_DB_INTERNAL_INIT(ip);

    BU_ASSERT_LONG(dbip->dbi_version, ==, 5);

    if (db_get_external(&ext, dp, dbip) < 0)
	return -2;		/* FAIL */

    ret = rt_db_external5_to_internal5(ip, &ext, dp->d_namep, dbip, mat, resp);
    bu_free_external(&ext);
    return ret;
}


/**
 *
 */
void
db5_export_color_table(struct bu_vls *ostr, struct db_i *dbip)
{
    BU_CK_VLS(ostr);
    RT_CK_DBI(dbip);
    rt_vls_color_map(ostr);
}


/**
 * D B 5 _ I M P O R T _ C O L O R _ T A B L E
 */
void
db5_import_color_table(char *cp)
{
    char *sp = cp;
    int low, high, r, g, b;

    while ((sp = strchr(sp, '{')) != NULL) {
	sp++;
	if (sscanf(sp, "%d %d %d %d %d", &low, &high, &r, &g, &b) != 5) break;
	rt_color_addrec(low, high, r, g, b, MATER_NO_ADDR);
    }
}


/**
 * D B 5 _ P U T _ C O L O R _ T A B L E
 *
 * Put the old region-id-color-table into the global object.  A null
 * attribute is set if the material table is empty.
 *
 * Returns -
 * <0 error
 * 0 OK
 */
int
db5_put_color_table(struct db_i *dbip)
{
    struct bu_vls str;
    int ret;

    RT_CK_DBI(dbip);
    BU_ASSERT_LONG(dbip->dbi_version, ==, 5);

    bu_vls_init(&str);
    db5_export_color_table(&str, dbip);

    ret = db5_update_attribute(DB5_GLOBAL_OBJECT_NAME,
			       "regionid_colortable", bu_vls_addr(&str), dbip);

    bu_vls_free(&str);
    return ret;
}


/** D B _ G E T _ A T T R I B U T E S
 *
 * Get attributes for an object pointed to by *dp
 *
 * returns:
 * 0 - all is well
 * <0 - error
 */
int
db5_get_attributes(const struct db_i *dbip, struct bu_attribute_value_set *avs, const struct directory *dp)
{
    struct bu_external ext;
    struct db5_raw_internal raw;

    RT_CK_DBI(dbip);

    if (dbip->dbi_version < 5)
	return 0;	/* not an error, just no attributes */

    RT_CK_DIR(dp);

    BU_EXTERNAL_INIT(&ext);

    if (db_get_external(&ext, dp, dbip) < 0)
	return -1;		/* FAIL */

    if (db5_get_raw_internal_ptr(&raw, ext.ext_buf) == NULL) {
	bu_free_external(&ext);
	return -2;
    }

    if (raw.attributes.ext_buf) {
	if (db5_import_attributes(avs, &raw.attributes) < 0) {
	    bu_free_external(&ext);
	    return -3;
	}
    }

    bu_free_external(&ext);
    return 0;
}


/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
