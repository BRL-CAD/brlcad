/*                      B I N U N I F . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2026 United States Government as represented by
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
/** @file rt/binunif.h
 *
 */

#ifndef RT_BINUNIF_H
#define RT_BINUNIF_H

#include "common.h"
#include "vmath.h"
#include "bu/vls.h"
#include "wdb.h"
#include "rt/defines.h"
#include "rt/directory.h"
#include "rt/nongeom.h"

__BEGIN_DECLS

/** Base DB5 BINUNIF minor type bits accepted by rt_mk_binunif(). */
#define RT_BINUNIF_TYPE_MASK 0x00ffu

/**
 * The input file has network byte order.  Input is assumed to have host byte
 * order when this flag is absent.
 */
#define RT_BINUNIF_NETWORK_ORDER 0x0100u

/**
 * Import a typed raw file as a uniform binary database object.
 *
 * input_type is a DB5_MINORTYPE_BINU_* value, optionally combined with
 * RT_BINUNIF_NETWORK_ORDER.  File input defaults to host byte order.  A
 * positive max_count limits the number of imported elements; zero imports the
 * complete file.  The resulting in-memory elements use host byte order and
 * are serialized into the database in network byte order.
 *
 * Returns 0 on success and -1 on error.
 */
RT_EXPORT extern int rt_mk_binunif(struct rt_wdb *wdbp,
				   const char *obj_name,
				   const char *file_name,
				   unsigned int input_type,
				   size_t max_count);


/* defined in db5_bin.c */

/**
 * Free the storage associated with a binunif_internal object
 */
RT_EXPORT extern void rt_binunif_free(struct rt_binunif_internal *bip);

/**
 * Diagnostic routine
 */
RT_EXPORT extern void rt_binunif_dump(struct rt_binunif_internal *bip);

/**
 * Decode binunif type into string.
 */
RT_EXPORT extern const char * rt_binunif_typestr(const struct directory *dp);

__END_DECLS

#endif /* RT_BINUNIF_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
