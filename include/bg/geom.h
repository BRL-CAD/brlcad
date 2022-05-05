/*                        G E O M . H
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
/** @addtogroup bg_geom
 *
 * @brief Definitions of geometric shapes.
 *
 * Originally, these definitions were part of rt/geom.h (which still holds
 * internal definitions not also used by libbg.)  These moved here are
 * relocated to allow libbg to define shape-specific logic without requiring
 * duplicate (or different) definitions for the same concepts or introducing
 * a cyclic dependency problem (librt depends on libbg, not vice versa.)
 *
 * Geometry shape structures define here may also be used by librt's struct
 * rt_db_internal generic pointer idb_ptr, based on idb_type indicating a solid
 * id ID_xxx, such as ID_TGC.
 */
/** @{ */
/** @file bg/geom.h */
/** @} */

#ifndef BG_GEOM_H
#define BG_GEOM_H

#include "common.h"
#include "vmath.h"
#include "bg/defines.h"

__BEGIN_DECLS

/*
 * Torus (ID_TOR)
 */
struct bg_torus {
    uint32_t magic;
    point_t v;		/**< @brief center point */
    vect_t h;		/**< @brief normal, unit length */
    fastf_t r_h;	/**< @brief radius in H direction (r2) */
    fastf_t r_a;	/**< @brief radius in A direction (r1) */
    /* REMAINING ELEMENTS PROVIDED BY IMPORT, UNUSED BY EXPORT */
    vect_t a;		/**< @brief r_a length */
    vect_t b;		/**< @brief r_b length */
    fastf_t r_b;	/**< @brief radius in B direction (typ == r_a) */
};
#define BG_TOR_CK_MAGIC(_p) BU_CKMAG(_p, BG_TOR_MAGIC, "bg_torus")

/** @} */

__END_DECLS

#endif /* BG_GEOM_H */

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
