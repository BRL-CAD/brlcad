/*                        I O . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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

/*----------------------------------------------------------------------*/
/** @addtogroup nmg_io */
/** @{ */
/** @file nmg/io.h */

#ifndef NMG_IO_H
#define NMG_IO_H

#include "common.h"

#include "vmath.h"
#include "bu/parse.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define V4_NAMESIZE 16
struct nmg_rec {
    char        N_id;                   /* DBID_NMG */
    char        N_version;              /* Version indicator */
    char        N_name[V4_NAMESIZE];
    char        N_pad2[2];              /* neatness */
    unsigned char       N_count[4];     /* # additional granules */
    unsigned char       N_structs[26*4];/* # of structs needed */
}; /* struct nmg_rec */


#define DISK_MODEL_VERSION 1	/* V0 was Release 4.0 */

/* The last parameter is either sizeof(union record) (for v4 .g files) or
 * 0 (for v5 and later.) */
NMG_EXPORT extern struct model *
nmg_import(const struct bu_external *ep, const mat_t mat, int record_sizeof);

NMG_EXPORT extern int
nmg_export(struct bu_external *ep, struct model *m, double local2mm, int ver);

__END_DECLS

#endif  /* NMG_MODEL_H */
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
