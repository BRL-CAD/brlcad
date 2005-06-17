#include "slave.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "camera.h"
#include "igvt.h"
#include "cdb.h"
#include "tienet.h"
#include "unpack.h"

void igvt_slave(int port, char *host, int threads);
void igvt_slave_init(tie_t *tie, void *app_data, int app_size);
void igvt_slave_free(void);
void igvt_slave_work(tie_t *tie, void *data, int size, void **res_buf, int *res_len);
void igvt_slave_mesg(void *mesg, int mesg_len);


int igvt_slave_threads;
int igvt_slave_completed;
common_db_t db;
util_camera_t camera;


void igvt_slave(int port, char *host, int threads) {
  igvt_slave_threads = threads;
  tienet_slave_init(port, host, igvt_slave_init, igvt_slave_work, igvt_slave_free, igvt_slave_mesg, IGVT_VER_KEY);
}


void igvt_slave_init(tie_t *tie, void *app_data, int app_size) {
  printf("scene data received\n");

  igvt_slave_completed = 0;
  util_camera_init(&camera, igvt_slave_threads);

  common_unpack(&db, tie, &camera, COMMON_PACK_ALL, app_data, app_size);
  common_env_prep(&db.env);
  util_camera_prep(&camera, &db);
  printf("prepping geometry\n");
}


void igvt_slave_free() {
  util_camera_free(&camera);
  common_unpack_free();
}


void igvt_slave_work(tie_t *tie, void *data, int size, void **res_buf, int *res_len) {
  common_work_t work;
  int ind;
  short frame_ind;
  TIE_3 pos, foc;
  unsigned char rm;


  ind = 0;
  memcpy(&work, &((char *)data)[0], sizeof(common_work_t));
  ind += sizeof(common_work_t);


  if(work.size_x == 0) {
    tie_ray_t ray;
    void *mesg;
    int dlen;


    mesg = NULL;

    /* position */
    memcpy(&ray.pos.v[0], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&ray.pos.v[1], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&ray.pos.v[2], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);

    /* position */
    memcpy(&ray.dir.v[0], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&ray.dir.v[1], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&ray.dir.v[2], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);

    /* Fire the shotline */
    ray.depth = 0;
    render_shotline_work(tie, &ray, &mesg, &dlen);

    /* Make room for shotline data */
    *res_len = sizeof(common_work_t) + dlen;
    *res_buf = (void *)realloc(*res_buf, *res_len);

    ind = 0;

    /* Pack work unit data and shotline data */
    memcpy(&((char *)*res_buf)[ind], &work, sizeof(common_work_t));
    ind += sizeof(common_work_t);

    memcpy(&((char *)*res_buf)[ind], mesg, dlen);

    free(mesg);
  } else {
    /* Extract updated camera data */
    memcpy(&frame_ind, &((char *)data)[ind], sizeof(short));
    ind += sizeof(short);

    /* Camera position */
    memcpy(&pos.v[0], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&pos.v[1], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&pos.v[2], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);

    /* Camera Focus */
    memcpy(&foc.v[0], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&foc.v[1], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);
    memcpy(&foc.v[2], &((char *)data)[ind], sizeof(tfloat));
    ind += sizeof(tfloat);

    /* Update Rendering Method if it has Changed */
    memcpy(&rm, &((char *)data)[ind], 1);
    ind += 1;

    if(rm != db.env.rm) {
      db.env.render.free(&db.env.render);

      switch(rm) {
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
            TIE_3 ray_dir;

            math_vec_sub(ray_dir, foc, pos);
            render_plane_init(&db.env.render, pos, ray_dir);
          }
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

    /* Tack on the frame index data */
    memcpy(&((char *)*res_buf)[*res_len], &frame_ind, sizeof(short));
    *res_len += sizeof(short);
  }


#if 0
  gettimeofday(&tv, NULL);
  printf("[Work Units Completed: %.6d  Rays: %.5d k/sec %lld]\r", ++igvt_slave_completed, (int)((tfloat)tie->rays_fired / (tfloat)(1000 * (tv.tv_sec - igvt_slave_startsec + 1))), tie->rays_fired);
  fflush(stdout);
#endif
}


void igvt_slave_mesg(void *mesg, int mesg_len) {
  short		op;
  TIE_3		foo;

  memcpy(&op, mesg, sizeof(short));

  switch(op) {
    case IGVT_OP_CAMERA:
    {
      TIE_3	pos;
      TIE_3	foc;

      memcpy(&pos.v[0], &((char *)mesg)[2], sizeof(tfloat));
      memcpy(&pos.v[1], &((char *)mesg)[2+1*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&pos.v[2], &((char *)mesg)[2+2*sizeof(tfloat)], sizeof(tfloat));

      memcpy(&foc.v[0], &((char *)mesg)[2+3*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&foc.v[1], &((char *)mesg)[2+4*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&foc.v[2], &((char *)mesg)[2+5*sizeof(tfloat)], sizeof(tfloat));

      camera.pos = pos;
      camera.focus = foc;
      /* recompute the camera data */
      util_camera_prep(&camera, &db);

      break;
    }

    case IGVT_OP_SHOTLINE:
    {
      int i, n, num, ind;
      char c, name[256];

      /* Reset all meshes */
      for(i = 0; i < db.mesh_num; i++) {
        db.mesh_list[i]->flags = 0;
      }

      /* Read the data */
      ind = sizeof(short);
      memcpy(&num, &((unsigned char *)mesg)[ind], sizeof(int));

      ind += sizeof(int);


      for(i = 0; i < num; i++) {
        memcpy(&c, &((unsigned char *)mesg)[ind], 1);
        ind += 1;
        memcpy(name, &((unsigned char *)mesg)[ind], c);
        ind += c;

/*        printf("trying to assign: -%s-\n", name); */
        for(n = 0; n < db.mesh_num; n++) {
          if(!strcmp(db.mesh_list[n]->name, name)) {
/*            printf("  assigned[%d]: %s\n", i, name); */
            db.mesh_list[n]->flags = 1;
          }
        }
      }

      break;
    }

    default:
      break;
  }
}
