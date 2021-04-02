/*                         S O L I D . H
 * BRL-CAD
 *
 * Copyright (c) 1985-2021 United States Government as represented by
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
/** @addtogroup rt_solid
 *
 * Solids structure definition
 *
 */

#ifndef RT_SOLID_H
#define RT_SOLID_H

#include "common.h"
#include "vmath.h"
#include "bu/list.h"
#include "rt/defines.h"
#include "rt/db_fullpath.h"

/** @{ */
/** @file solid.h */

struct bview_scene_obj  {
    struct bu_list l;

    /* Display properties */
    char s_flag;		/**< @brief  UP = object visible, DOWN = obj invis */
    char s_iflag;	        /**< @brief  UP = illuminated, DOWN = regular */
    char s_soldash;		/**< @brief  solid/dashed line flag */
    char s_uflag;		/**< @brief  1 - the user specified the color */
    char s_dflag;		/**< @brief  1 - s_basecolor is derived from the default */
    char s_cflag;		/**< @brief  1 - use the default color */
    char s_wflag;		/**< @brief  work flag */
    unsigned char s_basecolor[3];	/**< @brief  color from containing region */
    unsigned char s_color[3];	/**< @brief  color to draw as */
    fastf_t s_transparency;	/**< @brief  holds a transparency value in the range [0.0, 1.0] */
    int s_hiddenLine;         	/**< @brief  1 - hidden line */
    int s_dmode;         	/**< @brief  draw mode: 0 - wireframe
				 *	      1 - shaded bots and polysolids only (booleans NOT evaluated)
				 *	      2 - shaded (booleans NOT evaluated)
				 *	      3 - shaded (booleans evaluated)
				 */

    /* Actual 3D geometry data and information */
    struct bu_list s_vlist;	/**< @brief  Pointer to unclipped vector list */
    int s_vlen;			/**< @brief  Number of actual cmd[] entries in vlist */
    unsigned int s_dlist;	/**< @brief  display list index */
    fastf_t s_size;		/**< @brief  Distance across solid, in model space */
    fastf_t s_csize;		/**< @brief  Dist across clipped solid (model space) */
    vect_t s_center;		/**< @brief  Center point of solid, in model space */


    /* View object name */
    struct bu_vls name;

    /* Database object related info */
    int s_represents_dbobj;
    char s_Eflag;		/**< @brief  flag - not a solid but an "E'd" region */
    short s_regionid;		/**< @brief  region ID */
    mat_t s_mat;		/**< @brief mat to use for internal lookup */
    struct db_full_path s_fullpath;

    /* User data to associate with this view object */
    void *s_u_data;
};

#define GET_BVIEW_SCENE_OBJ(p, fp) { \
	if (BU_LIST_IS_EMPTY(fp)) { \
	    BU_ALLOC((p), struct bview_scene_obj); \
	    db_full_path_init(&(p)->s_fullpath); \
	} else { \
	    p = BU_LIST_NEXT(bview_scene_obj, fp); \
	    BU_LIST_DEQUEUE(&((p)->l)); \
	    (p)->s_fullpath.fp_len = 0; \
	} \
	BU_LIST_INIT( &((p)->s_vlist) ); }

#define FREE_BVIEW_SCENE_OBJ(p, fp) { \
	BU_LIST_APPEND(fp, &((p)->l)); \
	RT_FREE_VLIST(&((p)->s_vlist)); }

#define FOR_ALL_BVIEW_SCENE_OBJS(p, hp)  \
    for (BU_LIST_FOR(p, bview_scene_obj, hp))



/** Obtain the last node (the solid) on the path */
#define LAST_SOLID(_sp)	DB_FULL_PATH_CUR_DIR( &(_sp)->s_fullpath )
#define FIRST_SOLID(_sp)	((_sp)->s_fullpath.fp_names[0])

#endif /* RT_SOLID_H */

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
