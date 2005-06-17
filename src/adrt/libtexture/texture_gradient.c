#include "texture_gradient.h"
#include <stdlib.h>
#include "umath.h"


void texture_gradient_init(texture_t *texture, int axis);
void texture_gradient_free(texture_t *texture);
void texture_gradient_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_gradient_init(texture_t *texture, int axis) {
  texture_gradient_t *td;

  texture->data = malloc(sizeof(texture_gradient_t));
  texture->free = texture_gradient_free;
  texture->work = texture_gradient_work;

  td = (texture_gradient_t *)texture->data;
  td->axis = axis;
}


void texture_gradient_free(texture_t *texture) {
  free(texture->data);
}


void texture_gradient_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_gradient_t *td;
  TIE_3 pt;


  td = (texture_gradient_t *)texture->data;

  /* Transform the Point */
  math_vec_transform(pt, id->pos, mesh->matinv);

  if(td->axis == 1) {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[1] - mesh->min.v[1] > TIE_PREC ? (pt.v[1] - mesh->min.v[1]) / (mesh->max.v[1] - mesh->min.v[1]) : 0.0;
  } else if(td->axis == 2) {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[2] - mesh->min.v[2] > TIE_PREC ? (pt.v[2] - mesh->min.v[2]) / (mesh->max.v[2] - mesh->min.v[1]) : 0.0;
  } else {
    pixel->v[0] = pixel->v[1] = pixel->v[2] = mesh->max.v[0] - mesh->min.v[0] > TIE_PREC ? (pt.v[0] - mesh->min.v[0]) / (mesh->max.v[0] - mesh->min.v[1]) : 0.0;
  }
}
