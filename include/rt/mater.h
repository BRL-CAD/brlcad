/*                          M A T E R . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2021 United States Government as represented by
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
/** @file mater.h
 *
 */

#ifndef RT_MATER_H
#define RT_MATER_H

#include "common.h"
#include "rt/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

struct region; /* forward declaration */

/**
 * Container for material information
 */
struct mater_info {
    float       ma_color[3];    /**< @brief explicit color:  0..1  */
    float       ma_temperature; /**< @brief positive ==> degrees Kelvin */
    char        ma_color_valid; /**< @brief non-0 ==> ma_color is non-default */
    char        ma_cinherit;    /**< @brief color: DB_INH_LOWER / DB_INH_HIGHER */
    char        ma_minherit;    /**< @brief mater: DB_INH_LOWER / DB_INH_HIGHER */
    char        *ma_shader;     /**< @brief shader name & parms */
};
#define RT_MATER_INFO_INIT_ZERO { VINIT_ZERO, 0.0, 0, 0, 0, NULL }
/* From MGED initial tree state */
#define RT_MATER_INFO_INIT_IDN { {1.0, 0.0, 0.0} , -1.0, 0, 0, 0, NULL }

struct mater {
    short		mt_low;		/**< @brief bounds of region IDs, inclusive */
    short		mt_high;
    unsigned char	mt_r;		/**< @brief color */
    unsigned char	mt_g;
    unsigned char	mt_b;
    b_off_t		mt_daddr;	/**< @brief db address, for updating */
    struct mater	*mt_forw;	/**< @brief next in chain */
};
#define MATER_NULL	((struct mater *)0)
#define MATER_NO_ADDR	((b_off_t)0)		/**< @brief invalid mt_daddr */


RT_EXPORT extern void rt_region_color_map(struct region *regp);

/* process ID_MATERIAL record */
RT_EXPORT extern void rt_color_addrec(int low,
				      int hi,
				      int r,
				      int g,
				      int b,
				      b_off_t addr);
RT_EXPORT extern void rt_insert_color(struct mater *newp);
RT_EXPORT extern void rt_vls_color_map(struct bu_vls *str);
RT_EXPORT extern struct mater *rt_material_head(void);
RT_EXPORT extern void rt_new_material_head(struct mater *);
RT_EXPORT extern struct mater *rt_dup_material_head(void);
RT_EXPORT extern void rt_color_free(void);

__END_DECLS

#endif /* RT_MATER_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
