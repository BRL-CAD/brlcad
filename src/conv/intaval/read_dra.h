/*                      R E A D _ D R A . H
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file read_dra.h
 *
 * INTAVAL Target Geometry File to BRL-CAD converter:
 * INTAVAL data reading function declaration
 *
 *  Origin -
 *	TNO (Netherlands)
 *	IABG mbH (Germany)
 */

#ifndef CONV_INTAVAL_READ_DRA_H
#define CONV_INTAVAL_READ_DRA_H

#include "common.h"

#include <istream>

#include "wdb.h"

void conv
(
    std::istream& is,
    rt_wdb*       wdbp
);


#endif /* CONV_INTAVAL_READ_DRA_H */
