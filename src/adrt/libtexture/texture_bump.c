#include "texture_bump.h"
#include <stdlib.h>
#include "umath.h"


void texture_bump_init(texture_t *texture, TIE_3 coef);
void texture_bump_free(texture_t *texture);
void texture_bump_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_bump_init(texture_t *texture, TIE_3 coef) {
  texture_bump_t *sd;

  texture->data = malloc(sizeof(texture_bump_t));
  texture->free = texture_bump_free;
  texture->work = texture_bump_work;

  sd = (texture_bump_t *)texture->data;
  sd->coef = coef;
}


void texture_bump_free(texture_t *texture) {
  free(texture->data);
}


void texture_bump_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_bump_t *sd;
  TIE_3 n;
  tfloat d;


  sd = (texture_bump_t *)texture->data;


  n.v[0] = id->norm.v[0] + sd->coef.v[0]*(2*pixel->v[0]-1.0);
  n.v[1] = id->norm.v[1] + sd->coef.v[1]*(2*pixel->v[1]-1.0);
  n.v[2] = id->norm.v[2] + sd->coef.v[2]*(2*pixel->v[2]-1.0);
  math_vec_unitize(n);

  math_vec_dot(d, n, id->norm);
  if(d < 0)
    math_vec_mul_scalar(n, n, -1.0);
  id->norm = n;
}
