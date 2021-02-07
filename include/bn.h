/*                            B N . H
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


/** @addtogroup libbn
 *
 * @brief
 * The library provides a broad assortment of numerical algorithms and
 * computational routines, including random number generation, vector
 * math, matrix math, quaternion math, complex math, synthetic
 * division, root finding, etc.
 *
 * The functionality provided by this library is specified in the bn.h
 * header or appropriate included files from the ./bn subdirectory.
 *
 */
/** @{ */
/** @brief Header file for the BRL-CAD Numerical Computation Library, LIBBN.*/
/** @file bn.h */
/** @} */

#ifndef BN_H
#define BN_H

#include "common.h"

#include "bn/defines.h"
#include "bn/version.h"
#include "bn/tol.h"
#include "bn/anim.h"
#include "bn/complex.h"
#include "bn/mat.h"
#include "bn/msr.h"
#include "bn/noise.h"
#include "bn/poly.h"
#include "bn/plot3.h"
#include "bn/multipoly.h"
#include "bn/qmath.h"
#include "bn/rand.h"
#include "bn/randmt.h"
#include "bn/wavelet.h"
#include "bn/sobol.h"
#include "bn/str.h"
#include "bn/tabdata.h"
#include "bn/vlist.h"
#include "bn/vert_tree.h"
#include "bn/vectfont.h"
#include "bn/plane.h"
#include "bn/clip.h"
#include "bn/adc.h"

#endif /* BN_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
