#ifndef _RENDER_INTERNAL_H
#define _RENDER_INTERNAL_H

#include "tie.h"

#define RENDER_METHOD_FLAT	0
#define RENDER_METHOD_GRID	1
#define RENDER_METHOD_NORMAL	2
#define RENDER_METHOD_PHONG	3
#define RENDER_METHOD_PATH	4
#define RENDER_METHOD_PLANE	5
#define RENDER_METHOD_COMPONENT	6
#define RENDER_METHOD_SPALL	7
#define RENDER_METHOD_DEPTH	8


#define RENDER_MAX_DEPTH	24


struct render_s;
typedef void render_work_t(struct render_s *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);
typedef void render_free_t(struct render_s *render);

typedef struct render_s {
  render_work_t *work;
  render_free_t *free;
  void *data;
} render_t;

#endif
