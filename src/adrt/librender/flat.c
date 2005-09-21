#include "flat.h"
#include "hit.h"
#include "adrt_common.h"
#include <stdio.h>


void render_flat_init(render_t *render) {
  render->work = render_flat_work;
  render->free = render_flat_free;
}


void render_flat_free(render_t *render) {
}


void render_flat_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  tie_id_t id;
  common_mesh_t *mesh;

  if((mesh = (common_mesh_t *)tie_work(tie, ray, &id, render_hit, NULL))) {
    *pixel = mesh->prop->color;
    if(mesh->texture)
      mesh->texture->work(mesh->texture, mesh, ray, &id, pixel);
  }
}
