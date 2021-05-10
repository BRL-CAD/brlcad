/*                        B V I E W . H
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
/** @addtogroup libbview */
/** @{ */
/** @file bview.h
 *
 */

// TODO - bview currently conflates scene and camera concepts - for example,
// data axes and polygons are properly scene objects being viewed by the
// camera, but at the moment they're directly part of the bview struct.
//
// The plan is to address this, so until this notice is removed the bview data
// structure and related data structures should be considered in flux.

#ifndef BVIEW_H
#define BVIEW_H

#include "common.h"

#include "vmath.h"
#include "bu/vls.h"
#include "bn.h"

#include "./bview/defines.h"
#include "./bview/adc.h"
#include "./bview/polygons.h"
#include "./bview/util.h"

#endif /* BVIEW_H */

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
