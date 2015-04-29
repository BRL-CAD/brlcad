/*                          M A T E R . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2015 United States Government as represented by
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

__BEGIN_DECLS

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
