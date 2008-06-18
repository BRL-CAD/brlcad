/*                   	P C _ S O L V E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2012 United States Government as represented by
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
/** @addtogroup soln */
/** @{ */
/** @file pc_solver.cpp
 *
 * Library for writing MGED databases from arbitrary procedures.
 * Assumes that some of the structure of such databases are known by
 * the calling routines.
 *
 * It is expected that this library will grow as experience is gained.
 * Routines for writing every permissible solid do not yet exist.
 *
 * Note that routines which are passed point_t or vect_t or mat_t
 * parameters (which are call-by-address) must be VERY careful to
 * leave those parameters unmodified (eg, by scaling), so that the
 * calling routine is not surprised.
 *
 * Return codes of 0 are OK, -1 signal an error.
 *
 * @author Dawn Thomas
 *
 */

#include "pc_solver.h"


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
