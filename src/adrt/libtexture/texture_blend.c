#include "texture_blend.h"
#include <stdlib.h>


void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2);
void texture_blend_free(texture_t *texture);
void texture_blend_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_blend_init(texture_t *texture, TIE_3 color1, TIE_3 color2) {
  texture_blend_t *sd;

  texture->data = malloc(sizeof(texture_blend_t));
  texture->free = texture_blend_free;
  texture->work = texture_blend_work;

  sd = (texture_blend_t *)texture->data;
  sd->color1 = color1;
  sd->color2 = color2;
}


void texture_blend_free(texture_t *texture) {
  free(texture->data);
}


void texture_blend_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_blend_t *sd;
  tfloat coef;

  sd = (texture_blend_t *)texture->data;

  coef = pixel->v[0];
  pixel->v[0] = (1.0 - coef)*sd->color1.v[0] + coef*sd->color2.v[0];
  pixel->v[1] = (1.0 - coef)*sd->color1.v[1] + coef*sd->color2.v[1];
  pixel->v[2] = (1.0 - coef)*sd->color1.v[2] + coef*sd->color2.v[2];
}
