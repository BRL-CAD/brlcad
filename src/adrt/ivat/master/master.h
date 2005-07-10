#ifndef _IVAT_MASTER_H
#define _IVAT_MASTER_H

#include "tie.h"

extern void ivat_master(int port, int obs_port, char *proj, char *list, char *exec, char *comp_host);

extern TIE_3 ivat_master_camera_pos;
extern TIE_3 ivat_master_shot_pos;
extern TIE_3 ivat_master_shot_dir;
extern tfloat ivat_master_azim;
extern tfloat ivat_master_elev;
extern tfloat ivat_master_spall_angle;

#endif
