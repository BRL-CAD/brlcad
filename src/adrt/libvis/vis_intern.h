#ifndef _VIS_INTERN_H
#define _VIS_INTERN_H

#include "tie.h"

#define VM_FLAT		0
#define VM_GRID		1
#define VM_NORMAL	2
#define VM_PHONG	3
#define VM_PATH		4
#define VM_PLANE	5
#define VM_COMPONENT	6
#define VM_SPALL	7
#define VM_DEPTH	8


#define VIS_MAX_DEPTH	24


struct vis_s;
typedef void vis_work_t(struct vis_s *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);
typedef void vis_free_t(struct vis_s *vis);

typedef struct vis_s {
  vis_work_t *work;
  vis_free_t *free;
  void *data;
} vis_t;

#endif
