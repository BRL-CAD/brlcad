/*                       G L O B A L S . C
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
/** @file libbn/globals.c
 *
 * Global variables in LIBBN.
 *
 * New global variables are discouraged and refactoring in ways that
 * eliminates existing global variables without reducing functionality
 * is always encouraged.
 *
 */

#include "common.h"

#include <math.h>

#include "vmath.h"
#include "bn.h"


/* see rand.c for random constant globals */
extern const float bn_rand_table[];
extern double bn_sin_scale;
extern const float bn_sin_table[];
extern int bn_randhalftabsize;
extern float bn_rand_halftab[];
extern float bn_rand_poison_[];

/* see vectfont.c for vector font constant globals */
extern int *tp_cindex[];
extern int tp_ctable[];


/**
 * pi
 */
const fastf_t bn_pi = M_PI;
/**
 * pi*2
 */
const fastf_t bn_twopi = 2.0 * M_PI;

/**
 * pi/2
 */
const fastf_t bn_halfpi = M_PI_2;
/**
 * pi/4
 */
const fastf_t bn_quarterpi = M_PI_4;

/**
 * 1/pi
 */
const fastf_t bn_invpi = M_1_PI;
/**
 * 1/(pi*2)
 */
const fastf_t bn_inv2pi = 1.0 / (2.0 * M_PI);
/**
 * 1/(pi*4)
 */
const fastf_t bn_inv4pi = 1.0 / (4.0 * M_PI);

/**
 * 1.0/255.0
 */
const fastf_t bn_inv255 = 1.0 / 255.0;

/**
 * (pi*2)/360
 */
const fastf_t bn_degtorad = DEG2RAD;
/**
 * 360/(pi*2)
 */
const fastf_t bn_radtodeg = RAD2DEG;


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
