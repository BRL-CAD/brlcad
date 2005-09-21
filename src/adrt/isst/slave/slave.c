#include "slave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "camera.h"
#include "isst.h"
#include "cdb.h"
#include "tienet.h"
#include "unpack.h"
#include "render_util.h"


void isst_slave(int port, char *host, int threads);
void isst_slave_init(tie_t *tie, int socknum);
void isst_slave_free(void);
void isst_slave_work(tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len);
void isst_slave_mesg(void *mesg, unsigned int mesg_len);


int isst_slave_threads;
int isst_slave_completed;
common_db_t db;
util_camera_t camera;


void isst_slave(int port, char *host, int threads) {
  isst_slave_threads = threads;
  tienet_slave_init(port, host, isst_slave_init, isst_slave_work, isst_slave_free, isst_slave_mesg, ISST_VER_KEY);
}


void isst_slave_init(tie_t *tie, int socknum) {
  printf("scene data received\n");

  isst_slave_completed = 0;
  util_camera_init(&camera, isst_slave_threads);

  printf("prepping geometry... ");
  fflush(stdout);
  common_unpack(&db, tie, &camera, socknum);
  common_env_prep(&db.env);
  util_camera_prep(&camera, &db);

  printf("done.\n");
}


void isst_slave_free() {
  util_camera_free(&camera);
  common_unpack_free();
}


void isst_slave_work(tie_t *tie, void *data, unsigned int size, void **res_buf, unsigned int *res_len) {
  common_work_t work;
  int ind;
  TIE_3 pos, foc;
  unsigned char rm;
  char op;


  ind = 0;
  memcpy(&work, &((char *)data)[ind], sizeof(common_work_t));
  ind += sizeof(common_work_t);

  memcpy(&op, &((char *)data)[ind], 1);
  ind += 1;

  switch(op) {
    case ISST_OP_SHOT:
      {
        tie_ray_t ray;
        void *mesg;
        int dlen;

        mesg = NULL;

        /* position */
        memcpy(&ray.pos, &((char *)data)[ind], sizeof(TIE_3));
        ind += sizeof(TIE_3);

        /* direction */
        memcpy(&ray.dir, &((char *)data)[ind], sizeof(TIE_3));
        ind += sizeof(TIE_3);

        /* Fire the shot */
        ray.depth = 0;
        render_util_shotline_list(tie, &ray, &mesg, &dlen);

        /* Make room for shot data */
        *res_len = sizeof(common_work_t) + dlen;
        *res_buf = (void *)realloc(*res_buf, *res_len);

        ind = 0;

        /* Pack work unit data and shot data */
        memcpy(&((char *)*res_buf)[ind], &work, sizeof(common_work_t));
        ind += sizeof(common_work_t);

        memcpy(&((char *)*res_buf)[ind], mesg, dlen);

        free(mesg);
      }
      break;

    case ISST_OP_SPALL:
      {
        tie_ray_t ray;
        tfloat angle;
        void *mesg;
        int dlen;

        mesg = NULL;

        /* position */
        memcpy(&ray.pos, &((char *)data)[ind], sizeof(TIE_3));
        ind += sizeof(TIE_3);

        /* direction */
        memcpy(&ray.dir, &((char *)data)[ind], sizeof(TIE_3));
        ind += sizeof(TIE_3);

        /* angle */
        memcpy(&angle, &((char *)data)[ind], sizeof(tfloat));
        ind += sizeof(tfloat);

        /* Fire the shot */
        ray.depth = 0;
        render_util_spall_list(tie, &ray, angle, &mesg, &dlen);

        /* Make room for shot data */
        *res_len = sizeof(common_work_t) + dlen;
        *res_buf = (void *)realloc(*res_buf, *res_len);

        ind = 0;

        /* Pack work unit data and shot data */
        memcpy(&((char *)*res_buf)[ind], &work, sizeof(common_work_t));
        ind += sizeof(common_work_t);

        memcpy(&((char *)*res_buf)[ind], mesg, dlen);

        free(mesg);
      }
      break;

    case ISST_OP_RENDER:
      /* Camera position */
      memcpy(&pos.v, &((char *)data)[ind], sizeof(TIE_3));
      ind += sizeof(TIE_3);

      /* Camera Focus */
      memcpy(&foc.v, &((char *)data)[ind], sizeof(TIE_3));
      ind += sizeof(TIE_3);

      /* Update Rendering Method if it has Changed */
      memcpy(&rm, &((char *)data)[ind], 1);
      ind += 1;

      if(rm != db.env.rm) {
        db.env.render.free(&db.env.render);

        switch(rm) {
          case RENDER_METHOD_DEPTH:
            render_depth_init(&db.env.render);
            break;

          case RENDER_METHOD_COMPONENT:
            render_component_init(&db.env.render);
            break;

          case RENDER_METHOD_GRID:
            render_grid_init(&db.env.render);
            break;

          case RENDER_METHOD_NORMAL:
            render_normal_init(&db.env.render);
            break;

          case RENDER_METHOD_PATH:
            render_path_init(&db.env.render, 12);
            break;

          case RENDER_METHOD_PHONG:
            render_phong_init(&db.env.render);
            break;

          case RENDER_METHOD_PLANE:
            {
              TIE_3 shot_pos, shot_dir;

              /* Extract shot position and direction */
              memcpy(&shot_pos, &((char *)data)[ind], sizeof(TIE_3));
              ind += sizeof(TIE_3);

              memcpy(&shot_dir, &((char *)data)[ind], sizeof(TIE_3));
              ind += sizeof(TIE_3);

              render_plane_init(&db.env.render, shot_pos, shot_dir);
            }
            break;

          case RENDER_METHOD_SPALL:
            {
              TIE_3 shot_pos, shot_dir;
              tfloat angle;

              /* Extract shot position and direction */
              memcpy(&shot_pos, &((char *)data)[ind], sizeof(TIE_3));
              ind += sizeof(TIE_3);

              memcpy(&shot_dir, &((char *)data)[ind], sizeof(TIE_3));
              ind += sizeof(TIE_3);

              memcpy(&angle, &((char *)data)[ind], sizeof(tfloat));
              ind += sizeof(tfloat);

              render_spall_init(&db.env.render, shot_pos, shot_dir, angle); /* 10 degrees for now */
            }
            break;

          default:
            break;
        }
        db.env.rm = rm;
      }

      /* Update camera */
      camera.pos = pos;
      camera.focus = foc;
      util_camera_prep(&camera, &db);

      util_camera_render(&camera, &db, tie, data, size, res_buf, res_len);
      *res_buf = (void *)realloc(*res_buf, *res_len + sizeof(short));
      break;

    default:
      break;
  }

#if 0
  gettimeofday(&tv, NULL);
  printf("[Work Units Completed: %.6d  Rays: %.5d k/sec %lld]\r", ++isst_slave_completed, (int)((tfloat)tie->rays_fired / (tfloat)(1000 * (tv.tv_sec - isst_slave_startsec + 1))), tie->rays_fired);
  fflush(stdout);
#endif
}


void isst_slave_mesg(void *mesg, unsigned int mesg_len) {
  short		op;
  TIE_3		foo;

  memcpy(&op, mesg, sizeof(short));

  switch(op) {
    case ISST_OP_SHOT:
    {
      int i, n, num, ind;
      char name[256];
      unsigned char c;

      /* Reset all meshes hit flag */
      for(i = 0; i < db.mesh_num; i++)
        db.mesh_list[i]->flags &= 0x2;

      /* Read the data */
      ind = sizeof(short);
      memcpy(&num, &((unsigned char *)mesg)[ind], sizeof(int));

      ind += sizeof(int);

      for(i = 0; i < num; i++) {
        memcpy(&c, &((unsigned char *)mesg)[ind], 1);
        ind += 1;
        memcpy(name, &((unsigned char *)mesg)[ind], c);
        ind += c;

        /* set hit flag */
        for(n = 0; n < db.mesh_num; n++) {
          if(!strcmp(db.mesh_list[n]->name, name)) {
            db.mesh_list[n]->flags |= 1;
            continue;
          }
        }
      }
    }
    break;

    case ISST_OP_SELECT:
    {
      uint8_t c, t;
      char string[256];
      uint32_t n;

      /* select or deslect */
      memcpy(&t, &((uint8_t *)mesg)[2], 1);
      /* string */
      memcpy(&c, &((uint8_t *)mesg)[3], 1);
      memcpy(string, &((uint8_t *)mesg)[4], c);

      /* set select flag */
      for(n = 0; n < db.mesh_num; n++)
        if(strstr(db.mesh_list[n]->name, string) || c == 1)
          db.mesh_list[n]->flags = (db.mesh_list[n]->flags & 0x1) | t<<1;
    }
    break;

    default:
      break;
  }
}
