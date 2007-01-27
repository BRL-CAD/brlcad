/*                     D B 5 _ T Y P E S . C
 * BRL-CAD
 *
 * Copyright (c) 2000-2007 United States Government as represented by
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
/** @file db5_types.c
 *
 *	Map between Major_Types/Minor_Types and ASCII strings
 *
 *  Author -
 *	Paul J. Tanenbaum
 *
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSell[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"


#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <math.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "db5.h"
#include "nmg.h"
#include "raytrace.h"

struct db5_type {
    int		major_code;
    int		minor_code;
    int		heed_minor;
    char	*tag;
    char	*description;
};

/*
 *	In order to support looking up Major_Types
 *	as well as (Major_Type, Minor_Type) pairs,
 *	every Major_Type needs an entry with heed_minor==0
 *	and it must occur below any of its entries that
 *	have heed_minor==1.
 */
const static struct db5_type type_table[] = {
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_TOR, 1, "tor", "torus"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_TGC, 1, "tgc", "truncated general cone"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ELL, 1, "ell", "ellipsoid"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARB8, 1, "arb8", "arb8"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARS, 1, "ars", "waterline"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_HALF, 1, "half", "halfspace"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_REC, 1, "rec", "right elliptical cylinder"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_BSPLINE, 1, "bspline", "B-spline"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SPH, 1, "sph", "sphere"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_NMG, 1, "nmg", "nmg"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EBM, 1, "ebm", "extruded bitmap"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_VOL, 1, "vol", "voxels"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ARBN, 1, "arbn", "arbn"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_PIPE, 1, "pipe", "pipe"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_PARTICLE, 1, "particle", "particle"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_RPC, 1, "rpc", "right parabolic cylinder"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_RHC, 1, "rhc", "right hyperbolic cylinder"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EPA, 1, "epa", "elliptical paraboloid"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EHY, 1, "ehy", "elliptical hyperboloid"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_ETO, 1, "eto", "elliptical torus"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_GRIP, 1, "grip", "grip"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_JOINT, 1, "joint", "joint"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_DSP, 1, "dsp", "displacement map (height field)"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SKETCH, 1, "sketch", "sketch"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_EXTRUDE, 1, "extrude", "extrusion"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_SUBMODEL, 1, "submodel", "submodel"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_CLINE, 1, "cline", "cline"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_BOT, 1, "bot", "bag of triangles"
    },
    {
	DB5_MAJORTYPE_BRLCAD, DB5_MINORTYPE_BRLCAD_COMBINATION, 1, "combination", "combination"
    },
    {
	DB5_MAJORTYPE_BRLCAD, 0, 0, "brlcad", "BRL-CAD geometry"
    },
    {
	DB5_MAJORTYPE_ATTRIBUTE_ONLY, 0, 0, "attribonly", "attribute only"
    },
    {
	DB5_MAJORTYPE_BINARY_EXPM, 0, 0, "binexpm", "experimental binary"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_FLOAT, 1, "float", "array of floats"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_FLOAT, 1, "f", "array of floats"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_DOUBLE, 1, "double", "array of doubles"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_DOUBLE, 1, "d", "array of doubles"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT_U, 1, "u8", "array of unsigned 8-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_16BITINT_U, 1, "u16", "array of unsigned 16-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "u32", "array of unsigned 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "uint", "array of unsigned 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT_U, 1, "ui", "array of unsigned 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_64BITINT_U, 1, "u64", "array of unsigned 64-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_8BITINT, 1, "8", "array of 8-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_16BITINT, 1, "16", "array of 16-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "32", "array of 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "int", "array of 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_32BITINT, 1, "i", "array of 32-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, DB5_MINORTYPE_BINU_64BITINT, 1, "64", "array of 64-bit ints"
    },
    {
	DB5_MAJORTYPE_BINARY_UNIF, 0, 0, "binunif", "uniform-array binary"
    },
    {
	DB5_MAJORTYPE_BINARY_MIME, 0, 0, "binmime", "MIME-typed binary"
    },
    /*
     *	Following entry must be at end of table
     */
    {
	DB5_MAJORTYPE_RESERVED, 0, 0, 0, 0
    },
};

int
db5_type_tag_from_major( char **tag, const int major ) {
    register struct db5_type	*tp;

    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if ((tp -> major_code == major) && !(tp -> heed_minor)) {
	    *tag = tp -> tag;
	    return 0;
	}
    }
    return 1;
}

int
db5_type_descrip_from_major( char **descrip, const int major ) {
    register struct db5_type	*tp;

    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if ((tp -> major_code == major) && !(tp -> heed_minor)) {
	    *descrip = tp -> description;
	    return 0;
	}
    }
    return 1;
}

int
db5_type_tag_from_codes( char **tag, const int major, const int minor ) {
    register struct db5_type	*tp;
    register int		found_minors = 0;

    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if (tp -> major_code == major) {
	    if (tp -> heed_minor)
		found_minors = 1;
	    if ((tp -> minor_code == minor) || !found_minors) {
		*tag = tp -> tag;
		return 0;
	    }
	}
    }
    return 1;
}

int
db5_type_descrip_from_codes( char **descrip, const int major,
			    const int minor ) {
    register struct db5_type	*tp;
    register int		found_minors = 0;

    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if (tp -> major_code == major) {
	    if (tp -> heed_minor)
		found_minors = 1;
	    if ((tp -> minor_code == minor) || !found_minors) {
		*descrip = tp -> description;
		return 0;
	    }
	}
    }
    return 1;
}

int
db5_type_codes_from_tag( int *major, int *minor, const char *tag ) {
    register struct db5_type	*tp;


    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if ((*(tp -> tag) == *tag) && (strcmp(tp -> tag, tag) == 0)) {
	    *major = tp -> major_code;
	    *minor = tp -> minor_code;
	    return 0;
	}
    }
    return 1;
}

int
db5_type_codes_from_descrip( int *major, int *minor, const char *descrip ) {
    register struct db5_type	*tp;


    for (tp = (struct db5_type *) type_table;
	    tp -> major_code != DB5_MAJORTYPE_RESERVED;
	    ++tp) {
	if ((*(tp -> description) == *descrip)
	 && (strcmp(tp -> description, descrip) == 0)) {
	    *major = tp -> major_code;
	    *minor = tp -> minor_code;
	    return 0;
	}
    }
    return 1;
}

size_t
db5_type_sizeof_h_binu( const int minor ) {
    switch ( minor ) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    return sizeof(float);
	case DB5_MINORTYPE_BINU_DOUBLE:
	    return sizeof(double);
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    return (size_t) 1;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    return (size_t) 2;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    return (size_t) 4;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    return (size_t) 8;
    }
    return 0;
}

size_t
db5_type_sizeof_n_binu( const int minor ) {
    switch ( minor ) {
	case DB5_MINORTYPE_BINU_FLOAT:
	    return (size_t) SIZEOF_NETWORK_FLOAT;
	case DB5_MINORTYPE_BINU_DOUBLE:
	    return (size_t) SIZEOF_NETWORK_DOUBLE;
	case DB5_MINORTYPE_BINU_8BITINT:
	case DB5_MINORTYPE_BINU_8BITINT_U:
	    return (size_t) 1;
	case DB5_MINORTYPE_BINU_16BITINT:
	case DB5_MINORTYPE_BINU_16BITINT_U:
	    return (size_t) 2;
	case DB5_MINORTYPE_BINU_32BITINT:
	case DB5_MINORTYPE_BINU_32BITINT_U:
	    return (size_t) 4;
	case DB5_MINORTYPE_BINU_64BITINT:
	case DB5_MINORTYPE_BINU_64BITINT_U:
	    return (size_t) 8;
    }
    return 0;
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
