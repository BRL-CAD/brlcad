/*                          S I M P H Y S I C S . C P P
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
/** @file libged/simphysics.cpp
 *
 *
 * Routines related to performing a single step of physics on passed objects
 *
 * 
 * 
 */

#include <iostream>

#include "common.h"
#include "db.h"

/**
 * C++ wrapper for doing physics using bullet : Doesn't have any significant physics code yet
 * 
 */
extern "C" int
single_step_sim(struct bu_vls *result_str, int argc, const char *argv[])
{
        bu_vls_printf(result_str, "%s: In physics\n", argv[0]);
        
        
        return 0;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C++
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
