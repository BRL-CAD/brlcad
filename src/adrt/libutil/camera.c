/*                     C A M E R A . C
 *
 * @file camera.c
 *
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


void	util_camera_init(util_camera_t *camera, int threads);
void	util_camera_free(util_camera_t *camera);
void	util_camera_prep(util_camera_t *camera, common_db_t *db);
void*	util_camera_render_thread(void *ptr);
void	util_camera_render(util_camera_t *camera, common_db_t *db, tie_t *tie, void *data, int size, void **res_buf, int *res_len);


void util_camera_init(util_camera_t *camera, int threads) {
  camera->view_num = 0;
  camera->view_list = NULL;

  /* The camera will use a thread for every cpu the machine has. */
  camera->thread_num = threads ? threads : get_nprocs();

  if(camera->thread_num > 1)
    util_tlist = (pthread_t *)malloc(sizeof(pthread_t) * camera->thread_num);
}


void util_camera_free(util_camera_t *camera) {
  if(camera->thread_num > 1)
    free(util_tlist);
}


void util_camera_prep(util_camera_t *camera, common_db_t *db) {
  TIE_3		dof_topl, dof_botl, dof_topr, fov_topl, fov_botl, fov_topr;
  TIE_3		temp, dof_look, dof_up, dof_side, fov_look, fov_up, fov_side, step_x, step_y;
  tfloat	sfov, cfov, sdof, cdof, aspect, angle, mag;
  int		i, n;


  /* Generate an aspect ratio coefficient */
  aspect = (tfloat)db->env.img_vw / (tfloat)db->env.img_vh;

  /* Free camera view list if already allocated */
  if(camera->view_list)
    free(camera->view_list);

  /* If there is no depth of field then just generate the standard look vector */
  if(camera->dof == 0.0) {
    camera->view_num = 1;
    camera->view_list = (util_camera_view_t *)malloc(sizeof(util_camera_view_t) * camera->view_num);

    /* Generate unitized look vector */
    math_vec_sub(fov_look, camera->focus, camera->pos);
    math_vec_unitize(fov_look);

    /* Generate standard up vector */
    fov_up.v[0] = 0;
    fov_up.v[1] = 0;
    fov_up.v[2] = 1;

    /* Make unitized up vector perpendicular to look vector */
    temp = fov_look;
    math_vec_dot(angle, fov_up, temp);
    math_vec_mul_scalar(temp, temp, angle);
    math_vec_sub(fov_up, fov_up, temp);
    math_vec_unitize(fov_up);

    /* Generate a temporary side vector */
    math_vec_cross(fov_side, fov_up, fov_look);

    /* Apply tilt to up vector - negate angle to make positive angles clockwise */
    sfov = sin(-camera->tilt * math_pi * math_1div180);
    cfov = cos(-camera->tilt * math_pi * math_1div180);
    math_vec_mul_scalar(fov_up, fov_up, cfov);
    math_vec_mul_scalar(fov_side, fov_side, sfov);
    math_vec_add(fov_up, fov_up, fov_side);

    /* Create final side vector */
    math_vec_cross(fov_side, fov_up, fov_look);

    /* Compute sine and cosine terms for field of view */
    sfov = sin(camera->fov*math_pi * math_1div180);
    cfov = cos(camera->fov*math_pi * math_1div180);


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

    math_vec_unitize(fov_topl);
    math_vec_unitize(fov_botl);
    math_vec_unitize(fov_topr);

    /* Store Camera Position */
    camera->view_list[0].pos = camera->pos;

    /* Store the top left vector */
    camera->view_list[0].top_l = fov_topl;

    /* Generate stepx and stepy vectors for sampling each pixel */
    math_vec_sub(camera->view_list[0].step_x, fov_topr, fov_topl);
    math_vec_sub(camera->view_list[0].step_y, fov_botl, fov_topl);

    /* Divide stepx and stepy by the number of pixels */
    math_vec_mul_scalar(camera->view_list[0].step_x, camera->view_list[0].step_x, 1.0 / db->env.img_vw);
    math_vec_mul_scalar(camera->view_list[0].step_y, camera->view_list[0].step_y, 1.0 / db->env.img_vh);
    return;
  }

  /*
  * GENERATE DEPTH OF VIEW CAMERA DATA
  */

  /* Generate unitized look vector */
  math_vec_sub(dof_look, camera->focus, camera->pos);
  math_vec_unitize(dof_look);

  /* Generate standard up vector */
  dof_up.v[0] = 0;
  dof_up.v[1] = 0;
  dof_up.v[2] = 1;

  /* Make unitized up vector perpendicular to look vector */
  temp = dof_look;
  math_vec_dot(angle, dof_up, temp);
  math_vec_mul_scalar(temp, temp, angle);
  math_vec_sub(dof_up, dof_up, temp);
  math_vec_unitize(dof_up);

  /* Generate a temporary side vector */
  math_vec_cross(dof_side, dof_up, dof_look);

  /* Apply tilt to up vector - negate angle to make positive angles clockwise */
  sdof = sin(-camera->tilt * math_pi * math_1div180);
  cdof = cos(-camera->tilt * math_pi * math_1div180);
  math_vec_mul_scalar(dof_up, dof_up, cdof);
  math_vec_mul_scalar(dof_side, dof_side, sdof);
  math_vec_add(dof_up, dof_up, dof_side);

  /* Create final side vector */
  math_vec_cross(dof_side, dof_up, dof_look);

  /*
  * Generage a camera position, top left vector, and step vectors for each DOF sample
  */

  /* Obtain magnitude of reverse look vector */
  math_vec_sub(dof_look, camera->pos, camera->focus);
  math_vec_mag(mag, dof_look);
  math_vec_unitize(dof_look);


  /* Compute sine and cosine terms for field of view */
  sdof = sin(camera->dof*math_pi * math_1div180);
  cdof = cos(camera->dof*math_pi * math_1div180);


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

  math_vec_unitize(dof_topl);
  math_vec_unitize(dof_botl);
  math_vec_unitize(dof_topr);

  math_vec_sub(step_x, dof_topr, dof_topl);
  math_vec_sub(step_y, dof_botl, dof_topl);

  /* Generate camera positions for depth of field */
  camera->view_num = UTIL_CAMERA_DOF_SAMPLES*UTIL_CAMERA_DOF_SAMPLES;
  camera->view_list = (util_camera_view_t *)malloc(sizeof(util_camera_view_t) * camera->view_num);

  for(i = 0; i < UTIL_CAMERA_DOF_SAMPLES; i++) {
    for(n = 0; n < UTIL_CAMERA_DOF_SAMPLES; n++) {
      /* Generate virtual camera position for this depth of field sample */
      math_vec_mul_scalar(temp, step_x, ((tfloat)i/(tfloat)(UTIL_CAMERA_DOF_SAMPLES-1)));
      math_vec_add(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, dof_topl, temp);
      math_vec_mul_scalar(temp, step_y, ((tfloat)n/(tfloat)(UTIL_CAMERA_DOF_SAMPLES-1)));
      math_vec_add(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, temp);
      math_vec_unitize(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos);
      math_vec_mul_scalar(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, mag);
      math_vec_add(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos, camera->focus);

      /* Generate unitized look vector */
      math_vec_sub(fov_look, camera->focus, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].pos);
      math_vec_unitize(fov_look);

      /* Generate standard up vector */
      fov_up.v[0] = 0;
      fov_up.v[1] = 0;
      fov_up.v[2] = 1;

      /* Make unitized up vector perpendicular to look vector */
      temp = fov_look;
      math_vec_dot(angle, fov_up, temp);
      math_vec_mul_scalar(temp, temp, angle);
      math_vec_sub(fov_up, fov_up, temp);
      math_vec_unitize(fov_up);

      /* Generate a temporary side vector */
      math_vec_cross(fov_side, fov_up, fov_look);

      /* Apply tilt to up vector - negate angle to make positive angles clockwise */
      sfov = sin(-camera->tilt * math_pi * math_1div180);
      cfov = cos(-camera->tilt * math_pi * math_1div180);
      math_vec_mul_scalar(fov_up, fov_up, cfov);
      math_vec_mul_scalar(fov_side, fov_side, sfov);
      math_vec_add(fov_up, fov_up, fov_side);

      /* Create final side vector */
      math_vec_cross(fov_side, fov_up, fov_look);

      /* Compute sine and cosine terms for field of view */
      sfov = sin(camera->fov*math_pi * math_1div180);
      cfov = cos(camera->fov*math_pi * math_1div180);


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

      math_vec_unitize(fov_topl);
      math_vec_unitize(fov_botl);
      math_vec_unitize(fov_topr);

      /* Store the top left vector */
      camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].top_l = fov_topl;

      /* Generate stepx and stepy vectors for sampling each pixel */
      math_vec_sub(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, fov_topr, fov_topl);
      math_vec_sub(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, fov_botl, fov_topl);

      /* Divide stepx and stepy by the number of pixels */
      math_vec_mul_scalar(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_x, 1.0 / db->env.img_vw);
      math_vec_mul_scalar(camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, camera->view_list[i*UTIL_CAMERA_DOF_SAMPLES+n].step_y, 1.0 / db->env.img_vh);
    }
  }
}


void* util_camera_render_thread(void *ptr) {
  util_camera_thread_data_t	*td;
  int				d, i, n, res_ind, compute;
  TIE_3				pixel, accum, v;
  tie_ray_t			ray;
  tfloat			view_inv;


  td = (util_camera_thread_data_t *)ptr;
  view_inv = 1.0 / td->camera->view_num;


  res_ind = 0;
  for(i = td->work.orig_y; i < td->work.orig_y + td->work.size_y; i++) {	/* row, vertical */
    /* Determine if this scanline should be computed by this thread */
    pthread_mutex_lock(&td->mut);
    compute = td->scan_map[i-td->work.orig_y];
    td->scan_map[i-td->work.orig_y] = 0;
    pthread_mutex_unlock(&td->mut);

    if(compute) {
      for(n = td->work.orig_x; n < td->work.orig_x + td->work.size_x; n++) {	/* scanline, horizontal, each pixel */

        accum.v[0] = accum.v[1] = accum.v[2] = 0;
        for(d = 0; d < td->camera->view_num; d++) {	/* depth of view samples */
          math_vec_mul_scalar(v, td->camera->view_list[d].step_x, n);
          math_vec_add(ray.dir, td->camera->view_list[d].top_l, v);
          math_vec_mul_scalar(v, td->camera->view_list[d].step_y, i);
          math_vec_add(ray.dir, ray.dir, v);

          math_vec_set(pixel, 0, 0, 0);

          ray.pos = td->camera->view_list[d].pos;
          ray.depth = 0;
          math_vec_unitize(ray.dir);

          /* Compute pixel value using this ray */
          td->db->env.render.work(&td->db->env.render, td->tie, &ray, &pixel);

          math_vec_add(accum, accum, pixel);
        }

        /* Find Mean value of all views */
        math_vec_mul_scalar(pixel, accum, view_inv);

        if(td->work.format == COMMON_BIT_DEPTH_24) {
          unsigned char rgb_24[3];

          if(pixel.v[0] > 1) pixel.v[0] = 1;
          if(pixel.v[1] > 1) pixel.v[1] = 1;
          if(pixel.v[2] > 1) pixel.v[2] = 1;
          rgb_24[0] = (unsigned char)(255 * pixel.v[0]);
          rgb_24[1] = (unsigned char)(255 * pixel.v[1]);
          rgb_24[2] = (unsigned char)(255 * pixel.v[2]);

          /* Pack pixel into result buffer */
          memcpy(&((char *)(td->res_buf))[res_ind], rgb_24, 3);
          res_ind += 3;
        } else if(td->work.format == COMMON_BIT_DEPTH_128) {
          tfloat rgb_128[4];

          rgb_128[0] = pixel.v[0];
          rgb_128[1] = pixel.v[1];
          rgb_128[2] = pixel.v[2];
          rgb_128[3] = 1.0;

          memcpy(&((char *)(td->res_buf))[res_ind], rgb_128, 4*sizeof(tfloat));
          res_ind += 4*sizeof(tfloat);
        }
/*          printf("Pixel: [%d, %d, %d]\n", rgb[0], rgb[1], rgb[2]); */

      }
    } else {
      if(td->work.format == COMMON_BIT_DEPTH_24) {
        res_ind += 3*td->work.size_x;
      } else if(td->work.format == COMMON_BIT_DEPTH_128) {
        res_ind += 4*sizeof(tfloat)*td->work.size_x;
      }
    }
  }

  return(0);
}


void util_camera_render(util_camera_t *camera, common_db_t *db, tie_t *tie, void *data, int size, void **res_buf, int *res_len) {
  common_work_t work;
  util_camera_thread_data_t td;
  unsigned char *scan_map;
  TIE_3 vec;
  int i;


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

  /* allocate memory for scanmap */
  scan_map = (unsigned char *)malloc(work.size_y);
  memset(scan_map, 1, work.size_y);


  if(work.format == COMMON_BIT_DEPTH_24) {
    *res_len = 3 * work.size_x * work.size_y + sizeof(common_work_t);
  } else if(work.format == COMMON_BIT_DEPTH_128) {
    *res_len = 4 * sizeof(tfloat) * work.size_x * work.size_y + sizeof(common_work_t);
  }

  *res_buf = realloc(*res_buf, *res_len);
  memcpy(*res_buf, data, sizeof(common_work_t));

  td.tie = tie;
  td.camera = camera;
  td.db = db;
  td.work = work;
  td.res_buf = &((char *)*res_buf)[sizeof(common_work_t)];
  td.scan_map = scan_map;
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

  free(scan_map);
  pthread_mutex_destroy(&td.mut);
}
