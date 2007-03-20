/*                     C A M E R A . C
 * BRL-CAD / ADRT
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
/** @file camera.c
 *
 *  Comments -
 *      Utilities Library - Camera
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

#include "camera.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "brlcad_config.h"
#include "image.h"
#include "umath.h"

#ifdef HAVE_SYS_SYSINFO_H
#  include <sys/sysinfo.h>
#elif defined(HAVE_SYS_SYSCTL_H)
#  include <sys/sysctl.h>
#endif


pthread_t *util_tlist;


int get_nprocs(void);


#ifndef HAVE_SYS_SYSINFO_H
#ifdef HAVE_SYS_SYSCTL_H
int get_nprocs() {
  int mib[2], maxproc;
  size_t len;

  mib[0] = CTL_HW;
  mib[1] = HW_NCPU;
  len = sizeof(maxproc);
  sysctl(mib, 2, &maxproc, &len, NULL, 0);
  return maxproc;
}
#else
int get_nprocs() {
  return 1;
}
#endif
#endif


void* util_camera_render_thread(void *ptr);


void util_camera_init(util_camera_t *camera, int threads) {
  camera->view_num = 0;
  camera->view_list = NULL;

  /* The camera will use a thread for every cpu the machine has. */
  camera->thread_num = threads ? threads : get_nprocs();

  if(camera->thread_num > 1) {
    util_tlist = (pthread_t *)malloc(sizeof(pthread_t) * camera->thread_num);
    if (!util_tlist) {
	perror("malloc");
	exit(1);
    }
    if (!util_tlist) {
	perror("util_tlist");
	exit(1);
    }
  }
}


void util_camera_free(util_camera_t *camera) {
  if(camera->thread_num > 1)
    free(util_tlist);
}


void util_camera_prep(util_camera_t *camera, common_db_t *db) {
  TIE_3		dof_topl, dof_botl, dof_topr, fov_topl, fov_botl, fov_topr;
  TIE_3		temp, dof_look, dof_up, dof_side, fov_look, fov_up, fov_side, step_x, step_y;
  TFLOAT	sfov, cfov, sdof, cdof, aspect, angle, mag;
  int		i, n;


  /* Generate an aspect ratio coefficient */
  aspect = (TFLOAT)db->env.img_vw / (TFLOAT)db->env.img_vh;

  /* Free camera view list if already allocated */
  if(camera->view_list)
    free(camera->view_list);

  /* If there is no depth of field then just generate the standard look vector */
  if(camera->dof == 0.0) {
    camera->view_num = 1;
    camera->view_list = (util_camera_view_t *)malloc(sizeof(util_camera_view_t) * camera->view_num);
    if (!camera->view_list) {
	perror("view_list");
	exit(1);
    }

    /* Generate unitized look vector */
    MATH_VEC_SUB(fov_look, camera->focus, camera->pos);
    MATH_VEC_UNITIZE(fov_look);

    /* Generate standard up vector */
    fov_up.v[0] = 0;
    fov_up.v[1] = 0;
    fov_up.v[2] = 1;

    /* Make unitized up vector perpendicular to look vector */
    temp = fov_look;
    MATH_VEC_DOT(angle, fov_up, temp);
    MATH_VEC_MUL_SCALAR(temp, temp, angle);
    MATH_VEC_SUB(fov_up, fov_up, temp);
    MATH_VEC_UNITIZE(fov_up);

    /* Generate a temporary side vector */
    MATH_VEC_CROSS(fov_side, fov_up, fov_look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    sfov = sin(-camera->tilt * MATH_PI * MATH_1DIV180);
    cfov = cos(-camera->tilt * MATH_PI * MATH_1DIV180);
    MATH_VEC_MUL_SCALAR(fov_up, fov_up, cfov);
    MATH_VEC_MUL_SCALAR(fov_side, fov_side, sfov);
    MATH_VEC_ADD(fov_up, fov_up, fov_side);

    /* Create final side vector */
    MATH_VEC_CROSS(fov_side, fov_up, fov_look);

    /* Compute sine and cosine terms for field of view */
    sfov = sin(camera->fov*MATH_PI * MATH_1DIV180);
    cfov = cos(camera->fov*MATH_PI * MATH_1DIV180);


    /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
    fov_topl.v[0] = sfov*fov_up.v[0] + aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
    fov_topl.v[1] = sfov*fov_up.v[1] + aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
    fov_topl.v[2] = sfov*fov_up.v[2] + aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

    fov_topr.v[0] = sfov*fov_up.v[0] - aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
    fov_topr.v[1] = sfov*fov_up.v[1] - aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
    fov_topr.v[2] = sfov*fov_up.v[2] - aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

    fov_botl.v[0] = -sfov*fov_up.v[0] + aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
    fov_botl.v[1] = -sfov*fov_up.v[1] + aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
    fov_botl.v[2] = -sfov*fov_up.v[2] + aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

    MATH_VEC_UNITIZE(fov_topl);
    MATH_VEC_UNITIZE(fov_botl);
    MATH_VEC_UNITIZE(fov_topr);

    /* Store Camera Position */
    camera->view_list[0].pos = camera->pos;

    /* Store the top left vector */
    camera->view_list[0].top_l = fov_topl;

    /* Generate stepx and stepy vectors for sampling each pixel */
    MATH_VEC_SUB(camera->view_list[0].step_x, fov_topr, fov_topl);
    MATH_VEC_SUB(camera->view_list[0].step_y, fov_botl, fov_topl);

    /* Divide stepx and stepy by the number of pixels */
    MATH_VEC_MUL_SCALAR(camera->view_list[0].step_x, camera->view_list[0].step_x, 1.0 / db->env.img_vw);
    MATH_VEC_MUL_SCALAR(camera->view_list[0].step_y, camera->view_list[0].step_y, 1.0 / db->env.img_vh);
    return;
  }

  /*
  * GENERATE DEPTH OF VIEW CAMERA DATA
  */

  /* Generate unitized look vector */
  MATH_VEC_SUB(dof_look, camera->focus, camera->pos);
  MATH_VEC_UNITIZE(dof_look);

  /* Generate standard up vector */
  dof_up.v[0] = 0;
  dof_up.v[1] = 0;
  dof_up.v[2] = 1;

  /* Make unitized up vector perpendicular to look vector */
  temp = dof_look;
  MATH_VEC_DOT(angle, dof_up, temp);
  MATH_VEC_MUL_SCALAR(temp, temp, angle);
  MATH_VEC_SUB(dof_up, dof_up, temp);
  MATH_VEC_UNITIZE(dof_up);

  /* Generate a temporary side vector */
  MATH_VEC_CROSS(dof_side, dof_up, dof_look);

  /* Apply tilt to up vector - negate angle to make positive angles clockwise */
  sdof = sin(-camera->tilt * MATH_PI * MATH_1DIV180);
  cdof = cos(-camera->tilt * MATH_PI * MATH_1DIV180);
  MATH_VEC_MUL_SCALAR(dof_up, dof_up, cdof);
  MATH_VEC_MUL_SCALAR(dof_side, dof_side, sdof);
  MATH_VEC_ADD(dof_up, dof_up, dof_side);

  /* Create final side vector */
  MATH_VEC_CROSS(dof_side, dof_up, dof_look);

  /*
  * Generage a camera position, top left vector, and step vectors for each DOF sample
  */

  /* Obtain magnitude of reverse look vector */
  MATH_VEC_SUB(dof_look, camera->pos, camera->focus);
  MATH_VEC_MAG(mag, dof_look);
  MATH_VEC_UNITIZE(dof_look);


  /* Compute sine and cosine terms for field of view */
  sdof = sin(camera->dof*MATH_PI * MATH_1DIV180);
  cdof = cos(camera->dof*MATH_PI * MATH_1DIV180);


  /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
  dof_topl.v[0] = sdof*dof_up.v[0] + sdof*dof_side.v[0] + cdof*dof_look.v[0];
  dof_topl.v[1] = sdof*dof_up.v[1] + sdof*dof_side.v[1] + cdof*dof_look.v[1];
  dof_topl.v[2] = sdof*dof_up.v[2] + sdof*dof_side.v[2] + cdof*dof_look.v[2];

  dof_topr.v[0] = sdof*dof_up.v[0] - sdof*dof_side.v[0] + cdof*dof_look.v[0];
  dof_topr.v[1] = sdof*dof_up.v[1] - sdof*dof_side.v[1] + cdof*dof_look.v[1];
  dof_topr.v[2] = sdof*dof_up.v[2] - sdof*dof_side.v[2] + cdof*dof_look.v[2];

  dof_botl.v[0] = -sdof*dof_up.v[0] + sdof*dof_side.v[0] + cdof*dof_look.v[0];
  dof_botl.v[1] = -sdof*dof_up.v[1] + sdof*dof_side.v[1] + cdof*dof_look.v[1];
  dof_botl.v[2] = -sdof*dof_up.v[2] + sdof*dof_side.v[2] + cdof*dof_look.v[2];

  MATH_VEC_UNITIZE(dof_topl);
  MATH_VEC_UNITIZE(dof_botl);
  MATH_VEC_UNITIZE(dof_topr);

  MATH_VEC_SUB(step_x, dof_topr, dof_topl);
  MATH_VEC_SUB(step_y, dof_botl, dof_topl);

  /* Generate camera positions for depth of field */
  camera->view_num = UTIL_CAMERA_DOF_SAMPLES*UTIL_CAMERA_DOF_SAMPLES;
  camera->view_list = (util_camera_view_t *)malloc(sizeof(util_camera_view_t) * camera->view_num);
  if (!camera->view_list) {
      perror("view_list");
      exit(1);
  }

  for(i = 0; i < UTIL_CAMERA_DOF_SAMPLES; i++) {
    for(n = 0; n < UTIL_CAMERA_DOF_SAMPLES; n++) {
      /* Generate virtual camera position for this depth of field sample */
      MATH_VEC_MUL_SCALAR(temp, step_x, ((TFLOAT)i/(TFLOAT)(UTIL_CAMERA_DOF_SAMPLES-1)));
      MATH_VEC_ADD(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, dof_topl, temp);
      MATH_VEC_MUL_SCALAR(temp, step_y, ((TFLOAT)n/(TFLOAT)(UTIL_CAMERA_DOF_SAMPLES-1)));
      MATH_VEC_ADD(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, temp);
      MATH_VEC_UNITIZE(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos);
      MATH_VEC_MUL_SCALAR(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, mag);
      MATH_VEC_ADD(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->focus);

      /* Generate unitized look vector */
      MATH_VEC_SUB(fov_look, camera->focus, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos);
      MATH_VEC_UNITIZE(fov_look);

      /* Generate standard up vector */
      fov_up.v[0] = 0;
      fov_up.v[1] = 0;
      fov_up.v[2] = 1;

      /* Make unitized up vector perpendicular to look vector */
      temp = fov_look;
      MATH_VEC_DOT(angle, fov_up, temp);
      MATH_VEC_MUL_SCALAR(temp, temp, angle);
      MATH_VEC_SUB(fov_up, fov_up, temp);
      MATH_VEC_UNITIZE(fov_up);

      /* Generate a temporary side vector */
      MATH_VEC_CROSS(fov_side, fov_up, fov_look);

      /* Apply tilt to up vector - negate angle to make positive angles clockwise */
      sfov = sin(-camera->tilt * MATH_PI * MATH_1DIV180);
      cfov = cos(-camera->tilt * MATH_PI * MATH_1DIV180);
      MATH_VEC_MUL_SCALAR(fov_up, fov_up, cfov);
      MATH_VEC_MUL_SCALAR(fov_side, fov_side, sfov);
      MATH_VEC_ADD(fov_up, fov_up, fov_side);

      /* Create final side vector */
      MATH_VEC_CROSS(fov_side, fov_up, fov_look);

      /* Compute sine and cosine terms for field of view */
      sfov = sin(camera->fov*MATH_PI * MATH_1DIV180);
      cfov = cos(camera->fov*MATH_PI * MATH_1DIV180);


      /* Up, Look, and Side vectors are complete, generate Top Left reference vector */
      fov_topl.v[0] = sfov*fov_up.v[0] + aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
      fov_topl.v[1] = sfov*fov_up.v[1] + aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
      fov_topl.v[2] = sfov*fov_up.v[2] + aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

      fov_topr.v[0] = sfov*fov_up.v[0] - aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
      fov_topr.v[1] = sfov*fov_up.v[1] - aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
      fov_topr.v[2] = sfov*fov_up.v[2] - aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

      fov_botl.v[0] = -sfov*fov_up.v[0] + aspect*sfov*fov_side.v[0] + cfov*fov_look.v[0];
      fov_botl.v[1] = -sfov*fov_up.v[1] + aspect*sfov*fov_side.v[1] + cfov*fov_look.v[1];
      fov_botl.v[2] = -sfov*fov_up.v[2] + aspect*sfov*fov_side.v[2] + cfov*fov_look.v[2];

      MATH_VEC_UNITIZE(fov_topl);
      MATH_VEC_UNITIZE(fov_botl);
      MATH_VEC_UNITIZE(fov_topr);

      /* Store the top left vector */
      camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].top_l = fov_topl;

      /* Generate stepx and stepy vectors for sampling each pixel */
      MATH_VEC_SUB(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, fov_topr, fov_topl);
      MATH_VEC_SUB(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, fov_botl, fov_topl);

      /* Divide stepx and stepy by the number of pixels */
      MATH_VEC_MUL_SCALAR(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, 1.0 / db->env.img_vw);
      MATH_VEC_MUL_SCALAR(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, 1.0 / db->env.img_vh);
    }
  }
}


void* util_camera_render_thread(void *ptr) {
  util_camera_thread_data_t *td;
  int d, n, res_ind, scanline, v_scanline;
  TIE_3 pixel, accum, v1, v2;
  tie_ray_t ray;
  TFLOAT view_inv;


  td = (util_camera_thread_data_t *)ptr;
  view_inv = 1.0 / td->camera->view_num;


  res_ind = 0;
  /*  for(i = td->work.orig_y; i < td->work.orig_y + td->work.size_y; i++) { */	/* row, vertical */
  while(1) {
    /* Determine if this scanline should be computed by this thread */
    pthread_mutex_lock(&td->mut);
    if(*td->scanline == td->work.size_y) {
      pthread_mutex_unlock(&td->mut);
      return(0);
    } else {
      scanline = *td->scanline;
      (*td->scanline)++;
    }
    pthread_mutex_unlock(&td->mut);

    v_scanline = scanline + td->work.orig_y;
    if(td->work.format == COMMON_BIT_DEPTH_24) {
      res_ind = 3*scanline*td->work.size_x;
    } else if(td->work.format == COMMON_BIT_DEPTH_128) {
      res_ind = 4*scanline*td->work.size_x;
    }


    /* optimization if there is no depth of field being applied */
    if(td->camera->view_num == 1) {
      MATH_VEC_MUL_SCALAR(v1, td->camera->view_list[0].step_y, v_scanline);
      MATH_VEC_ADD(v1, v1, td->camera->view_list[0].top_l);
    }


    /* scanline, horizontal, each pixel */
    for(n = td->work.orig_x; n < td->work.orig_x + td->work.size_x; n++) {

      /* depth of view samples */
      if(td->camera->view_num > 1) {
	MATH_VEC_SET(accum, 0, 0, 0);

	for(d = 0; d < td->camera->view_num; d++) {
	  MATH_VEC_MUL_SCALAR(ray.dir, td->camera->view_list[d].step_y, v_scanline);
	  MATH_VEC_ADD(ray.dir, ray.dir, td->camera->view_list[d].top_l);
	  MATH_VEC_MUL_SCALAR(v1, td->camera->view_list[d].step_x, n);
	  MATH_VEC_ADD(ray.dir, ray.dir, v1);

	  MATH_VEC_SET(pixel, 0, 0, 0);

	  ray.pos = td->camera->view_list[d].pos;
	  ray.depth = 0;
	  MATH_VEC_UNITIZE(ray.dir);

	  /* Compute pixel value using this ray */
	  td->db->env.render.work(&td->db->env.render, td->tie, &ray, &pixel);

	  MATH_VEC_ADD(accum, accum, pixel);
	}

	/* Find Mean value of all views */
	MATH_VEC_MUL_SCALAR(pixel, accum, view_inv);
      } else {
	MATH_VEC_MUL_SCALAR(v2, td->camera->view_list[0].step_x, n);
	MATH_VEC_ADD(ray.dir, v1, v2);

	MATH_VEC_SET(pixel, 0, 0, 0);

	ray.pos = td->camera->view_list[0].pos;
	ray.depth = 0;
	MATH_VEC_UNITIZE(ray.dir);

	/* Compute pixel value using this ray */
	td->db->env.render.work(&td->db->env.render, td->tie, &ray, &pixel);
      }


      if(td->work.format == COMMON_BIT_DEPTH_24) {
	if(pixel.v[0] > 1) pixel.v[0] = 1;
	if(pixel.v[1] > 1) pixel.v[1] = 1;
	if(pixel.v[2] > 1) pixel.v[2] = 1;
	((char *)(td->res_buf))[res_ind+0] = (unsigned char)(255 * pixel.v[0]);
	((char *)(td->res_buf))[res_ind+1] = (unsigned char)(255 * pixel.v[1]);
	((char *)(td->res_buf))[res_ind+2] = (unsigned char)(255 * pixel.v[2]);
	res_ind += 3;
      } else if(td->work.format == COMMON_BIT_DEPTH_128) {
	TFLOAT alpha;

	alpha = 1.0;

	((TFLOAT *)(td->res_buf))[res_ind + 0] = pixel.v[0];
	((TFLOAT *)(td->res_buf))[res_ind + 1] = pixel.v[1];
	((TFLOAT *)(td->res_buf))[res_ind + 2] = pixel.v[2];
	((TFLOAT *)(td->res_buf))[res_ind + 3] = alpha;

	res_ind += 4;
      }
/*          printf("Pixel: [%d, %d, %d]\n", rgb[0], rgb[1], rgb[2]); */

    }
  }

  return(0);
}


void util_camera_render(util_camera_t *camera, common_db_t *db, tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len) {
  common_work_t work;
  util_camera_thread_data_t td;
  unsigned char *scan_map;
  TIE_3 vec;
  unsigned int i, scanline;


  /* Format incoming data into a work structure */
  memcpy(&work, data, sizeof(common_work_t));

  /* Flip bits if endian requires us to */
  if(tienet_endian) {
    tienet_flip(&work.orig_x, &work.orig_x, sizeof(short));
    tienet_flip(&work.orig_y, &work.orig_y, sizeof(short));
    tienet_flip(&work.size_x, &work.size_x, sizeof(short));
    tienet_flip(&work.size_y, &work.size_y, sizeof(short));
    tienet_flip(&work.format, &work.format, sizeof(short));
  }


  if(work.format == COMMON_BIT_DEPTH_24) {
    *res_len = 3 * work.size_x * work.size_y + sizeof(common_work_t);
  } else if(work.format == COMMON_BIT_DEPTH_128) {
    *res_len = 4 * sizeof(TFLOAT) * work.size_x * work.size_y + sizeof(common_work_t);
  }

  *res_buf = realloc(*res_buf, *res_len);
  memcpy(*res_buf, data, sizeof(common_work_t));

  td.tie = tie;
  td.camera = camera;
  td.db = db;
  td.work = work;
  td.res_buf = &((char *)*res_buf)[sizeof(common_work_t)];
  scanline = 0;
  td.scanline = &scanline;
  pthread_mutex_init(&td.mut, 0);

  /* Launch Render threads */
  if(camera->thread_num > 1) {
    for(i = 0; i < camera->thread_num; i++)
      pthread_create(&util_tlist[i], NULL, util_camera_render_thread, &td);
    for(i = 0; i < camera->thread_num; i++)
      pthread_join(util_tlist[i], NULL);
  } else {
    util_camera_render_thread(&td);
  }

  pthread_mutex_destroy(&td.mut);
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
