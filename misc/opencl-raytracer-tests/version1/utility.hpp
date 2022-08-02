/*                     U T I L I T Y . H P P
 * BRL-CAD
 *
 * Copyright (c) 2020-2022 United States Government as represented by
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
/** @file utility.hpp
 *
 * Brief description
 *
 */

#ifndef __UTILITY_H__
#define __UTILITY_H__


#ifdef DEBUG_BUILD
#include <iostream>
#define D(x) do {std::cerr << x << std::endl;} while(0)
#else
#define D(x) do {} while(0)
#endif


#endif


