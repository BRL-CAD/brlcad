/*                        T C L C A D . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2021 United States Government as represented by
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
/** @addtogroup libtclcad */
/** @{ */
/** @file tclcad/defines.h
 *
 * @brief
 *  Definitions header file for the BRL-CAD TclCAD Library, LIBTCLCAD.
 *
 */

#ifndef TCLCAD_DEFINES_H
#define TCLCAD_DEFINES_H

#include "common.h"

__BEGIN_DECLS

#ifndef TCLCAD_EXPORT
#  if defined(TCLCAD_DLL_EXPORTS) && defined(TCLCAD_DLL_IMPORTS)
#    error "Only TCLCAD_DLL_EXPORTS or TCLCAD_DLL_IMPORTS can be defined, not both."
#  elif defined(TCLCAD_DLL_EXPORTS)
#    define TCLCAD_EXPORT COMPILER_DLLEXPORT
#  elif defined(TCLCAD_DLL_IMPORTS)
#    define TCLCAD_EXPORT COMPILER_DLLIMPORT
#  else
#    define TCLCAD_EXPORT
#  endif
#endif

#define TCLCAD_IDLE_MODE 0
#define TCLCAD_ROTATE_MODE 1
#define TCLCAD_TRANSLATE_MODE 2
#define TCLCAD_SCALE_MODE 3
#define TCLCAD_CONSTRAINED_ROTATE_MODE 4
#define TCLCAD_CONSTRAINED_TRANSLATE_MODE 5
#define TCLCAD_OROTATE_MODE 6
#define TCLCAD_OSCALE_MODE 7
#define TCLCAD_OTRANSLATE_MODE 8
#define TCLCAD_MOVE_ARB_EDGE_MODE 9
#define TCLCAD_MOVE_ARB_FACE_MODE 10
#define TCLCAD_ROTATE_ARB_FACE_MODE 11
#define TCLCAD_PROTATE_MODE 12
#define TCLCAD_PSCALE_MODE 13
#define TCLCAD_PTRANSLATE_MODE 14
// TCLCAD_POLY_CIRCLE_MODE replaced by BV_POLY_CIRCLE_MODE
// TCLCAD_POLY_CONTOUR_MODE replaced by BV_POLY_CONTOUR_MODE
#define TCLCAD_POLY_ELLIPSE_MODE 17
#define TCLCAD_POLY_RECTANGLE_MODE 18
#define TCLCAD_POLY_SQUARE_MODE 19
#define TCLCAD_RECTANGLE_MODE 20
#define TCLCAD_MOVE_METABALL_POINT_MODE 21
#define TCLCAD_MOVE_PIPE_POINT_MODE 22
#define TCLCAD_MOVE_BOT_POINT_MODE 23
#define TCLCAD_MOVE_BOT_POINTS_MODE 24
#define TCLCAD_DATA_MOVE_OBJECT_MODE 25
#define TCLCAD_DATA_MOVE_POINT_MODE 26
#define TCLCAD_DATA_SCALE_MODE 27

#define TCLCAD_OBJ_FB_MODE_OFF 0
#define TCLCAD_OBJ_FB_MODE_UNDERLAY 1
#define TCLCAD_OBJ_FB_MODE_INTERLAY 2
#define TCLCAD_OBJ_FB_MODE_OVERLAY  3

/* Use fbserv */
#define USE_FBSERV 1

__END_DECLS

#endif /* TCLCAD_DEFINES_H */

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
