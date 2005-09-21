#include "depth.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_depth_init(render_t *render) {
  render->work = render_depth_work;
  render->free = render_depth_free;
}


void render_depth_free(render_t *render) {
}


void render_depth_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *mesh;

  /* Visualize ray depth, must put ray->depth++ hack into bsp for this to be of any use */
  if((mesh = (common_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL)))
    pixel->v[0] = 0.0075 * ray->kdtree_depth;
}
