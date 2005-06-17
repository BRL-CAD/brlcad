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

  math_vec_sub(vec, ray->pos, id.pos);
  math_vec_unitize(vec);
  math_vec_dot(angle, vec, id.norm);
  math_vec_mul_scalar((*pixel), (*pixel), (angle*0.9));

  pixel->v[0] += 0.1;
  pixel->v[1] += 0.1;
  pixel->v[2] += 0.1;
}
