#include "grid.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>

#define GRID 5.0
#define LINE 0.4
void render_grid_init(render_t *render) {
  render->work = render_grid_work;
  render->free = render_grid_free;
}


void render_grid_free(render_t *render) {
}

void render_grid_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *m;
  TIE_3 vec;
  tfloat angle;


  if((m = (common_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
    /* if X or Y lie in the grid paint it white else make it gray */
    if(fabs(GRID*id.pos.v[0] - (int)(GRID*id.pos.v[0])) < 0.2*LINE || fabs(GRID*id.pos.v[1] - (int)(GRID*id.pos.v[1])) < 0.2*LINE) {
      pixel->v[0] = 0.9;
      pixel->v[1] = 0.9;
      pixel->v[2] = 0.9;
    } else {
      pixel->v[0] = 0.1;
      pixel->v[1] = 0.1;
      pixel->v[2] = 0.1;
    }
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
