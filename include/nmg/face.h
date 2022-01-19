/*                       F A C E . H
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
 * NMG face definitions
 */
/** @{ */
/** @file nmg/face.h */

#ifndef NMG_FACE_H
#define NMG_FACE_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_FACE(_p)               NMG_CKMAG(_p, NMG_FACE_MAGIC, "face")
#define NMG_CK_FACE_G_PLANE(_p)       NMG_CKMAG(_p, NMG_FACE_G_PLANE_MAGIC, "face_g_plane")
#define NMG_CK_FACE_G_SNURB(_p)       NMG_CKMAG(_p, NMG_FACE_G_SNURB_MAGIC, "face_g_snurb")
#define NMG_CK_FACE_G_EITHER(_p)      NMG_CK2MAG(_p, NMG_FACE_G_PLANE_MAGIC, NMG_FACE_G_SNURB_MAGIC, "face_g_plane|face_g_snurb")
#define NMG_CK_FACEUSE(_p)            NMG_CKMAG(_p, NMG_FACEUSE_MAGIC, "faceuse")

/**
 * Returns a 4-tuple (plane_t), given faceuse and state of flip flags.
 */
#define NMG_GET_FU_PLANE(_N, _fu) { \
        register const struct faceuse *_fu1 = (_fu); \
        register const struct face_g_plane *_fg; \
        NMG_CK_FACEUSE(_fu1); \
        NMG_CK_FACE(_fu1->f_p); \
        _fg = _fu1->f_p->g.plane_p; \
        NMG_CK_FACE_G_PLANE(_fg); \
        if ((_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0)) { \
            HREVERSE(_N, _fg->N); \
        } else { \
            HMOVE(_N, _fg->N); \
        } }

/** Returns a 3-tuple (vect_t), given faceuse and state of flip flags */
#define NMG_GET_FU_NORMAL(_N, _fu) { \
        register const struct faceuse *_fu1 = (_fu); \
        register const struct face_g_plane *_fg; \
        NMG_CK_FACEUSE(_fu1); \
        NMG_CK_FACE(_fu1->f_p); \
        _fg = _fu1->f_p->g.plane_p; \
        NMG_CK_FACE_G_PLANE(_fg); \
        if ((_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0)) { \
            VREVERSE(_N, _fg->N); \
        } else { \
            VMOVE(_N, _fg->N); \
        } }

/**
 * Returns a 4-tuple (plane_t), given faceuse and state of flip flags.
 */
#define NMG_GET_FU_PLANE(_N, _fu) { \
        register const struct faceuse *_fu1 = (_fu); \
        register const struct face_g_plane *_fg; \
        NMG_CK_FACEUSE(_fu1); \
        NMG_CK_FACE(_fu1->f_p); \
        _fg = _fu1->f_p->g.plane_p; \
        NMG_CK_FACE_G_PLANE(_fg); \
        if ((_fu1->orientation != OT_SAME) != (_fu1->f_p->flip != 0)) { \
            HREVERSE(_N, _fg->N); \
        } else { \
            HMOVE(_N, _fg->N); \
        } }

#define GET_FACE(p, m)              {NMG_GETSTRUCT(p, face); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_PLANE(p, m)      {NMG_GETSTRUCT(p, face_g_plane); NMG_INCR_INDEX(p, m);}
#define GET_FACE_G_SNURB(p, m)      {NMG_GETSTRUCT(p, face_g_snurb); NMG_INCR_INDEX(p, m);}
#define GET_FACEUSE(p, m)           {NMG_GETSTRUCT(p, faceuse); NMG_INCR_INDEX(p, m);}

#define FREE_FACE(p)              NMG_FREESTRUCT(p, face)
#define FREE_FACE_G_PLANE(p)      NMG_FREESTRUCT(p, face_g_plane)
#define FREE_FACE_G_SNURB(p)      NMG_FREESTRUCT(p, face_g_snurb)
#define FREE_FACEUSE(p)           NMG_FREESTRUCT(p, faceuse)

NMG_EXPORT extern struct faceuse *nmg_find_fu_of_eu(const struct edgeuse *eu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_lu(const struct loopuse *lu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_of_vu(const struct vertexuse *vu);
NMG_EXPORT extern struct faceuse *nmg_find_fu_with_fg_in_s(const struct shell *s1,
                                                           const struct faceuse *fu2);
NMG_EXPORT extern double nmg_measure_fu_angle(const struct edgeuse *eu,
                                              const vect_t xvec,
                                              const vect_t yvec,
                                              const vect_t zvec);

/* From file nmg_mk.c */
/*      MAKE routines */
NMG_EXPORT extern struct faceuse *nmg_mf(struct loopuse *lu1);
NMG_EXPORT extern int nmg_kfu(struct faceuse *fu1);


__END_DECLS

#endif  /* NMG_FACE_H */
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
