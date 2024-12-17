/*                       G L O B A L . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2024 United States Government as represented by
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
/** @file rt/global.h
 *
 */

#ifndef RT_GLOBAL_H
#define RT_GLOBAL_H

#include "common.h"

#include "rt/wdb.h"
#include "vmath.h"

__BEGIN_DECLS


/**
 * Definitions for librt.a which are global to the library regardless
 * of how many different models are being worked on
 */


/**
 * Applications that are going to use rtg_vlfree
 * are required to execute this macro once, first:
 *
 * BU_LIST_INIT(&RTG.rtg_vlfree);
 */

struct rt_g {
    struct bu_list      rtg_vlfree;     /**< @brief  head of bv_vlist freelist */
};
#define RT_G_INIT_ZERO { BU_LIST_INIT_ZERO }

/**
 * global ray-trace geometry state
 */
RT_EXPORT extern struct rt_g RTG;

__END_DECLS

#endif /* RT_GLOBAL_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
