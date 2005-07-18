#ifndef _ISST_MASTER_H
#define _ISST_MASTER_H

#include "tie.h"

extern void isst_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host);

extern TIE_3 isst_master_camera_pos;
extern TIE_3 isst_master_shot_pos;
extern TIE_3 isst_master_shot_dir;
extern tfloat isst_master_origin_azimuth;
extern tfloat isst_master_origin_elevation;
extern tfloat isst_master_camera_azimuth;
extern tfloat isst_master_camera_elevation;
extern tfloat isst_master_spall_angle;

#endif
