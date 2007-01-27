/*                     C A N I M . C
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
/** @file canim.c
 *
 *  Comments -
 *      Common Library - Animation Data Parser
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

#include "canim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tie.h"
#include "umath.h"


int common_anim_read(common_anim_t *anim, char *frames_file) {
  FILE *fh;
  char line[256], *token;

  fh = fopen(frames_file, "r");
  if(!fh) {
    printf("Frames file: %s does not exist, exiting...\n", frames_file);
    exit(1);
  }

  anim->frame_num = -1;
  anim->frame_list = NULL;


  while(fgets(line, 256, fh)) {
    token = strtok(line, ",");

    if(!strcmp("frame", token)) {

      anim->frame_num++;
      anim->frame_list = (common_anim_frame_t *)realloc(anim->frame_list, sizeof(common_anim_frame_t) * (anim->frame_num+1));
      anim->frame_list[anim->frame_num].tnum = 0;
      anim->frame_list[anim->frame_num].tlist = NULL;

    } else if(!strcmp("camera", token)) {

      /* Position */
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].pos.v[0] = atof(token);
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].pos.v[1] = atof(token);
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].pos.v[2] = atof(token);

      /* Focus Point */
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].focus.v[0] = atof(token);
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].focus.v[1] = atof(token);
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].focus.v[2] = atof(token);

      /* Tilt */
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].tilt = atof(token);

      /* Field of View */
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].fov = atof(token);

      /* Depth of Field */
      token = strtok(NULL, ",");
      anim->frame_list[anim->frame_num].dof = atof(token);

    } else if(!strcmp("transform_mesh", token)) {

      int i;

      anim->frame_list[anim->frame_num].tlist = (common_anim_transform_t *)realloc(anim->frame_list[anim->frame_num].tlist, sizeof(common_anim_transform_t) * (anim->frame_list[anim->frame_num].tnum+1));


      /* Mesh Name */
      token = strtok(NULL, ",");
      strcpy(anim->frame_list[anim->frame_num].tlist[anim->frame_list[anim->frame_num].tnum].mesh_name, token);


      /* Matrix */
      for(i = 0; i < 16; i++) {
	token = strtok(NULL, ",");
	anim->frame_list[anim->frame_num].tlist[anim->frame_list[anim->frame_num].tnum].matrix[i] = atof(token);
      }

      anim->frame_list[anim->frame_num].tnum++;
    }
  }

  anim->frame_num++;

  fclose(fh);

  return(0);
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
