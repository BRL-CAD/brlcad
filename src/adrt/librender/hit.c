#include "hit.h"
#include "adrt_common.h"
#include "umath.h"

void* render_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);

void* render_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  common_triangle_t *t = ((common_triangle_t *)(tri->ptr));

  /* Flip normal to face ray origin (via dot product check) */
  if(ray->dir.v[0] * id->norm.v[0] + ray->dir.v[1] * id->norm.v[1] + ray->dir.v[2] * id->norm.v[2] > 0)
    MATH_VEC_MUL_SCALAR(id->norm, id->norm, -1.0);

  if(t->normals) {
    tfloat	dot;
    TIE_3	norm;

    norm.v[0] = (1.0 - (id->alpha + id->beta)) * t->normals[0] + id->alpha * t->normals[3] + id->beta * t->normals[6];
    norm.v[1] = (1.0 - (id->alpha + id->beta)) * t->normals[1] + id->alpha * t->normals[4] + id->beta * t->normals[7];
    norm.v[2] = (1.0 - (id->alpha + id->beta)) * t->normals[2] + id->alpha * t->normals[5] + id->beta * t->normals[8];

    MATH_VEC_DOT(dot, norm, id->norm);
    if(dot < 0)
      MATH_VEC_MUL_SCALAR(norm, norm, -1.0);
    id->norm = norm;
  }

  return( t->mesh );
}
