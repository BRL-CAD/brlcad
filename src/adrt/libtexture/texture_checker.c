#include "texture_checker.h"
#include <stdlib.h>
#include "umath.h"


void texture_checker_init(texture_t *texture, int tile);
void texture_checker_free(texture_t *texture);
void texture_checker_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel);


void texture_checker_init(texture_t *texture, int tile) {
  texture_checker_t   *td;

  texture->data = malloc(sizeof(texture_checker_t));
  texture->free = texture_checker_free;
  texture->work = texture_checker_work;

  td = (texture_checker_t *)texture->data;
  td->tile = tile;
}


void texture_checker_free(texture_t *texture) {
  free(texture->data);
}


void texture_checker_work(texture_t *texture, common_mesh_t *mesh, tie_ray_t *ray, tie_id_t *id, TIE_3 *pixel) {
  texture_checker_t	*td;
  TIE_3			pt;
  int			u, v;


  td = (texture_checker_t *)texture->data;

  /* Transform the Point */
  math_vec_transform(pt, id->pos, mesh->matinv);
  
  if(pt.v[0]+TIE_PREC > mesh->max.v[0]) pt.v[0] = mesh->max.v[0];
  if(pt.v[1]+TIE_PREC > mesh->max.v[1]) pt.v[1] = mesh->max.v[1];
  u = mesh->max.v[0] - mesh->min.v[0] > 0 ? (int)((pt.v[0] - mesh->min.v[0]) / ((mesh->max.v[0] - mesh->min.v[0])/td->tile))%2 : 0;
  v = mesh->max.v[1] - mesh->min.v[1] > 0 ? (int)((pt.v[1] - mesh->min.v[1]) / ((mesh->max.v[1] - mesh->min.v[1])/td->tile))%2 : 0;

  pixel->v[0] = pixel->v[1] = pixel->v[2] = u ^ v ? 1.0 : 0.0;
}
