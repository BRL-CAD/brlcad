/*                     U N P A C K . C
 * BRL-CAD
 *
 * Copyright (c) 2002-2007 United States Government as represented by
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
/** @file unpack.c
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

int prop_num;
common_unpack_prop_node_t *prop_list;

int texture_num;
common_unpack_texture_node_t *texture_list;

common_prop_t common_unpack_def_prop;

void	common_unpack(common_db_t *db, tie_t *tie, util_camera_t *camera, int socknum);
void	common_unpack_free(common_db_t *db);
void	common_unpack_camera(util_camera_t *camera, int socknum);
void	common_unpack_env(common_db_t *db, int socknum);
void	common_unpack_prop(int socknum);
void	common_unpack_texture(int socknum);
void	common_unpack_mesh(common_db_t *db, int socknum, tie_t *tie);
void	common_unpack_kdtree_cache(int socknum, tie_t *tie);
void	common_unpack_mesh_map(common_db_t *db, int socknum);
void	common_unpack_mesh_link(char *mesh_name, char *prop_name, common_db_t *db);
void	common_unpack_prop_lookup(char *name, common_prop_t **prop);
void	common_unpack_texture_lookup(char *name, texture_t **texture);


void common_unpack(common_db_t *db, tie_t *tie, util_camera_t *camera, int socknum) {
  int size;
  short ver, block;

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

  /* Read in the size of the application data */
  tienet_recv(socknum, &size, sizeof(int), 0);

  /* Version */
  tienet_recv(socknum, &ver, sizeof(short), tienet_endian);

  /* Unpack Data */
  common_unpack_env(db, socknum);
  common_unpack_camera(camera, socknum);
  common_unpack_prop(socknum);
  common_unpack_texture(socknum);
  common_unpack_mesh(db, socknum, tie);
  common_unpack_kdtree_cache(socknum, tie);
  common_unpack_mesh_map(db, socknum);
}


void common_unpack_free(common_db_t *db) {
  int i;

  /* Free texture data */
  for(i = 0; i < texture_num; i++)
    texture_list[i].texture->free(texture_list[i].texture);
  free(texture_list);

  if (!db)
    return;

  /* Free mesh data */
  for(i = 0; i < db->mesh_num; i++) {
    /* Free triangle data */
    free(db->mesh_list[i]->tri_list);
    free(db->mesh_list[i]);
  }
  free(db->mesh_list);
}


void common_unpack_env(common_db_t *db, int socknum) {
  int ind, size;
  short block;

  /* size of environment data */
  tienet_recv(socknum, &size, sizeof(int), 0);

  ind = 0;
  do {
    tienet_recv(socknum, &block, sizeof(short), tienet_endian);
    ind += sizeof(short);
    switch(block) {
      case COMMON_PACK_ENV_RM:
	{
	  tienet_recv(socknum, &db->env.rm, sizeof(int), tienet_endian);
	  ind += sizeof(int);
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

	    case RENDER_METHOD_DEPTH:
	      render_depth_init(&db->env.render);
	      break;

	    case RENDER_METHOD_PATH:
	      {
		int samples;

		tienet_recv(socknum, &samples, sizeof(int), tienet_endian);
		ind += sizeof(int);
		render_path_init(&db->env.render, samples);
	      }
	      break;

	    case RENDER_METHOD_PLANE:
	      {
		TIE_3 ray_pos, ray_dir;

		tienet_recv(socknum, &ray_pos.v[0], sizeof(TFLOAT), tienet_endian);
		tienet_recv(socknum, &ray_pos.v[1], sizeof(TFLOAT), tienet_endian);
		tienet_recv(socknum, &ray_pos.v[2], sizeof(TFLOAT), tienet_endian);

		tienet_recv(socknum, &ray_dir.v[0], sizeof(TFLOAT), tienet_endian);
		tienet_recv(socknum, &ray_dir.v[1], sizeof(TFLOAT), tienet_endian);
		tienet_recv(socknum, &ray_dir.v[2], sizeof(TFLOAT), tienet_endian);

		ind += 6 * sizeof(TFLOAT);
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
	  tienet_recv(socknum, &db->env.img_w, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &db->env.img_h, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &db->env.img_hs, sizeof(int), tienet_endian);
	  ind += 3 * sizeof(int);
	}
	break;

      default:
	break;
    }
  } while(ind < size);
}


void common_unpack_camera(util_camera_t *camera, int socknum) {
  int size;

  /* size of camera data */
  tienet_recv(socknum, &size, sizeof(int), 0);

  /* POSITION */
  tienet_recv(socknum, &camera->pos.v[0], sizeof(TFLOAT), tienet_endian);
  tienet_recv(socknum, &camera->pos.v[1], sizeof(TFLOAT), tienet_endian);
  tienet_recv(socknum, &camera->pos.v[2], sizeof(TFLOAT), tienet_endian);

  /* FOCUS */
  tienet_recv(socknum, &camera->focus.v[0], sizeof(TFLOAT), tienet_endian);
  tienet_recv(socknum, &camera->focus.v[1], sizeof(TFLOAT), tienet_endian);
  tienet_recv(socknum, &camera->focus.v[2], sizeof(TFLOAT), tienet_endian);

  /* TILT */
  tienet_recv(socknum, &camera->tilt, sizeof(TFLOAT), tienet_endian);

  /* FIELD OF VIEW */
  tienet_recv(socknum, &camera->fov, sizeof(TFLOAT), tienet_endian);

  /* DEPTH OF FIELD */
  tienet_recv(socknum, &camera->dof, sizeof(TFLOAT), tienet_endian);
}


void common_unpack_prop(int socknum) {
  int ind, size;
  char c;

  /* size of property data */
  tienet_recv(socknum, &size, sizeof(int), 0);

  ind = 0;
  while(ind < size) {
    prop_num++;
    prop_list = (common_unpack_prop_node_t*)realloc(prop_list, sizeof(common_unpack_prop_node_t)*prop_num);

    /* property name */
    tienet_recv(socknum, &c, sizeof(char), 0);
    tienet_recv(socknum, prop_list[prop_num-1].name, c, 0);
    ind += c + 1;

    /* property data */
    tienet_recv(socknum, &prop_list[prop_num-1].prop.color.v[0], sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.color.v[1], sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.color.v[2], sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.density, sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.gloss, sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.emission, sizeof(TFLOAT), tienet_endian);
    tienet_recv(socknum, &prop_list[prop_num-1].prop.ior, sizeof(TFLOAT), tienet_endian);
    ind += 7 * sizeof(TFLOAT);
  }
}


void common_unpack_texture(int socknum) {
  texture_t *stack = NULL, *texture = NULL;
  int ind, size;
  short block;
  unsigned char c;

  /* size of texture data */
  tienet_recv(socknum, &size, sizeof(int), 0);


  ind = 0;
  while(ind < size) {
    tienet_recv(socknum, &block, sizeof(short), tienet_endian);
    ind += sizeof(short);

    switch(block) {
      case TEXTURE_STACK:
	texture_num++;
	texture_list = (common_unpack_texture_node_t *)realloc(texture_list, sizeof(common_unpack_texture_node_t)*texture_num);

	texture_list[texture_num-1].texture = stack = (texture_t *)malloc(sizeof(texture_t));
	texture_stack_init(stack);

	tienet_recv(socknum, &c, sizeof(char), 0);
	tienet_recv(socknum, texture_list[texture_num-1].name, c, 0);
	ind += c + 1;
	break;

      case TEXTURE_MIX:
	{
	  texture_t *texture1, *texture2;
	  char s1[64], s2[64];
	  TFLOAT coef;

	  texture_num++;
	  texture_list = (common_unpack_texture_node_t*)realloc(texture_list, sizeof(common_unpack_texture_node_t)*texture_num);
	  texture_list[texture_num-1].texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &c, sizeof(char), 0);
	  tienet_recv(socknum, texture_list[texture_num-1].name, c, 0);
	  ind += c + c+1;
	  tienet_recv(socknum, &c, sizeof(char), 0);
	  tienet_recv(socknum, s1, c, 0);
	  ind += c + c+1;
	  tienet_recv(socknum, &c, sizeof(char), 0);
	  tienet_recv(socknum, s2, c, 0);
	  ind += c + c+1;
	  tienet_recv(socknum, &coef, sizeof(TFLOAT), tienet_endian);
	  ind += sizeof(TFLOAT);
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
	  tienet_recv(socknum, &color1.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color1.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color1.v[2], sizeof(TFLOAT), tienet_endian);
	  ind += 3 * sizeof(TFLOAT);
	  /* COLOR 2 */
	  tienet_recv(socknum, &color2.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color2.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color2.v[2], sizeof(TFLOAT), tienet_endian);
	  ind += 3 * sizeof(TFLOAT);
	  texture_blend_init(texture, color1, color2);
	  texture_stack_push(stack, texture);
	}
	break;

      case TEXTURE_BUMP:
	{
	  TIE_3 coef;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &coef.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &coef.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &coef.v[2], sizeof(TFLOAT), tienet_endian);
	  ind += 3 * sizeof(TFLOAT);
	  texture_bump_init(texture, coef);
	  texture_stack_push(stack, texture);
	}
	break;

      case TEXTURE_CHECKER:
	{
	  int tile;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &tile, sizeof(int), tienet_endian);
	  ind += sizeof(int);
	  texture_checker_init(texture, tile);
	  texture_stack_push(stack, texture);
	}
	break;

      case TEXTURE_CAMO:
	{
	  TFLOAT size;
	  int octaves, absolute;
	  TIE_3 color1, color2, color3;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &size, sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &octaves, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &absolute, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &color1.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color1.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color1.v[2], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color2.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color2.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color2.v[2], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color3.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color3.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &color3.v[2], sizeof(TFLOAT), tienet_endian);
	  ind += 10*sizeof(TFLOAT) + 2*sizeof(int);
	  texture_camo_init(texture, size, octaves, absolute, color1, color2, color3);
	  texture_stack_push(stack, texture);
	}
	break;

      case TEXTURE_CLOUDS:
	{
	  TFLOAT size;
	  int octaves, absolute;
	  TIE_3 scale, translate;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &size, sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &octaves, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &absolute, sizeof(int), tienet_endian);
	  tienet_recv(socknum, &scale.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &scale.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &scale.v[2], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &translate.v[0], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &translate.v[1], sizeof(TFLOAT), tienet_endian);
	  tienet_recv(socknum, &translate.v[2], sizeof(TFLOAT), tienet_endian);
	  ind += 7*sizeof(TFLOAT) + 2*sizeof(int);
	  texture_clouds_init(texture, size, octaves, absolute, scale, translate);
	  texture_stack_push(stack, texture);
	}
	break;

      case TEXTURE_IMAGE:
	{
	  short w, h;
	  unsigned char *image;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &w, sizeof(short), tienet_endian);
	  tienet_recv(socknum, &h, sizeof(short), tienet_endian);
	  ind += 2*sizeof(short);
	  image = (unsigned char*)malloc(3*w*h);
	  tienet_recv(socknum, image, 3*w*h, 0);
	  ind += 3*w*h;
	  texture_image_init(texture, w, h, image);
	  texture_stack_push(stack, texture);
	  free(image);
	}
	break;

      case TEXTURE_GRADIENT:
	{
	  int axis;

	  texture = (texture_t*)malloc(sizeof(texture_t));
	  tienet_recv(socknum, &axis, sizeof(int), tienet_endian);
	  ind += sizeof(int);
	  texture_gradient_init(texture, axis);
	  texture_stack_push(stack, texture);
	}
	break;

      default:
	break;
    }
  }
}


void common_unpack_mesh(common_db_t *db, int socknum, tie_t *tie) {
  TIE_3 v[3], *vlist, *tlist;
  char name[256];
  unsigned char c;
  short block;
  unsigned int num, ind;
  int size, *flist, i, start, vnum, vmax, fnum, fmax;


  vlist = NULL;
  flist = NULL;
  tlist = NULL;
  vmax = 0;
  fmax = 0;

  /* size of mesh data */
  tienet_recv(socknum, &size, sizeof(int), 0);
  ind = 0;

  /* initialize tie with triangle number */
  tienet_recv(socknum, &num, sizeof(unsigned int), tienet_endian);
  tie_init(tie, num);
  ind += sizeof(unsigned int);

  while(ind < size) {
    /* Create a Mesh */
    db->mesh_num++;
    db->mesh_list = (common_mesh_t **)realloc(db->mesh_list, sizeof(common_mesh_t *)*db->mesh_num);
    db->mesh_list[db->mesh_num-1] = (common_mesh_t *)malloc(sizeof(common_mesh_t));
    db->mesh_list[db->mesh_num-1]->flags = 0;
    db->mesh_list[db->mesh_num-1]->tri_num = 0;
    db->mesh_list[db->mesh_num-1]->tri_list = NULL;

    /* Mesh Name */
    tienet_recv(socknum, &c, sizeof(char), 0);
    tienet_recv(socknum, db->mesh_list[db->mesh_num-1]->name, c, 0);
    ind += c + 1;

    /* Vertices */
    tienet_recv(socknum, &vnum, sizeof(int), tienet_endian);
    ind += sizeof(int);
    if(vnum > vmax) {
      vmax = vnum;
      vlist = (TIE_3 *)realloc(vlist, vmax * sizeof(TIE_3));
    }
    tienet_recv(socknum, vlist, vnum * sizeof(TIE_3), 0);
    ind += vnum * sizeof(TIE_3);

    /* Faces */
    /* Determine short or int based indices */
    tienet_recv(socknum, &c, 1, 0);
    ind += 1;

    if(c) {
      tienet_recv(socknum, &fnum, sizeof(int), tienet_endian);
      ind += sizeof(int);

      if(fnum > fmax) {
	fmax = fnum;
	flist = (int *)realloc(flist, fmax * 3 * sizeof(int));
	tlist = (TIE_3 *)realloc(tlist, fmax * 3 * sizeof(TIE_3));
      }
      tienet_recv(socknum, flist, fnum * 3 * sizeof(int), 0);
      ind += fnum * 3 * sizeof(int);
    } else {
      unsigned short sfnum, sind[144];
      int j, n;

      tienet_recv(socknum, &sfnum, sizeof(unsigned short), tienet_endian);
      ind += sizeof(unsigned short);

      fnum = sfnum;
      if(fnum > fmax) {
	fmax = fnum;
	flist = (int *)realloc(flist, fmax * 3 * sizeof(int));
	tlist = (TIE_3 *)realloc(tlist, fmax * 3 * sizeof(TIE_3));
      }

      i = 0;
      while(i < fnum) {
	n = fnum - i > 48 ? 48 : fnum - i;
	tienet_recv(socknum, &sind, 3 * n * sizeof(unsigned short), 0);

	for(j = 0; j < n; j++) {
	  flist[3*(i+j) + 0] = sind[3*j + 0];
	  flist[3*(i+j) + 1] = sind[3*j + 1];
	  flist[3*(i+j) + 2] = sind[3*j + 2];
	}

	i += n;
      }

      ind += fnum * 3 * sizeof(unsigned short);
    }

    /* Min and Max */
#if 0
    for(j = 0; j < 3; j++)
      tienet_recv(socknum, &(db->mesh_list[db->mesh_num-1]->min.v[j]), sizeof(TFLOAT), tienet_endian);
    for(j = 0; j < 3; j++)
      tienet_recv(socknum, &(db->mesh_list[db->mesh_num-1]->max.v[j]), sizeof(TFLOAT), tienet_endian);
#endif

    /* Matrix */
    for(i = 0; i < 16; i++)
      tienet_recv(socknum, &(db->mesh_list[db->mesh_num-1]->matrix[i]), sizeof(TFLOAT), tienet_endian);
    ind += 16 * sizeof(TFLOAT);

    /* Store inverted matrix */
    math_mat_invert(db->mesh_list[db->mesh_num-1]->matinv, db->mesh_list[db->mesh_num-1]->matrix, 4);

    /* Apply Transformation Matrix to Vertices */
    for(i = 0; i < vnum; i++) {
      v[0] = vlist[i];
      MATH_VEC_TRANSFORM(vlist[i], v[0], db->mesh_list[db->mesh_num-1]->matrix);
    }

    /* Allocate memory for ADRT triangles */
    db->mesh_list[db->mesh_num-1]->tri_num = fnum;
    db->mesh_list[db->mesh_num-1]->tri_list = (common_triangle_t *)malloc(fnum * sizeof(common_triangle_t));

    /* Build the triangle list */
    for(i = 0; i < fnum; i++) {
      db->mesh_list[db->mesh_num-1]->tri_list[i].mesh = db->mesh_list[db->mesh_num-1];
      db->mesh_list[db->mesh_num-1]->tri_list[i].normals = NULL;
      tlist[i*3+0] = vlist[flist[3*i+0]];
      tlist[i*3+1] = vlist[flist[3*i+1]];
      tlist[i*3+2] = vlist[flist[3*i+2]];
    }

    /* ADD TRIANGLES TO TIE */
    tie_push(tie, tlist, fnum, db->mesh_list[db->mesh_num-1]->tri_list, sizeof(common_triangle_t));
  }

  free(vlist);
  free(flist);
  free(tlist);
}


void common_unpack_kdtree_cache(int socknum, tie_t *tie) {
  int size;
  void *kdcache;

  /* size of kdtree cache data */
  tienet_recv(socknum, &size, sizeof(int), 0);

  /* retreive the data */
  if(size > 0) {
    kdcache = malloc(size);
    tienet_recv(socknum, kdcache, size, 0);

    /* feed the kd-tree into libtie */
    tie_kdtree_cache_load(tie, kdcache);

    free(kdcache);
  }
}


void common_unpack_mesh_map(common_db_t *db, int socknum) {
  unsigned int size, ind;
  unsigned char c;
  char mesh_name[256], prop_name[256];

  /* size of mesh data */
  tienet_recv(socknum, &size, sizeof(unsigned int), 0);
  ind = 0;

  while(ind < size) {
    tienet_recv(socknum, &c, 1, 0);
    tienet_recv(socknum, mesh_name, c, 0);
    ind += 1 + c;

    tienet_recv(socknum, &c, 1, 0);
    tienet_recv(socknum, prop_name, c, 0);
    ind += 1 + c;

    /* Link a property and texture to a mesh */
    common_unpack_mesh_link(mesh_name, prop_name, db);
  }
}


void common_unpack_mesh_link(char *mesh_name, char *prop_name, common_db_t *db) {
  unsigned int i;


  for(i = 0; i < db->mesh_num; i++) {
    /* Find Mesh */
    if(!strcmp(mesh_name, db->mesh_list[i]->name)) {
      common_unpack_prop_lookup(prop_name, &(db->mesh_list[i]->prop));
      db->mesh_list[i]->texture = NULL;
      return;
    }
  }
}


void common_unpack_prop_lookup(char *name, common_prop_t **prop) {
  unsigned int i;

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
  unsigned int i;

  for(i = 0; i < texture_num; i++)
    if(!strcmp(name, texture_list[i].name)) {
      *texture = texture_list[i].texture;
      return;
    }

  *texture = NULL;
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
