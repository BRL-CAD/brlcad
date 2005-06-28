#include "spawl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "adrt_common.h"
#include "hit.h"
#include "umath.h"

#define thickness 0.02
#define TESSELATION 30
#define SPAWL_LEN 20

void* render_spawl_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);
void render_plane(tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);


typedef struct render_spawl_hit_s {
  tie_id_t id;
  common_mesh_t *mesh;
  tfloat plane[4];
  tfloat mod;
} render_spawl_hit_t;


void render_spawl_init(render_t *render, TIE_3 ray_pos, TIE_3 ray_dir, tfloat angle) {
  render_spawl_t *d;
  TIE_3 list[3*TESSELATION], normal, up, vec;
  tfloat plane[4], radius, t;
  int i;

  render->work = render_spawl_work;
  render->free = render_spawl_free;

  render->data = (render_spawl_t *)malloc(sizeof(render_spawl_t));
  d = (render_spawl_t *)render->data;

  d->ray_pos = ray_pos;
  d->ray_dir = ray_dir;

  tie_init(&d->tie, TESSELATION);

  /* Calculate the normal to be used for the plane */
  up.v[0] = 0;
  up.v[1] = 0;
  up.v[2] = 1;

  math_vec_cross(normal, ray_dir, up);
  math_vec_unitize(normal);

  /* Construct the plane */
  d->plane[0] = normal.v[0];
  d->plane[1] = normal.v[1];
  d->plane[2] = normal.v[2];
  math_vec_dot(plane[3], normal, ray_pos); /* up is really new ray_pos */
  d->plane[3] = -plane[3];

  /******************/
  /* The Spawl Cone */
  /******************/

  /* Figure out the rotations of the ray direction */
  vec = ray_dir;
  vec.v[2] = 0;

  radius = sqrt(vec.v[0]*vec.v[0] + vec.v[1]*vec.v[1]);
  vec.v[0] /= radius;
  vec.v[1] /= radius;

  vec.v[0] = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*math_rad2deg : acos(vec.v[0])*math_rad2deg;

  /* triangles to approximate */
  for(i = 0; i < TESSELATION; i++) {
    list[3*i+0] = ray_pos;
    list[3*i+1] = ray_pos;
    list[3*i+2] = ray_pos;

    t = angle * sin((i * 360 / TESSELATION) * math_deg2rad);
    list[3*i+1].v[0] += SPAWL_LEN * cos((vec.v[0] + t) * math_deg2rad);
    list[3*i+1].v[1] += SPAWL_LEN * sin((vec.v[0] + t) * math_deg2rad);

    t = angle * cos((i * 360 / TESSELATION) * math_deg2rad);
    list[3*i+1].v[2] += SPAWL_LEN * cos(acos(ray_dir.v[2]) + t * math_deg2rad);

    t = angle * sin(((i+1) * 360 / TESSELATION) * math_deg2rad);
    list[3*i+2].v[0] += SPAWL_LEN * cos((vec.v[0] + t) * math_deg2rad);
    list[3*i+2].v[1] += SPAWL_LEN * sin((vec.v[0] + t) * math_deg2rad);

    t = angle * cos(((i+1) * 360 / TESSELATION) * math_deg2rad);
    list[3*i+2].v[2] += SPAWL_LEN * cos(acos(ray_dir.v[2]) + t * math_deg2rad);
  }

  tie_push(&d->tie, list, TESSELATION, NULL, 0);  
  tie_prep(&d->tie);
}


void render_spawl_free(render_t *render) {
  free(render->data);
}


static void* render_arrow_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  return(tri);
}


void* render_spawl_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  render_spawl_hit_t *hit = (render_spawl_hit_t *)ptr;

  hit->id = *id;
  hit->mesh = ((common_triangle_t *)(tri->ptr))->mesh;
  return( hit );
}


void render_spawl_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  render_spawl_t *rd;
  render_spawl_hit_t hit;
  TIE_3 vec, color;
  tie_id_t id;
  tfloat t, angle, dot;


  rd = (render_spawl_t *)render->data;

  /* Draw Spawl Cone */
  if(tie_work(&rd->tie, ray, &id, render_arrow_hit, NULL)) {
    pixel->v[0] = 0.5;
    pixel->v[1] = 0.0;
    pixel->v[2] = 0.0;
  }

  /*
  * I don't think this needs to be done for every pixel?
  * Flip plane normal to face us.
  */
  t = ray->pos.v[0]*rd->plane[0] + ray->pos.v[1]*rd->plane[1] + ray->pos.v[2]*rd->plane[2] + rd->plane[3];
  hit.mod = t < 0 ? 1 : -1;


  /*
  * Optimization:
  * First intersect this ray with the plane and fire the ray from there 
  * Plane: Ax + By + Cz + D = 0
  * Ray = O + td
  * t = -(Pn · R0 + D) / (Pn · Rd)
  * 
  */

  t = (rd->plane[0]*ray->pos.v[0] + rd->plane[1]*ray->pos.v[1] + rd->plane[2]*ray->pos.v[2] + rd->plane[3]) /
      (rd->plane[0]*ray->dir.v[0] + rd->plane[1]*ray->dir.v[1] + rd->plane[2]*ray->dir.v[2]);

  /* Ray never intersects plane */
  if(t > 0)
    return;

  ray->pos.v[0] += -t * ray->dir.v[0];
  ray->pos.v[1] += -t * ray->dir.v[1];
  ray->pos.v[2] += -t * ray->dir.v[2];

  hit.plane[0] = rd->plane[0];
  hit.plane[1] = rd->plane[1];
  hit.plane[2] = rd->plane[2];
  hit.plane[3] = rd->plane[3];

  /* Render Geometry */
  if(!tie_work(tie, ray, &id, render_spawl_hit, &hit))
    return;


  /*
  * If the point after the splitting plane is an outhit, fill it in as if it were solid.
  * If the point after the splitting plane is an inhit, then just shade as usual.
  */

  math_vec_dot(dot, ray->dir, hit.id.norm);
  /* flip normal */
  dot = fabs(dot);
  

  if(hit.mesh->flags == 1) {
    color.v[0] = 0.3;
    color.v[1] = 0.3;
    color.v[2] = 0.9;
  } else {
    color.v[0] = 0.8;
    color.v[1] = 0.8;
    color.v[2] = 0.7;
  }

#if 0
  if(dot < 0) {
#endif
    /* Shade using inhit */
    math_vec_mul_scalar(color, color, (dot*0.90));
    math_vec_add((*pixel), (*pixel), color);
#if 0
  } else {
    /* shade solid */
    math_vec_sub(vec, ray->pos, hit.id.pos);
    math_vec_unitize(vec);
    angle = vec.v[0]*hit.mod*-hit.plane[0] + vec.v[1]*-hit.mod*hit.plane[1] + vec.v[2]*-hit.mod*hit.plane[2];
    math_vec_mul_scalar((*pixel), color, (angle*0.90));
  }
#endif

  pixel->v[0] += 0.1;
  pixel->v[1] += 0.1;
  pixel->v[2] += 0.1;
}
