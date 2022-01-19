/*                       E D G E . H
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
 * NMG edge definitions
 */
/** @{ */
/** @file nmg/edge.h */

#ifndef NMG_EDGE_H
#define NMG_EDGE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_EDGE(_p)               NMG_CKMAG(_p, NMG_EDGE_MAGIC, "edge")
#define NMG_CK_EDGE_G_LSEG(_p)        NMG_CKMAG(_p, NMG_EDGE_G_LSEG_MAGIC, "edge_g_lseg")
#define NMG_CK_EDGEUSE(_p)            NMG_CKMAG(_p, NMG_EDGEUSE_MAGIC, "edgeuse")

#define GET_EDGE(p, m)              {NMG_GETSTRUCT(p, edge); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_LSEG(p, m)       {NMG_GETSTRUCT(p, edge_g_lseg); NMG_INCR_INDEX(p, m);}
#define GET_EDGE_G_CNURB(p, m)      {NMG_GETSTRUCT(p, edge_g_cnurb); NMG_INCR_INDEX(p, m);}
#define GET_EDGEUSE(p, m)           {NMG_GETSTRUCT(p, edgeuse); NMG_INCR_INDEX(p, m);}

#define FREE_EDGE(p)              NMG_FREESTRUCT(p, edge)
#define FREE_EDGE_G_LSEG(p)       NMG_FREESTRUCT(p, edge_g_lseg)
#define FREE_EDGE_G_CNURB(p)      NMG_FREESTRUCT(p, edge_g_cnurb)
#define FREE_EDGEUSE(p)           NMG_FREESTRUCT(p, edgeuse)

/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct edgeuse *nmg_me(struct vertex *v1,
                                         struct vertex *v2,
                                         struct shell *s);
NMG_EXPORT extern struct edgeuse *nmg_meonvu(struct vertexuse *vu);
NMG_EXPORT extern int nmg_keg(struct edgeuse *eu);
NMG_EXPORT extern int nmg_keu(struct edgeuse *eu);

/**
 * Do two edgeuses share the same two vertices? If yes, eu's should be
 * joined.
 */
#define NMG_ARE_EUS_ADJACENT(_eu1, _eu2) (\
        ((_eu1)->vu_p->v_p == (_eu2)->vu_p->v_p && \
         (_eu1)->eumate_p->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p)  || \
        ((_eu1)->vu_p->v_p == (_eu2)->eumate_p->vu_p->v_p && \
         (_eu1)->eumate_p->vu_p->v_p == (_eu2)->vu_p->v_p))

/** Compat: Used in nmg_misc.c and nmg_mod.c */
#define EDGESADJ(_e1, _e2) NMG_ARE_EUS_ADJACENT(_e1, _e2)


__END_DECLS

#endif  /* NMG_EDGE_H */
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
