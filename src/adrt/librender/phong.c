#include "phong.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_phong_init(render_t *render) {
  render->work = render_phong_work;
  render->free = render_phong_free;
}


void render_phong_free(render_t *render) {
}


void render_phong_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t		id;
  common_mesh_t		*m;
  TIE_3			vec;
  tfloat		angle;

  if((m = (common_mesh_t*)tie_work(tie, ray, &id, render_hit, NULL))) {
    *pixel = m->prop->color;
    if(m->texture)
      m->texture->work(m->texture, m, ray, &id, pixel);
  } else {
    return;
  }

  MATH_VEC_SUB(vec, ray->pos, id.pos);
  MATH_VEC_UNITIZE(vec);
  MATH_VEC_DOT(angle, vec, id.norm);
  MATH_VEC_MUL_SCALAR((*pixel), (*pixel), angle);
}
