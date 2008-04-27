/*                       G L O B A L S . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2008 United States Government as represented by
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
/** @addtogroup librt */
/** @{ */
/** @file globals.c
 *
 * Global variables in LIBRT.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */
/** @} */

#include "raytrace.h"

/**
 * global ray-trace geometry state
 */
struct rt_g rt_g;

/**
 * this array depends on the values of the definitions of the
 * DB5_MINORTYPE_BINU_* in db5.h
 */
const char *binu_types[]={
    NULL,
    NULL,
    "binary(float)",
    "binary(double)",
    "binary(u_8bit_int)",
    "binary(u_16bit_int)",
    "binary(u_32bit_int)",
    "binary(u_64bit_int)",
    NULL,
    NULL,
    NULL,
    NULL,
    "binary(8bit_int)",
    "binary(16bit_int)",
    "binary(32bit_int)",
    "binary(64bit_int)"
};

/**
 * Number of bytes used for each value of DB5HDR_WIDTHCODE_*
 */
const int db5_enc_len[4] = {
    1,
    2,
    4,
    8
};



/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
