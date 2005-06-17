#ifndef _TEXTURE_H
#define _TEXTURE_H


#include "tie.h"
#include "adrt_common.h"


struct texture_s;
struct mesh_s;
typedef void texture_init_t(struct texture_s *texture);
typedef void texture_free_t(struct texture_s *texture);
typedef void texture_work_t(struct texture_s *texture, struct mesh_s *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


typedef struct texture_s {
  texture_free_t *free;
  texture_work_t *work;
  void *data;
} texture_t;


#endif
