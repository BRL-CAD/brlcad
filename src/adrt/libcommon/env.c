#include "env.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "render.h"


void common_env_init(common_env_t *env);
void common_env_free(common_env_t *env);
void common_env_prep(common_env_t *env);
void common_env_read(common_env_t *env);


void common_env_init(common_env_t *env) {
  render_flat_init(&env->render);
  env->img_w = 640;
  env->img_h = 480;
  env->img_hs = 1;
  env->tile_w = 40;
  env->tile_h = 40;
}


void common_env_free(common_env_t *env) {
}


void common_env_prep(common_env_t *env) {
  env->img_vw = env->img_w * env->img_hs;
  env->img_vh = env->img_h * env->img_hs;
}


void common_env_read(common_env_t *env) {
  FILE *fh;
  char line[256], *token;

  fh = fopen("env.db", "r");

  while(fgets(line, 256, fh)) {
    token = strtok(line, ",");

    if(!strcmp("image_size", token)) {
      token = strtok(NULL, ",");
      env->img_w = atoi(token);
      token = strtok(NULL, ",");
      env->img_h = atoi(token);
      token = strtok(NULL, ",");
      env->tile_w = atoi(token);
      token = strtok(NULL, ",");
      env->tile_h = atoi(token);
    } else if(!strcmp("hypersamples", token)) {
      token = strtok(NULL, ",");
      env->img_hs = atoi(token);
    } else if(!strcmp("rendering_method", token)) {
      token = strtok(NULL, ",");
      /* strip off newline */
      if(token[strlen(token)-1] == '\n') token[strlen(token)-1] = 0;

      if(!strcmp(token, "normal")) {
        env->rm = RENDER_METHOD_NORMAL;
        render_normal_init(&env->render);
      } else if(!strcmp(token, "phong")) {
        env->rm = RENDER_METHOD_PHONG;
        render_phong_init(&env->render);
      } else if(!strcmp(token, "path")) {
        env->rm = RENDER_METHOD_PATH;
        token = strtok(NULL, ",");
        render_path_init(&env->render, atoi(token));
      } else if(!strcmp(token, "kelos")) {
        env->rm = RENDER_METHOD_KELOS;
        render_kelos_init(&env->render);
      } else if(!strcmp(token, "plane")) {
        TIE_3 pos, dir;
        int i;

        env->rm = RENDER_METHOD_PLANE;

        /* ray position */
        for(i = 0; i < 3; i++) {
          token = strtok(NULL, ",");
          pos.v[i] = atof(token);
        }

        /* ray direction */
        for(i = 0; i < 3; i++) {
          token = strtok(NULL, ",");
          dir.v[i] = atof(token);
        }

        render_plane_init(&env->render, pos, dir);
      } else {
        env->rm = RENDER_METHOD_FLAT;
        render_flat_init(&env->render);
      }
      
    }
  }

  fclose(fh);
}
