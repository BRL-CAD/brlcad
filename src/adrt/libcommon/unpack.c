/*                     U N P A C K . C
 * BRL-CAD
 *
 * Copyright (C) 2002-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file unpack.c
 *                     U N P A C K . C
 *
 *  Common Library - Parsing and Packing Header
 *
 *  Author -
 *      Justin L. Shumaker
 *
 *  Source -
 *      The U. S. Army Research Laboratory
 *      Aberdeen Proving Ground, Maryland  21005-5068  USA
 *
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unpack.h"
#include "tienet.h"
#include "umath.h"


int app_ind;

int prop_num;
common_unpack_prop_node_t *prop_list;

int texture_num;
common_unpack_texture_node_t *texture_list;

common_prop_t common_unpack_def_prop;


#define common_unpack_read(_src, _ind, _dest, _size, _flip) { \
	memcpy(_dest, &((char*)_src)[_ind], _size); \
	if(_flip) tienet_flip(&((char*)_src)[_ind], _dest, _size); \
	_ind += _size; }


void	common_unpack(common_db_t *db, tie_t *tie, util_camera_t *camera, int mask, void *app_data, int app_size);
void	common_unpack_free(common_db_t *db);
void	common_unpack_camera(util_camera_t *camera, void *app_data, int size);
void	common_unpack_env(common_db_t *db, void *app_data, int size);
void	common_unpack_prop(void *app_data, int size);
void	common_unpack_texture(void *app_data, int size);
void	common_unpack_mesh(common_db_t *db, void *app_data, int size, tie_t *tie);
void	common_unpack_prop_lookup(char *name, common_prop_t **prop);
void	common_unpack_texture_lookup(char *name, texture_t **texture);


void common_unpack(common_db_t *db, tie_t *tie, util_camera_t *camera, int mask, void *app_data, int app_size) {
  int size, marker;
  short ver, block;


  app_ind = 0;

  prop_num = 0;
  prop_list = NULL;
  texture_num = 0;
  texture_list = NULL;
  db->mesh_num = 0;
  db->mesh_list = NULL;

  /* Set default prop values */
  common_unpack_def_prop.color.v[0] = 0.8;
  common_unpack_def_prop.color.v[1] = 0.8;
  common_unpack_def_prop.color.v[2] = 0.8;
  common_unpack_def_prop.gloss = 0.2;
  common_unpack_def_prop.density = 0.5;
  common_unpack_def_prop.emission = 0.0;
  common_unpack_def_prop.ior = 1.0;

  /* COMMON PROJECT DATA VERSION */
  common_unpack_read(app_data, app_ind, &ver, sizeof(short), tienet_endian);

  while(app_ind != app_size) {
    common_unpack_read(app_data, app_ind, &block, sizeof(short), tienet_endian);
    common_unpack_read(app_data, app_ind, &size, sizeof(int), tienet_endian);
    marker = app_ind;
/*    printf("block: %d, %d\n", block, size); */
    switch(block) {
      case COMMON_PACK_CAMERA:
        if(mask & COMMON_PACK_ALL || mask & COMMON_PACK_CAMERA)
          common_unpack_camera(camera, app_data, size);
        break;

      case COMMON_PACK_ENV:
        if(mask & COMMON_PACK_ALL || mask & COMMON_PACK_ENV)
          common_unpack_env(db, app_data, size);
        break;

      case COMMON_PACK_PROP:
        if(mask & COMMON_PACK_ALL || mask & COMMON_PACK_PROP)
          common_unpack_prop(app_data, size);
         break;

      case COMMON_PACK_TEXTURE:
        if(mask & COMMON_PACK_ALL || mask & COMMON_PACK_TEXTURE)
          common_unpack_texture(app_data, size);
        break;

      case COMMON_PACK_MESH:
        if(mask & COMMON_PACK_ALL || mask & COMMON_PACK_MESH)
          common_unpack_mesh(db, app_data, size, tie);
        break;
    }
    app_ind = marker + size;
  }
}


void common_unpack_free(common_db_t *db) {
  int i;

#if 0
  /* Free texture data */
  for(i = 0; i < texture_num; i++)
    texture_list[i].texture->free(texture_list[i].texture);
  free(texture_list);

  /* Free mesh data */
  for(i = 0; i < mesh_num; i++)
    free(mesh_list[i]);
  free(mesh_list);

  /* Free triangle data */
  for(i = 0; i < tri_num; i++)
    free(tri_list[i]);
  free(tri_list);
#endif
}


void common_unpack_camera(util_camera_t *camera, void *app_data, int size) {
  TIE_3		pos, focus;
  tfloat	tilt, fov, dof;


  /* POSITION */
  common_unpack_read(app_data, app_ind, &pos.v[0], sizeof(tfloat), tienet_endian);
  common_unpack_read(app_data, app_ind, &pos.v[1], sizeof(tfloat), tienet_endian);
  common_unpack_read(app_data, app_ind, &pos.v[2], sizeof(tfloat), tienet_endian);

  /* FOCUS */
  common_unpack_read(app_data, app_ind, &focus.v[0], sizeof(tfloat), tienet_endian);
  common_unpack_read(app_data, app_ind, &focus.v[1], sizeof(tfloat), tienet_endian);
  common_unpack_read(app_data, app_ind, &focus.v[2], sizeof(tfloat), tienet_endian);

  /* TILT */
  common_unpack_read(app_data, app_ind, &tilt, sizeof(tfloat), tienet_endian);

  /* FIELD OF VIEW */
  common_unpack_read(app_data, app_ind, &fov, sizeof(tfloat), tienet_endian);

  /* DEPTH OF FIELD */
  common_unpack_read(app_data, app_ind, &dof, sizeof(tfloat), tienet_endian);

  camera->pos = pos;
  camera->focus = focus;
  camera->tilt = tilt;
  camera->fov = fov;
  camera->dof = dof;
}


void common_unpack_env(common_db_t *db, void *app_data, int size) {
  int		start;
  short		block;


  start = app_ind;
  do {
    common_unpack_read(app_data, app_ind, &block, sizeof(short), tienet_endian);
    switch(block) {
      case COMMON_PACK_ENV_RM:
        {
          common_unpack_read(app_data, app_ind, &db->env.rm, sizeof(int), tienet_endian);
          switch(db->env.rm) {
            case RENDER_METHOD_FLAT:
              render_flat_init(&db->env.render);
              break;

            case RENDER_METHOD_NORMAL:
              render_normal_init(&db->env.render);
              break;

            case RENDER_METHOD_PHONG:
              render_phong_init(&db->env.render);
              break;

            case RENDER_METHOD_PATH:
              {
                int samples;
                common_unpack_read(app_data, app_ind, &samples, sizeof(int), tienet_endian);
                render_path_init(&db->env.render, samples);
              }
              break;

            case RENDER_METHOD_KELOS:
              render_kelos_init(&db->env.render);
              break;

            case RENDER_METHOD_PLANE:
              {
                TIE_3 ray_pos, ray_dir;

                common_unpack_read(app_data, app_ind, &ray_pos.v[0], sizeof(tfloat), tienet_endian);
                common_unpack_read(app_data, app_ind, &ray_pos.v[1], sizeof(tfloat), tienet_endian);
                common_unpack_read(app_data, app_ind, &ray_pos.v[2], sizeof(tfloat), tienet_endian);

                common_unpack_read(app_data, app_ind, &ray_dir.v[0], sizeof(tfloat), tienet_endian);
                common_unpack_read(app_data, app_ind, &ray_dir.v[1], sizeof(tfloat), tienet_endian);
                common_unpack_read(app_data, app_ind, &ray_dir.v[2], sizeof(tfloat), tienet_endian);

                render_plane_init(&db->env.render, ray_pos, ray_dir);
              }
              break;

            default:
              break;
          }
        }
        break;

      case COMMON_PACK_ENV_IMAGESIZE:
        {
          common_unpack_read(app_data, app_ind, &db->env.img_w, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &db->env.img_h, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &db->env.img_hs, sizeof(int), tienet_endian);
        }
        break;

      default:
        break;
    }
  } while(app_ind - start < size);
}


void common_unpack_prop(void *app_data, int size) {
  int			start;
  char			c;


  start = app_ind;
  do {
    prop_num++;
    prop_list = (common_unpack_prop_node_t*)realloc(prop_list, sizeof(common_unpack_prop_node_t)*prop_num);

    /* property name */
    common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
    common_unpack_read(app_data, app_ind, prop_list[prop_num-1].name, c, 0);

    /* property data */
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.color.v[0], sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.color.v[1], sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.color.v[2], sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.density, sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.gloss, sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.emission, sizeof(tfloat), tienet_endian);
    common_unpack_read(app_data, app_ind, &prop_list[prop_num-1].prop.ior, sizeof(tfloat), tienet_endian);
  } while(app_ind - start < size);
}


void common_unpack_texture(void *app_data, int size) {
  int start;
  short block;
  char c;
  texture_t *stack = NULL, *texture = NULL;


  start = app_ind;
  do {
    common_unpack_read(app_data, app_ind, &block, sizeof(short), tienet_endian);
    switch(block) {
      case TEXTURE_STACK:
        texture_num++;
        texture_list = (common_unpack_texture_node_t *)realloc(texture_list, sizeof(common_unpack_texture_node_t)*texture_num);

        texture_list[texture_num-1].texture = stack = (texture_t *)malloc(sizeof(texture_t));
        texture_stack_init(stack);

        common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
        common_unpack_read(app_data, app_ind, texture_list[texture_num-1].name, c, 0);
        break;

      case TEXTURE_MIX:
        {
          texture_t *texture1, *texture2;
          char s1[64], s2[64];
          tfloat coef;

          texture_num++;
          texture_list = (common_unpack_texture_node_t*)realloc(texture_list, sizeof(common_unpack_texture_node_t)*texture_num);
          texture_list[texture_num-1].texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
          common_unpack_read(app_data, app_ind, texture_list[texture_num-1].name, c, 0);
          common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
          common_unpack_read(app_data, app_ind, s1, c, 0);
          common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
          common_unpack_read(app_data, app_ind, s2, c, 0);
          common_unpack_read(app_data, app_ind, &coef, sizeof(tfloat), tienet_endian);
          common_unpack_texture_lookup(s1, &texture1);
          common_unpack_texture_lookup(s2, &texture2);
          texture_mix_init(texture_list[texture_num-1].texture, texture1, texture2, coef);
        }
        break;

      case TEXTURE_BLEND:
        {
          TIE_3 color1, color2;

          texture = (texture_t*)malloc(sizeof(texture_t));
          /* COLOR 1 */
          common_unpack_read(app_data, app_ind, &color1.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color1.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color1.v[2], sizeof(tfloat), tienet_endian);
          /* COLOR 2 */
          common_unpack_read(app_data, app_ind, &color2.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color2.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color2.v[2], sizeof(tfloat), tienet_endian);
          texture_blend_init(texture, color1, color2);
          texture_stack_push(stack, texture);
        }
        break;

      case TEXTURE_BUMP:
        {
          TIE_3 coef;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &coef.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &coef.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &coef.v[2], sizeof(tfloat), tienet_endian);
          texture_bump_init(texture, coef);
          texture_stack_push(stack, texture);
        }
        break;

      case TEXTURE_CHECKER:
        {
          int tile;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &tile, sizeof(int), tienet_endian);
          texture_checker_init(texture, tile);
          texture_stack_push(stack, texture);
        }
        break;

      case TEXTURE_CAMO:
        {
          tfloat size;
          int octaves, absolute;
          TIE_3 color1, color2, color3;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &size, sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &octaves, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &absolute, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &color1.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color1.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color1.v[2], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color2.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color2.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color2.v[2], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color3.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color3.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &color3.v[2], sizeof(tfloat), tienet_endian);
          texture_camo_init(texture, size, octaves, absolute, color1, color2, color3);
          texture_stack_push(stack, texture);
        }
        break;

      case TEXTURE_CLOUDS:
        {
          tfloat size;
          int octaves, absolute;
          TIE_3 scale, translate;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &size, sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &octaves, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &absolute, sizeof(int), tienet_endian);
          common_unpack_read(app_data, app_ind, &scale.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &scale.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &scale.v[2], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &translate.v[0], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &translate.v[1], sizeof(tfloat), tienet_endian);
          common_unpack_read(app_data, app_ind, &translate.v[2], sizeof(tfloat), tienet_endian);
          texture_clouds_init(texture, size, octaves, absolute, scale, translate);
          texture_stack_push(stack, texture);
        }
        break;

      case TEXTURE_IMAGE:
        {
          short w, h;
          unsigned char *image;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &w, sizeof(short), tienet_endian);
          common_unpack_read(app_data, app_ind, &h, sizeof(short), tienet_endian);
          image = (unsigned char*)malloc(3*w*h);
          common_unpack_read(app_data, app_ind, image, 3*w*h, 0);
          texture_image_init(texture, w, h, image);
          texture_stack_push(stack, texture);
          free(image);
        }
        break;

      case TEXTURE_GRADIENT:
        {
          int axis;

          texture = (texture_t*)malloc(sizeof(texture_t));
          common_unpack_read(app_data, app_ind, &axis, sizeof(int), tienet_endian);
          texture_gradient_init(texture, axis);
          texture_stack_push(stack, texture);
        }
        break;

      default:
        break;
    }
  } while(app_ind - start < size);
}


void common_unpack_mesh(common_db_t *db, void *app_data, int size, tie_t *tie) {
  TIE_3			v[3], *tlist = NULL;
  char			c, name[256];
  short			block;
  int			i, j, k, num, start, tnum;


  tnum = 0;
  start = app_ind;

  /* initialize tie with triangle number */
  common_unpack_read(app_data, app_ind, &num, sizeof(int), tienet_endian);
  tie_init(tie, num);

  do {
    common_unpack_read(app_data, app_ind, &block, sizeof(short), tienet_endian);

    /* Create a Mesh */
    db->mesh_num++;
    db->mesh_list = (common_mesh_t **)realloc(db->mesh_list, sizeof(common_mesh_t *)*db->mesh_num);
    db->mesh_list[db->mesh_num-1] = (common_mesh_t *)malloc(sizeof(common_mesh_t));
    db->mesh_list[db->mesh_num-1]->flags = 0;
    db->mesh_list[db->mesh_num-1]->tri_num = 0;
    db->mesh_list[db->mesh_num-1]->tri_list = NULL;

    /* Mesh Name */
    common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
    common_unpack_read(app_data, app_ind, db->mesh_list[db->mesh_num-1]->name, c, 0);

    /* Texture */
    common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);
    common_unpack_read(app_data, app_ind, name, c, 0);
    common_unpack_texture_lookup(name, &(db->mesh_list[db->mesh_num-1]->texture));

    /* Properties */
    common_unpack_prop_lookup(name, &(db->mesh_list[db->mesh_num-1]->prop));

    /* Triangles */
    common_unpack_read(app_data, app_ind, &num, sizeof(int), tienet_endian);

    if(num > tnum) {
      tnum = num;
      tlist = (TIE_3 *)realloc(tlist, sizeof(TIE_3) * tnum * 3);
    }


    db->mesh_list[db->mesh_num-1]->tri_num = num;
    db->mesh_list[db->mesh_num-1]->tri_list = (common_triangle_t *)malloc(num * sizeof(common_triangle_t));

    for(i = 0; i < num; i++) {
      db->mesh_list[db->mesh_num-1]->tri_list[i].mesh = db->mesh_list[db->mesh_num-1];
      db->mesh_list[db->mesh_num-1]->tri_list[i].normals = NULL;

      /* Smooth Triangle */
      common_unpack_read(app_data, app_ind, &c, sizeof(char), 0);

      /* Triangle */
      for(j = 0; j < 3; j++)
        for(k = 0; k < 3; k++)
          common_unpack_read(app_data, app_ind, &v[j].v[k], sizeof(tfloat), tienet_endian);

      tlist[i*3+0] = v[0];
      tlist[i*3+1] = v[1];
      tlist[i*3+2] = v[2];

      if(c) {
        db->mesh_list[db->mesh_num-1]->tri_list[i].normals = (tfloat *)malloc(sizeof(tfloat)*9);
        for(j = 0; j < 9; j++)
          common_unpack_read(app_data, app_ind, &(db->mesh_list[db->mesh_num-1]->tri_list[i].normals[j]), sizeof(tfloat), tienet_endian);
      }

    }


    /* ADD TRIANGLES TO TIE */
    tie_push(tie, tlist, num, db->mesh_list[db->mesh_num-1]->tri_list, sizeof(common_triangle_t));


    /* Min and Max */
    for(j = 0; j < 3; j++)
      common_unpack_read(app_data, app_ind, &(db->mesh_list[db->mesh_num-1]->min.v[j]), sizeof(tfloat), tienet_endian);
    for(j = 0; j < 3; j++)
      common_unpack_read(app_data, app_ind, &(db->mesh_list[db->mesh_num-1]->max.v[j]), sizeof(tfloat), tienet_endian);

    /* Matrix */
    for(j = 0; j < 16; j++)
      common_unpack_read(app_data, app_ind, &(db->mesh_list[db->mesh_num-1]->matrix[j]), sizeof(tfloat), tienet_endian);

    math_mat_invert(db->mesh_list[db->mesh_num-1]->matinv, db->mesh_list[db->mesh_num-1]->matrix, 4);
  } while(app_ind - start < size);

  free(tlist);
}


void common_unpack_prop_lookup(char *name, common_prop_t **prop) {
  int		i;

  for(i = 0; i < prop_num; i++)
    if(!strcmp(name, prop_list[i].name)) {
/*       printf("*** FOUND FOR: -%s-\n", name); */
      *prop = &prop_list[i].prop;
      return;
    }

/*  printf("PROPERTIES NOT FOUND FOR: -%s-\n", name); */
  *prop = &common_unpack_def_prop;
}


void common_unpack_texture_lookup(char *name, texture_t **texture) {
  int		i;

  for(i = 0; i < texture_num; i++)
    if(!strcmp(name, texture_list[i].name)) {
      *texture = texture_list[i].texture;
      return;
    }

  *texture = NULL;
}
