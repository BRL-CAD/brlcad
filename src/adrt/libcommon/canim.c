/*
 * $Id$
 */

#include "canim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tie.h"
#include "umath.h"


int common_anim_read(common_anim_t *anim);

int common_anim_read(common_anim_t *anim) {
  FILE *fh;
  char line[256], *token;

  fh = fopen("frame.db", "r");

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
