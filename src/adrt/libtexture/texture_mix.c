#include "texture_mix.h"
#include <stdlib.h>
#include "umath.h"


void texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, tfloat coef);
void texture_mix_free(texture_t *texture);
void texture_mix_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_mix_init(texture_t *texture, texture_t *texture1, texture_t *texture2, tfloat coef) {
  texture_mix_t *td;

  texture->data = malloc(sizeof(texture_mix_t));
  texture->free = texture_mix_free;
  texture->work = texture_mix_work;

  td = (texture_mix_t *)texture->data;
  td->texture1 = texture1;
  td->texture2 = texture2;
  td->coef = coef;
}


void texture_mix_free(texture_t *texture) {
  free(texture->data);
}


void texture_mix_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_mix_t *td;
  TIE_3 t;
  int i;


  td = (texture_mix_t *)texture->data;

  td->texture1->work(td->texture1, mesh, ray, id, pixel);
  td->texture2->work(td->texture2, mesh, ray, id, &t);
  math_vec_mul_scalar((*pixel), (*pixel), td->coef);
  math_vec_mul_scalar(t, t, (1.0 - td->coef));
  math_vec_add((*pixel), (*pixel), t);
}
