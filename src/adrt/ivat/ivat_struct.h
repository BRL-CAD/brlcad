#ifndef _IVAT_STRUCT_H
#define _IVAT_STRUCT_H


typedef struct ivat_overlay_data_s {
  TIE_3 camera_pos;
  tfloat azimuth;
  tfloat elevation;
  char resolution[12];
  char controller;
} ivat_overlay_data_t;

#endif
