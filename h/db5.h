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
 *  The "raw internal" form of one database object.
 *  This is what the low-level database routines will operate on.
 *  Magic number1 has already been checked, and is not stored.
 */
struct db5_raw_internal {
	long		magic;
	unsigned char	h_object_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	h_name_present;
	unsigned char	i_object_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	i_attributes_present;
	unsigned char	i_body_present;
	unsigned char	i_zzz;
	unsigned char	major_type;
	unsigned char	minor_type;
	long		object_length;		/* in bytes, on disk */
	long		name_length;
	unsigned char	*name;
	long		interior_length;
	unsigned char	*interior;
	unsigned char	*buf;		/* if non-null needs to be bu_free()ed */
};
#define DB5_RAW_INTERNAL_MAGIC	0x64357269	/* "d5ri" */

/*
 * The format of an object's header as it exists on disk,
 * as best we can describe its variable size with a "C" structure.
 */
struct db5_ondisk_header {
	unsigned char	db5h_magic1;		/* [0] */
	unsigned char	db5h_hflags;		/* [1] */
	unsigned char	db5h_iflags;		/* [2] */
	unsigned char	db5h_major_type;	/* [3] */
	unsigned char	db5h_minor_type;	/* [4] */
	/* Next is a mandatory variable-size length field starting at [5] */
	/* Next are optional object name length & data fields */
};

#define DB5HDR_MAGIC1	0x76		/* 'v' */
#define DB5HDR_MAGIC2	0x35		/* '5' */

/* hflags */
#if 0
#define DB5HDR_HFLAGS_DLI_MASK				0x03
#define DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT	0
#define DB5HDR_HFLAGS_DLI_HEADER_OBJECT			1
#define DB5HDR_HFLAGS_DLI_FREE_STORAGE			2
#endif
#define DB5HDR_HFLAGS_NAME_PRESENT			0x20
#define DB5HDR_HFLAGS_OBJECT_WIDTH_MASK			0xc0
#define DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT		6

#define DB5HDR_WIDTHCODE_8BIT		0
#define DB5HDR_WIDTHCODE_16BIT		1
#define DB5HDR_WIDTHCODE_32BIT		2
#define DB5HDR_WIDTHCODE_64BIT		3

/* iflags */
#define DB5HDR_IFLAGS_ZZZ_MASK				0x07
#define DB5HDR_IFLAGS_ZZZ_UNCOMPRESSED			0
#define DB5HDR_IFLAGS_ZZZ_GNU_GZIP			1
#define DB5HDR_IFLAGS_ZZZ_BURROUGHS_WHEELER		2
#define DB5HDR_IFLAGS_ATTRIBUTES_PRESENT		0x20
#define DB5HDR_IFLAGS_BODY_PRESENT			0x10
#define DB5HDR_IFLAGS_INTERIOR_WIDTH_MASK		0xc0
#define DB5HDR_IFLAGS_INTERIOR_WIDTH_SHIFT		6

/* major_type */
#define DB5HDR_MAJORTYPE_BRLCAD_NONGEOM			1
#define DB5HDR_MAJORTYPE_BRLCAD_GEOMETRY		2
#define DB5HDR_MAJORTYPE_OPAQUE_BINARY			3
#define DB5HDR_MAJORTYPE_ATTRIBUTE_ONLY			4
#define DB5HDR_MAJORTYPE_DLI				255

/* minor_type */
#define DB5HDR_MINORTYPE_DLI_FREE			1
#define DB5HDR_MINORTYPE_DLI_HEADER			2

#endif	/* DB5_H */
