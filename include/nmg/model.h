/*                        M O D E L . H
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
 * NMG model definitions
 */
/** @{ */
/** @file nmg/model.h */

#ifndef NMG_MODEL_H
#define NMG_MODEL_H

#include "common.h"

#include "vmath.h"
#include "bu/list.h"
#include "nmg/defines.h"
#include "nmg/topology.h"

__BEGIN_DECLS

#define NMG_CK_MODEL(_p)              NMG_CKMAG(_p, NMG_MODEL_MAGIC, "model")

#define GET_MODEL(p, m)             {NMG_GETSTRUCT(p, model); NMG_INCR_INDEX(p, m);}
#define FREE_MODEL(p)             NMG_FREESTRUCT(p, model)

/* From file nmg_mk.c */
/*      MAKE & KILL routines */
NMG_EXPORT extern struct model *nmg_mm(void);
NMG_EXPORT extern struct model *nmg_mmr(void);
NMG_EXPORT extern struct nmgregion *nmg_mrsv(struct model *m);
NMG_EXPORT extern void nmg_km(struct model *m);


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
