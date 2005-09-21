#include "depth.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void vis_depth_init(vis_t *vis) {
  vis->work = vis_depth_work;
  vis->free = vis_depth_free;
}


void vis_depth_free(vis_t *vis) {
}


void vis_depth_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *mesh;

  /* Visualize ray depth, must put ray->depth++ hack into bsp for this to be of any use */
  if((mesh = (common_mesh_t *)tie_work(tie, ray, &id, vis_hit, NULL)))
    pixel->v[0] = 0.0075 * ray->kdtree_depth;
}
