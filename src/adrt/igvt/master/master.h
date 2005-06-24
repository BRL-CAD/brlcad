#ifndef _IGVT_MASTER_H
#define _IGVT_MASTER_H

#include "tie.h"

extern void igvt_master(int port, int obs_port, char *proj, char *geom_file, char *args, char *list, char *exec, int interval);

extern TIE_3 igvt_master_camera_pos;
extern TIE_3 igvt_master_shot_pos;
extern TIE_3 igvt_master_shot_dir;
extern tfloat igvt_master_azim;
extern tfloat igvt_master_elev;

#endif
