/*                           D B 5 . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
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
/** @file db5.h
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
#define	DB5HDR_HFLAGS_HIDDEN_OBJECT			0x4
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

/*************************************************************************
 *
 *	What follows is the C programming API for the routines
 *	implementing the v5 database.
 *	It may want to live in a different header file.
 *
 ************************************************************************/

/* Name of global attribute-only object for storing database title & units */
#define DB5_GLOBAL_OBJECT_NAME			"_GLOBAL"

/* Kinds of compression */
#define DB5_ZZZ_UNCOMPRESSED			0
#define DB5_ZZZ_GNU_GZIP			1
#define DB5_ZZZ_BURROUGHS_WHEELER		2


/* major_type */
#define DB5_MAJORTYPE_RESERVED			 0
#define DB5_MAJORTYPE_BRLCAD			 1
#define DB5_MAJORTYPE_ATTRIBUTE_ONLY		 2
#define DB5_MAJORTYPE_BINARY_MASK		 0x18
#define DB5_MAJORTYPE_BINARY_EXPM		 8
#define DB5_MAJORTYPE_BINARY_UNIF		 9
#define DB5_MAJORTYPE_BINARY_MIME		10

/*
 *	Minor types
 */
/* BRL-CAD */
#define DB5_MINORTYPE_BRLCAD_TOR		 1
#define DB5_MINORTYPE_BRLCAD_TGC		 2
#define DB5_MINORTYPE_BRLCAD_ELL		 3
#define DB5_MINORTYPE_BRLCAD_ARB8		 4
#define DB5_MINORTYPE_BRLCAD_ARS		 5
#define DB5_MINORTYPE_BRLCAD_HALF		 6
#define DB5_MINORTYPE_BRLCAD_REC		 7
#define DB5_MINORTYPE_BRLCAD_POLY		 8
#define DB5_MINORTYPE_BRLCAD_BSPLINE		 9
#define DB5_MINORTYPE_BRLCAD_SPH		10
#define	DB5_MINORTYPE_BRLCAD_NMG		11
#define DB5_MINORTYPE_BRLCAD_EBM		12
#define DB5_MINORTYPE_BRLCAD_VOL		13
#define DB5_MINORTYPE_BRLCAD_ARBN		14
#define DB5_MINORTYPE_BRLCAD_PIPE		15
#define DB5_MINORTYPE_BRLCAD_PARTICLE		16
#define DB5_MINORTYPE_BRLCAD_RPC		17
#define DB5_MINORTYPE_BRLCAD_RHC		18
#define DB5_MINORTYPE_BRLCAD_EPA		19
#define DB5_MINORTYPE_BRLCAD_EHY		20
#define DB5_MINORTYPE_BRLCAD_ETO		21
#define DB5_MINORTYPE_BRLCAD_GRIP		22
#define DB5_MINORTYPE_BRLCAD_JOINT		23
#define DB5_MINORTYPE_BRLCAD_HF			24
#define DB5_MINORTYPE_BRLCAD_DSP		25
#define	DB5_MINORTYPE_BRLCAD_SKETCH		26
#define	DB5_MINORTYPE_BRLCAD_EXTRUDE		27
#define DB5_MINORTYPE_BRLCAD_SUBMODEL		28
#define	DB5_MINORTYPE_BRLCAD_CLINE		29
#define	DB5_MINORTYPE_BRLCAD_BOT		30
#define DB5_MINORTYPE_BRLCAD_COMBINATION	31
#define DB5_MINORTYPE_BRLCAD_SUPERELL		32

/* Uniform-array binary */
#define DB5_MINORTYPE_BINU_WID_MASK		0x30
#define DB5_MINORTYPE_BINU_SGN_MASK		0x08
#define DB5_MINORTYPE_BINU_ATM_MASK		0x07
#define DB5_MINORTYPE_BINU_FLOAT		0x02
#define DB5_MINORTYPE_BINU_DOUBLE		0x03
#define DB5_MINORTYPE_BINU_8BITINT_U		0x04
#define DB5_MINORTYPE_BINU_16BITINT_U		0x05
#define DB5_MINORTYPE_BINU_32BITINT_U		0x06
#define DB5_MINORTYPE_BINU_64BITINT_U		0x07
#define DB5_MINORTYPE_BINU_8BITINT		0x0c
#define DB5_MINORTYPE_BINU_16BITINT		0x0d
#define DB5_MINORTYPE_BINU_32BITINT		0x0e
#define DB5_MINORTYPE_BINU_64BITINT		0x0f

/* this array depends on the values of the above definitions and is defined in db5_bin.c */
extern const char *binu_types[];

/*
 *  The "raw internal" form of one database object.
 *  This is what the low-level database routines will operate on.
 *  Magic number1 has already been checked, and is not stored.
 */
struct db5_raw_internal {
	long		magic;
	unsigned char	h_object_width;		/* DB5HDR_WIDTHCODE_x */
	unsigned char	h_name_hidden;
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
#define RT_CK_RIP(_ptr)		BU_CKMAG( _ptr, DB5_RAW_INTERNAL_MAGIC, "db5_raw_internal" )

extern const int db5_enc_len[4];	/* convert wid to nbytes */

extern unsigned char *db5_encode_length(
	unsigned char	*cp,
	long		val,
	int		format);
const unsigned char *db5_get_raw_internal_ptr(
	struct db5_raw_internal *rip,
	const unsigned char *ip);


#endif	/* DB5_H */



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
