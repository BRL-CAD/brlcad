/*
 *			D B 5 . H
 *
 *  Definition of the BRL-CAD "v5" database format used for new ".g" files.
 *
 *  Authors -
 *	Michael John Muuss
 *	Lee A. Butler
 *	Paul J. Tanenbaum
 *	John R. Anderson
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Status -
 *	Public Domain, Distribution Unlimited.
 *
 *  $Header$
 */
#ifndef DB5_H
#define DB5_H seen

/*
 * The format of an object's header as it exists on disk,
 * as best we can describe its variable size with a "C" structure.
 */
struct db5_ondisk_header {
	unsigned char	db5h_magic1;		/* [0] */
	unsigned char	db5h_hflags;		/* [1] */
	unsigned char	db5h_aflags;		/* [2] */
	unsigned char	db5h_bflags;		/* [3] */
	unsigned char	db5h_major_type;	/* [4] */
	unsigned char	db5h_minor_type;	/* [5] */
	/* Next is a mandatory variable-size length field starting at [6] */
	/* Next are optional object name length & data fields */
};

#define DB5HDR_MAGIC1	0x76		/* 'v' */
#define DB5HDR_MAGIC2	0x35		/* '5' */

/* hflags */
#define DB5HDR_HFLAGS_DLI_MASK				0x03
#define DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT	0
#define DB5HDR_HFLAGS_DLI_HEADER_OBJECT			1
#define DB5HDR_HFLAGS_DLI_FREE_STORAGE			2
#define DB5HDR_HFLAGS_NAME_PRESENT			0x20
#define DB5HDR_HFLAGS_OBJECT_WIDTH_MASK			0xc0
#define DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT		6
#define DB5HDR_HFLAGS_NAME_WIDTH_MASK			0x18
#define DB5HDR_HFLAGS_NAME_WIDTH_SHIFT			3

#define DB5HDR_WIDTHCODE_8BIT		0
#define DB5HDR_WIDTHCODE_16BIT		1
#define DB5HDR_WIDTHCODE_32BIT		2
#define DB5HDR_WIDTHCODE_64BIT		3

/* aflags */
#define DB5HDR_AFLAGS_ZZZ_MASK				0x07
#define DB5HDR_AFLAGS_PRESENT				0x20
#define DB5HDR_AFLAGS_WIDTH_MASK			0xc0
#define DB5HDR_AFLAGS_WIDTH_SHIFT			6

/* bflags */
#define DB5HDR_BFLAGS_ZZZ_MASK				0x07
#define DB5HDR_BFLAGS_PRESENT				0x20
#define DB5HDR_BFLAGS_WIDTH_MASK			0xc0
#define DB5HDR_BFLAGS_WIDTH_SHIFT			6

/* Kinds of compression */
#define DB5HDR_ZZZ_UNCOMPRESSED				0
#define DB5HDR_ZZZ_GNU_GZIP				1
#define DB5HDR_ZZZ_BURROUGHS_WHEELER			2


/* major_type */
#define DB5HDR_MAJORTYPE_RESERVED			0
#define DB5HDR_MAJORTYPE_BRLCAD				1
#define DB5HDR_MAJORTYPE_ATTRIBUTE_ONLY			2
#define DB5HDR_MAJORTYPE_OPAQUE_BINARY			8

/*************************************************************************
 *
 *	What follows is the C programming API for the routines
 *	implementing the v5 database.
 *	It may want to live in a different header file.
 *
 ************************************************************************/

/*
 *  The "raw internal" form of one database object.
 *  This is what the low-level database routines will operate on.
 *  Magic number1 has already been checked, and is not stored.
 */
struct db5_raw_internal {
	long		magic;
	unsigned char	h_object_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	h_name_present;
	unsigned char	h_name_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	h_dli;
	unsigned char	a_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	a_present;
	unsigned char	a_zzz;
	unsigned char	b_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	b_present;
	unsigned char	b_zzz;
	unsigned char	major_type;
	unsigned char	minor_type;
	long		object_length;		/* in bytes, on disk */
	/* These three MUST NOT be passed to bu_free_external()! */
	struct bu_external name;
	struct bu_external body;
	struct bu_external attributes;
	unsigned char	*buf;		/* if non-null needs to be bu_free()ed */
};
#define DB5_RAW_INTERNAL_MAGIC	0x64357269	/* "d5ri" */

extern const int db5_enc_len[4];	/* convert wid to nbytes */

extern unsigned char *db5_encode_length(
	unsigned char	*cp,
	long		val,
	int		format);


#endif	/* DB5_H */
