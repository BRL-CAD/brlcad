/*                    D I S P A T C H E R . C
 * BRL-CAD
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
/** @file dispatcher.c
 *
 *  Author -
 *      Justin L. Shumaker
 *
 */

#include "dispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "adrt_common.h"
#include "brlcad_config.h"
#include "rise.h"
#include "tienet.h"

#include <unistd.h>

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#elif defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
#endif


int	rise_dispatcher_progress_delta;
int	rise_dispatcher_progress;
int	rise_dispatcher_interval;
int	rise_dispatcher_lastsave;


void	rise_dispatcher_init(void);
void	rise_dispatcher_free(void);
void	rise_dispatcher_generate(common_db_t *db, void *data, int data_len);
void	rise_dispatcher_result(void *res_buf, int res_len);


void rise_dispatcher_init() {
}


void rise_dispatcher_free() {
}


void rise_dispatcher_generate(common_db_t *db, void *data, int data_len) {
  int i, n;
  common_work_t work;
  void *mesg;

  mesg = malloc(sizeof(common_work_t) + data_len);
  tienet_master_begin();

  work.size_x = db->env.tile_w;
  work.size_y = db->env.tile_h;
  work.format = COMMON_BIT_DEPTH_128;
  for(i = 0; i < db->env.img_vh; i += db->env.tile_w) {
    for(n = 0; n < db->env.img_vw; n += db->env.tile_h) {
      /*
      * Check to see if the alpha component of the rise_image_raw is high or low.
      * If the alpha value is low then this tile will get batched for computing.
      */
#if 0
      if(rise_image_raw[(i*db->env.img_vw+n)*4 + 3]) {
        rise_dispatcher_progress += RISE_TILE_SIZE * RISE_TILE_SIZE;
      } else {
#endif
        work.orig_x = n;
        work.orig_y = i;
        memcpy(&((char *)mesg)[0], &work, sizeof(common_work_t));
        memcpy(&((char *)mesg)[sizeof(common_work_t)], data, data_len);
        tienet_master_push(mesg, sizeof(common_work_t)+data_len);
#if 0
      }
#endif
    }
  }

  tienet_master_end();
  free(mesg);
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
