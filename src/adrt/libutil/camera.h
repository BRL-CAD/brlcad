#ifndef _UTIL_CAMERA_H
#define _UTIL_CAMERA_H


#include "common.h"
#include "tienet.h"
#include "cdb.h"


#define	UTIL_CAMERA_DOF_SAMPLES	13


typedef struct util_camera_view_s {
  TIE_3 step_x;
  TIE_3 step_y;
  TIE_3 pos;
  TIE_3 top_l;
} util_camera_view_t;


typedef struct util_camera_s {
  TIE_3 pos;
  TIE_3 focus;
  tfloat tilt;
  tfloat fov;
  tfloat dof;
  int thread_num;
  int view_num;
  util_camera_view_t *view_list;
} util_camera_t;


typedef struct util_camera_thread_data_s {
  common_db_t *db;
  util_camera_t *camera;
  tie_t *tie;
  common_work_t work;
  void *res_buf;
  unsigned char *scan_map;
  pthread_mutex_t mut;
} util_camera_thread_data_t;


void util_camera_init(util_camera_t *camera, int threads);
void util_camera_free(util_camera_t *camera);
void util_camera_prep(util_camera_t *camera, common_db_t *db);
void util_camera_render(util_camera_t *camera, common_db_t *db, tie_t *tie, void *data, int size, void **res_buf, int *res_len);

#endif
