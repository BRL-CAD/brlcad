#ifndef _ISST_STRUCT_H
#define _ISST_STRUCT_H


typedef struct isst_overlay_data_s {
  TIE_3 camera_pos;
  tfloat azimuth;
  tfloat elevation;
  char resolution[12];
  char controller;
} isst_overlay_data_t;

#endif
