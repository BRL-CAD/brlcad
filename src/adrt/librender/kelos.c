#include "kelos.h"
#include <stdio.h>
#include <stdlib.h>
#include "adrt_common.h"

void* render_kelos_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr);
void render_kelos(tie_t *tie, tie_ray_t *ray, TIE_3 *pixel);

typedef struct render_kelos_hit_s {
  tie_id_t id;
  common_mesh_t *mesh;
  struct render_kelos_hit_s *next;
}  render_kelos_hit_t;


typedef struct render_kelos_seg_s {
  int out;
  tie_id_t id;
  tfloat thickness;
  common_mesh_t *mesh;
} render_kelos_seg_t;


void render_kelos_init(render_t *render) {
  render->work = render_kelos_work;
  render->free = render_kelos_free;
}


void render_kelos_free(render_t *render) {
}


void* render_kelos_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  common_triangle_t	*t = ((common_triangle_t *)(tri->ptr));
  render_kelos_hit_t	**list, *prev;


  list = (render_kelos_hit_t **)ptr;

  /* Append to STACK */
  if(*list) {
    prev = (*list);
    (*list) = (render_kelos_hit_t *)malloc(sizeof(render_kelos_hit_t));
    (*list)->next = prev->next;
    prev->next = (*list) ;
  } else {
    (*list) = (render_kelos_hit_t *)malloc(sizeof(render_kelos_hit_t));
    (*list)->next = (*list);
  }

  (*list)->id = *id;
  (*list)->mesh  = t->mesh;

  return( NULL );
}



static void render_overlap(tie_t *tie, tie_ray_t *ray, render_kelos_seg_t *seg_list, int *seg_ind, render_kelos_seg_t *ins_list, int *ins_ind) {
  render_kelos_hit_t	*hit_list, *node, *free_node;
  tie_id_t		id;
  tfloat		dot;
  int			i, found, start;


  /* Build hit list going through all geometry */
  hit_list = NULL;
  tie_work(tie, ray, &id, render_kelos_hit, &hit_list);

  *seg_ind = 0;
  *ins_ind = 0;
  start = 0;

  if(!hit_list)
    return;

  /* set node to head and make list non-cyclical */
  node = hit_list->next;
  hit_list->next = NULL;

  while(node) {
    math_vec_dot(dot, ray->dir, node->id.norm);

    if(dot < 0) { /* Entry Point */
      seg_list[*seg_ind].out = 0;
      seg_list[*seg_ind].mesh = node->mesh;
      seg_list[*seg_ind].id = node->id;
      seg_list[*seg_ind].thickness = 0;
      (*seg_ind)++;
    } else { /* Exit Point */
      /* Find matching exit */
      found = 0;
      for(i = start; i < *seg_ind && !found; i++) {
        /* Find the first matching material type with no set out point yet */
        if(!seg_list[i].out && seg_list[i].mesh == node->mesh) {
          tfloat	dx, dy, dz;

          seg_list[i].out = 1; /* this is now a complete segment */
          dx = node->id.pos.v[0] - seg_list[i].id.pos.v[0];
          dy = node->id.pos.v[1] - seg_list[i].id.pos.v[1];
          dz = node->id.pos.v[2] - seg_list[i].id.pos.v[2];
          seg_list[i].thickness = sqrt(dx*dx + dy*dy + dz*dz);
          found = 1;

          if(i == start)
            start++;
        }
      }

      /*
      * An exit was found with no matching entry point.
      * This implies the ray started inside this mesh,
      * push it onto the ins_list.
      */
      if(!found) {
        /*
        * Check to see that if this exit with not matching entry is the
        * result of hitting a corner, check the barycentric coordinates.
        */
        if(node->id.alpha > 50000.0*TIE_PREC && node->id.beta > 50000.0*TIE_PREC && (1.0 - (node->id.alpha + node->id.beta)) > 50000.0*TIE_PREC) {

	
        tfloat	dx, dy, dz;

        ins_list[*ins_ind].out = 1;
        ins_list[*ins_ind].mesh = node->mesh;
        ins_list[*ins_ind].id.pos = ray->pos;

        dx = node->id.pos.v[0] - ray->pos.v[0];
        dy = node->id.pos.v[1] - ray->pos.v[1];
        dz = node->id.pos.v[2] - ray->pos.v[2];

        ins_list[*ins_ind].thickness = sqrt(dx*dx + dy*dy + dz*dz);
        (*ins_ind)++;


        }
      }
    }

    free_node = node;
    node = node->next;
    free(free_node);
  }
}

static int render_kelos_penetrate(tfloat density, tfloat thickness, tfloat *cap) {
#if 1
  tfloat	gamma, theff;

  gamma = density * 0.1273;
  theff = gamma * thickness;

  if(*cap < theff) {
    /* terminate */
    return(1);
  } else {
    *cap -= theff;
  }

#else
  *cap += density*thickness*0.1;
#endif

  return(0);
}


void render_kelos_work(render_t *render, tie_t *tie, tie_ray_t *ray, TIE_3 *pixel) {
  render_kelos_seg_t	seg_list[64], ins_list[64];
  TIE_3			vec;
  tfloat		cap, angle;
  int			seg_ind, ins_ind, i, stop;

#if 1
  cap = 800*2.06*exp(-(1292*1292) / (425*425));

  render_overlap(tie, ray, seg_list, &seg_ind, ins_list, &ins_ind);

  stop = 0;

  /* first process the ins_list */
  for(i = 0; i < ins_ind && !stop; i++) {
    if(render_kelos_penetrate(ins_list[i].mesh->prop->density, ins_list[i].thickness, &cap)) {
      *pixel = ins_list[i].mesh->prop->color;
      stop = 1;
    }
  }

  /* second process the seg_list */
  for(i = 0; i < seg_ind && !stop; i++) {
    if(render_kelos_penetrate(seg_list[i].mesh->prop->density, seg_list[i].thickness, &cap)) {

      *pixel = seg_list[i].mesh->prop->color;

  math_vec_sub(vec, ray->pos, seg_list[i].id.pos);
  math_vec_unitize(vec);
  math_vec_dot(angle, vec, seg_list[i].id.norm);
  math_vec_mul_scalar((*pixel), (*pixel), (angle*0.85));

  pixel->v[0] += 0.15;
  pixel->v[1] += 0.15;
  pixel->v[2] += 0.15;


      stop = 1;
    }
  }
#else
  cap = 0;

  render_overlap(tie, ray, seg_list, &seg_ind, ins_list, &ins_ind);

  stop = 0;

  /* first process the ins_list */
  for(i = 0; i < ins_ind && !stop; i++) {
    render_kelos_penetrate(ins_list[i].mesh->prop->density, ins_list[i].thickness, &cap);
  }

  /* second process the seg_list */

  for(i = 0; i < seg_ind && !stop; i++) {
    render_kelos_penetrate(seg_list[i].mesh->prop->density, seg_list[i].thickness, &cap);
  }


  pixel->v[0] += cap;
  pixel->v[1] += cap;
  pixel->v[2] += cap;
#endif

  return(0);
}
