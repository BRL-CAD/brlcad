/*                   R E N D E R _ U T I L . C
 * BRL-CAD / ADRT
 *
 * Copyright (c) 2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file render_util.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "render_util.h"
#include "umath.h"
#include "adrt_common.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct shotline_s {
  common_mesh_t **mesh_list;
  TIE_3 in;
  TIE_3 out;
  int mesh_num;
} shotline_t;


/* Generate vector list for a spall cone given a reference angle */
void render_util_spall_vec(TIE_3 dir, TFLOAT angle, int vec_num, TIE_3 *vec_list) {
  TIE_3 vec;
  TFLOAT radius, t;
  int i;


  /* Otherwise the cone would be twice the angle */
  angle *= 0.5;

  /* Figure out the rotations of the ray direction */
  vec = dir;
  vec.v[2] = 0;

  radius = sqrt(vec.v[0]*vec.v[0] + vec.v[1]*vec.v[1]);
  vec.v[0] /= radius;
  vec.v[1] /= radius;

  vec.v[0] = vec.v[1] < 0 ? 360.0 - acos(vec.v[0])*MATH_RAD2DEG : acos(vec.v[0])*MATH_RAD2DEG;

  /* triangles to approximate */
  for(i = 0; i < vec_num; i++) {
    t = angle * sin((i * 360 / vec_num) * MATH_DEG2RAD);
    vec_list[i].v[0] = cos((vec.v[0] + t) * MATH_DEG2RAD);
    vec_list[i].v[1] = sin((vec.v[0] + t) * MATH_DEG2RAD);

    t = angle * cos((i * 360 / vec_num) * MATH_DEG2RAD);
    vec_list[i].v[2] = cos(acos(dir.v[2]) + t * MATH_DEG2RAD);
  }
}


static void* shot_hit(tie_ray_t *ray, tie_id_t *id, tie_tri_t *tri, void *ptr) {
  common_triangle_t *t = ((common_triangle_t *)(tri->ptr));
  shotline_t *shotline;
  int i, match;

  shotline = (shotline_t *)ptr;

  match = 0;
  for(i = 0; i < shotline->mesh_num; i++)
    if(t->mesh == shotline->mesh_list[i])
      match = 1;

  if(!match) {
    shotline->mesh_list = (common_mesh_t **)realloc(shotline->mesh_list, sizeof(common_mesh_t *) * (shotline->mesh_num + 1));
    shotline->mesh_list[shotline->mesh_num] = t->mesh;
    if(!shotline->mesh_num)
      shotline->in = id->pos;
    shotline->out = id->pos;
    shotline->mesh_num++;
  }

  return(NULL);
}


void render_util_shotline_list(tie_t *tie, tie_ray_t *ray, void **data, int *dlen) {
  shotline_t shotline;
  tie_id_t id;
  int i, ind;
  unsigned char c;


  shotline.mesh_list = NULL;
  shotline.mesh_num = 0;

  shotline.in.v[0] = 0;
  shotline.in.v[1] = 0;
  shotline.in.v[2] = 0;

  shotline.out.v[0] = 0;
  shotline.out.v[1] = 0;
  shotline.out.v[2] = 0;

  tie_work(tie, ray, &id, shot_hit, &shotline);

  ind = 0;

  *data = (void *)realloc(*data, 6*sizeof(TFLOAT) + sizeof(int));

  /* pack in hit */
  memcpy(&((char *)*data)[ind], &shotline.in.v[0], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);
  memcpy(&((char *)*data)[ind], &shotline.in.v[1], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);
  memcpy(&((char *)*data)[ind], &shotline.in.v[2], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);

  /* pack out hit */
  memcpy(&((char *)*data)[ind], &shotline.out.v[0], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);
  memcpy(&((char *)*data)[ind], &shotline.out.v[1], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);
  memcpy(&((char *)*data)[ind], &shotline.out.v[2], sizeof(TFLOAT));
  ind += sizeof(TFLOAT);

  memcpy(&((char *)*data)[ind], &shotline.mesh_num, sizeof(int));
  ind += sizeof(int);

  for(i = 0; i < shotline.mesh_num; i++) {
    c = strlen(shotline.mesh_list[i]->name) + 1;

    *data = realloc(*data, ind + 1 + c); /* 1 for length */

    /* length */
    memcpy(&((char *)*data)[ind], &c, 1);
    ind += 1;

    /* name */
    memcpy(&((char *)*data)[ind], shotline.mesh_list[i]->name, c);
    ind += c;

/*    printf("hit[%d]: -%s-\n", i, shotline.mesh_list[i]->name); */
  }

  *dlen = ind;
}


void render_util_spall_list(tie_t *tie, tie_ray_t *ray, TFLOAT angle, void **data, int *dlen) {
  shotline_t shotline;
  tie_ray_t sray;
  tie_id_t id;
  int i, ind;
  unsigned char c;
  TIE_3 *vec_list, in, out;


  shotline.mesh_list = NULL;
  shotline.mesh_num = 0;

  MATH_VEC_SET(shotline.in, 0, 0 ,0);
  MATH_VEC_SET(shotline.out, 0, 0 ,0);

  /* Fire the center ray first */
  tie_work(tie, ray, &id, shot_hit, &shotline);
  in = shotline.in;
  out = shotline.out;

  sray.pos = shotline.in;

  /* allocate memory for 32 spall rays */
  vec_list = (TIE_3 *)malloc(32 * sizeof(TIE_3));

  /* Fire the 32 spall rays from the first in-hit at full angle */
  render_util_spall_vec(ray->dir, angle, 32, vec_list);
  for(i = 0; i < 32; i++) {
    sray.dir = vec_list[i];
    tie_work(tie, &sray, &id, shot_hit, &shotline);
  }

  /* Fire the 16 spall rays from the first in-hit at half angle */
  render_util_spall_vec(ray->dir, angle*0.5, 16, vec_list);
  for(i = 0; i < 16; i++) {
    sray.dir = vec_list[i];
    tie_work(tie, &sray, &id, shot_hit, &shotline);
  }

  /* Fire the 12 spall rays from the first in-hit at quarter angle */
  render_util_spall_vec(ray->dir, angle*0.25, 12, vec_list);
  for(i = 0; i < 12; i++) {
    sray.dir = vec_list[i];
    tie_work(tie, &sray, &id, shot_hit, &shotline);
  }

  free(vec_list);

  shotline.in = in;
  shotline.out = out;

  ind = 0;

  *data = (void *)realloc(*data, 6*sizeof(TFLOAT) + sizeof(int));

  /* pack in hit */
  memcpy(&((char *)*data)[ind], &shotline.in, sizeof(TIE_3));
  ind += sizeof(TIE_3);

  /* pack out hit */
  memcpy(&((char *)*data)[ind], &shotline.out, sizeof(TIE_3));
  ind += sizeof(TIE_3);

  memcpy(&((char *)*data)[ind], &shotline.mesh_num, sizeof(int));
  ind += sizeof(int);

  for(i = 0; i < shotline.mesh_num; i++) {
    c = strlen(shotline.mesh_list[i]->name) + 1;

    *data = realloc(*data, ind + c + 2); /* 1 for length, 1 for null char */

    /* length */
    memcpy(&((char *)*data)[ind], &c, 1);
    ind += 1;

    /* name */
    memcpy(&((char *)*data)[ind], shotline.mesh_list[i]->name, c);
    ind += c;

/*    printf("hit[%d]: -%s-\n", i, shotline.mesh_list[i]->name); */
  }

  *dlen = ind;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
