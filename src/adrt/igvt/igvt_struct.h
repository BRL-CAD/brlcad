#ifndef _IGVT_STRUCT_H
#define _IGVT_STRUCT_H


typedef struct igvt_overlay_data_s {
  TIE_3 camera_pos;
  tfloat azimuth;
  tfloat elevation;
  char resolution[12];
  char controller;
} igvt_overlay_data_t;

#endif
