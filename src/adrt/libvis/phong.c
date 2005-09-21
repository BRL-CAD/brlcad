#include "phong.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void vis_phong_init(vis_t *vis) {
  vis->work = vis_phong_work;
  vis->free = vis_phong_free;
}


void vis_phong_free(vis_t *vis) {
}


void vis_phong_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t		id;
  common_mesh_t		*m;
  TIE_3			vec;
  tfloat		angle;

  if((m = (common_mesh_t*)tie_work(tie, ray, &id, vis_hit, NULL))) {
    *pixel = m->prop->color;
    if(m->texture)
      m->texture->work(m->texture, m, ray, &id, pixel);
  } else {
    return;
  }

  math_vec_sub(vec, ray->pos, id.pos);
  math_vec_unitize(vec);
  math_vec_dot(angle, vec, id.norm);
  math_vec_mul_scalar((*pixel), (*pixel), angle);
}
