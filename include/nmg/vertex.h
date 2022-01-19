/*                       V E R T E X . H
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
/** @addtogroup nmg
 * @brief
 * NMG vertex definitions
 */
/** @{ */
/** @file nmg/vertex.h */

#ifndef NMG_VERTEX_H
#define NMG_VERTEX_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_VERTEX(_p)             NMG_CKMAG(_p, NMG_VERTEX_MAGIC, "vertex")
#define NMG_CK_VERTEX_G(_p)           NMG_CKMAG(_p, NMG_VERTEX_G_MAGIC, "vertex_g")
#define NMG_CK_VERTEXUSE(_p)          NMG_CKMAG(_p, NMG_VERTEXUSE_MAGIC, "vertexuse")
#define NMG_CK_VERTEXUSE_A_PLANE(_p)  NMG_CKMAG(_p, NMG_VERTEXUSE_A_PLANE_MAGIC, "vertexuse_a_plane")

#define GET_VERTEX(p, m)            {NMG_GETSTRUCT(p, vertex); NMG_INCR_INDEX(p, m);}
#define GET_VERTEX_G(p, m)          {NMG_GETSTRUCT(p, vertex_g); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE(p, m)         {NMG_GETSTRUCT(p, vertexuse); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_PLANE(p, m) {NMG_GETSTRUCT(p, vertexuse_a_plane); NMG_INCR_INDEX(p, m);}
#define GET_VERTEXUSE_A_CNURB(p, m) {NMG_GETSTRUCT(p, vertexuse_a_cnurb); NMG_INCR_INDEX(p, m);}
#define FREE_VERTEX(p)            NMG_FREESTRUCT(p, vertex)
#define FREE_VERTEX_G(p)          NMG_FREESTRUCT(p, vertex_g)
#define FREE_VERTEXUSE(p)         NMG_FREESTRUCT(p, vertexuse)
#define FREE_VERTEXUSE_A_PLANE(p) NMG_FREESTRUCT(p, vertexuse_a_plane)
#define FREE_VERTEXUSE_A_CNURB(p) NMG_FREESTRUCT(p, vertexuse_a_cnurb)

NMG_EXPORT extern int nmg_kvu(struct vertexuse *vu);


#define PREEXIST 1
#define NEWEXIST 2


#define VU_PREEXISTS(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = PREEXIST; }

#define VU_NEW(_bs, _vu) { chkidxlist((_bs), (_vu)); \
        (_bs)->vertlist[(_vu)->index] = NEWEXIST; }


__END_DECLS

#endif  /* NMG_VERTEX_H */
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
