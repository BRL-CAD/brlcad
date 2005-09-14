#ifndef _ISST_STRUCT_H
#define _ISST_STRUCT_H


#include <inttypes.h>


typedef struct isst_overlay_data_s {
  TIE_3 camera_position;
  tfloat camera_azimuth;
  tfloat camera_elevation;
  TIE_3 in_hit;
  TIE_3 out_hit;
  char resolution[12];
  char controller;
  short compute_nodes;
  tfloat scale;
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
