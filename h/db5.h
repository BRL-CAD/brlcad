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

struct db5_header {
	unsigned char	db5h_magic1;
	unsigned char	db5h_hflags;
	unsigned char	db5h_iflags;
	unsigned char	db5h_major_type;
	unsigned char	db5h_minor_type;
	/* Next is a mandatory variable-size length field */
	/* Next are optional object name length & data fields */
};

#define DB5HDR_MAGIC1	0x76		/* 'v' */
#define DB5HDR_MAGIC2	0x35		/* '5' */

#define DB5HDR_HFLAGS_DLI_MASK				0x03
#define DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT	0
#define DB5HDR_HFLAGS_DLI_HEADER_OBJECT			1
#define DB5HDR_HFLAGS_DLI_FREE_STORAGE			2
#define DB5HDR_HFLAGS_NAME_PRESENT			0x20
#define DB5HDR_HFLAGS_OBJECT_WIDTH_MASK			0xc0
#define DB5HDR_HFLAGS_OBJECT_WIDTH_SHIFT		6

#define DB5HDR_WIDTHCODE_8BIT		0
#define DB5HDR_WIDTHCODE_16BIT		1
#define DB5HDR_WIDTHCODE_32BIT		2
#define DB5HDR_WIDTHCODE_64BIT		3

#define DB5HDR_IFLAGS_ZZZ_MASK				0x07
#define DB5HDR_IFLAGS_ZZZ_UNCOMPRESSED			0
#define DB5HDR_IFLAGS_ZZZ_GNU_GZIP			1
#define DB5HDR_IFLAGS_ZZZ_BURROUGHS_WHEELER		2
#define DB5HDR_IFLAGS_ATTRIBUTES_PRESENT		0x20
#define DB5HDR_IFLAGS_BODY_PRESENT			0x10
#define DB5HDR_IFLAGS_INTERIOR_WIDTH_MASK		0xc0
#define DB5HDR_IFLAGS_INTERIOR_WIDTH_SHIFT		6

#endif	/* DB5_H */
