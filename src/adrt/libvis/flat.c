#include "flat.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void vis_flat_init(vis_t *vis) {
  vis->work = vis_flat_work;
  vis->free = vis_flat_free;
}


void vis_flat_free(vis_t *vis) {
}


void vis_flat_work(vis_t *vis, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *mesh;

  if((mesh = (common_mesh_t *)tie_work(tie, ray, &id, vis_hit, NULL))) {
    *pixel = mesh->prop->color;
    if(mesh->texture)
      mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);
  }
}
