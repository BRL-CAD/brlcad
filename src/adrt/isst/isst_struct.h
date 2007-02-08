/*                   I S S T _ S T R U C T . H
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007 United States Government as represented by
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
/** @file isst_struct.h
 *
 * Author -
 *   Justin Shumaker
 *
 */

#ifndef _ISST_STRUCT_H
#define _ISST_STRUCT_H


#include <inttypes.h>


typedef struct isst_overlay_data_s {
  TIE_3 camera_position;
  TFLOAT camera_azimuth;
  TFLOAT camera_elevation;
  TIE_3 in_hit;
  TIE_3 out_hit;
  char resolution[12];
  char controller;
  short compute_nodes;
  TFLOAT scale;
} isst_overlay_data_t;


/*
* isst_event_t exists because the SDL_Event structure is not
* the same size on 32-bit and 64-bit machines due to a pointer
* in the syswm struct in the SDL_Event struct.
*/
typedef struct isst_event_s {
  uint8_t type;
  uint16_t keysym;
  uint8_t button;
  uint8_t motion_state;
  int16_t motion_xrel;
  int16_t motion_yrel;
} isst_event_t;

#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
