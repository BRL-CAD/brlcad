/*                         M A T E R . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2011 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file mater.h
 *
 * Information about mapping between region IDs and material settings
 * (colors and outboard database "handles").
 *
 */

#ifndef __MATER_H__
#define __MATER_H__

#include "bu.h"
#include "raytrace.h"

__BEGIN_DECLS

#ifndef RT_EXPORT
#  if defined(RT_DLL_EXPORTS) && defined(RT_DLL_IMPORTS)
#    error "Only RT_DLL_EXPORTS or RT_DLL_IMPORTS can be defined, not both."
#  elif defined(RT_DLL_EXPORTS)
#    define RT_EXPORT __declspec(dllexport)
#  elif defined(RT_DLL_IMPORTS)
#    define RT_EXPORT __declspec(dllimport)
#  else
#    define RT_EXPORT
#  endif
#endif

struct mater {
    short		mt_low;		/**< @brief bounds of region IDs, inclusive */
    short		mt_high;
    unsigned char	mt_r;		/**< @brief color */
    unsigned char	mt_g;
    unsigned char	mt_b;
    off_t		mt_daddr;	/**< @brief db address, for updating */
    struct mater	*mt_forw;	/**< @brief next in chain */
};
#define MATER_NULL	((struct mater *)0)
#define MATER_NO_ADDR	((off_t)0)		/**< @brief invalid mt_daddr */


RT_EXPORT extern void rt_region_color_map(struct region *regp);

/* process ID_MATERIAL record */
RT_EXPORT extern void rt_color_addrec(int low,
				      int hi,
				      int r,
				      int g,
				      int b,
				      off_t addr);
RT_EXPORT extern void rt_insert_color(struct mater *newp);
RT_EXPORT extern void rt_vls_color_map(struct bu_vls *str);
RT_EXPORT extern struct mater *rt_material_head();
RT_EXPORT extern void rt_new_material_head(struct mater *);
RT_EXPORT extern struct mater *rt_dup_material_head();
RT_EXPORT extern void rt_color_free();

__END_DECLS

#endif /* __MATER_H__ */

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
