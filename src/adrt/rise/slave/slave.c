#include "slave.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "rise.h"
#include "camera.h"
#include "cdb.h"
#include "tienet.h"
#include "unpack.h"


void rise_slave(int port, char *host, int threads);
void rise_slave_init(tie_t *tie, int socknum);
void rise_slave_free(void);
void rise_slave_work(tie_t *tie, void *data, int size, void **res_buf, int *res_len);
void rise_slave_mesg(void *mesg, int mesg_len);

int rise_slave_threads;
int rise_slave_completed;
common_db_t db;
util_camera_t camera;


void rise_slave(int port, char *host, int threads) {
  rise_slave_threads = threads;
  tienet_slave_init(port, host, rise_slave_init, rise_slave_work, rise_slave_free, rise_slave_mesg, RISE_VER_KEY);
}


void rise_slave_init(tie_t *tie, int socknum) {
  printf("scene data received\n");

  rise_slave_completed = 0;
  util_camera_init(&camera, rise_slave_threads);

  printf("prepping geometry... ");
  fflush(stdout);
  common_unpack(&db, tie, &camera, socknum);
  common_env_prep(&db.env);
  util_camera_prep(&camera, &db);
  printf("done.\n");
}


void rise_slave_free() {
  util_camera_free(&camera);
  common_unpack_free();
}


void rise_slave_work(tie_t *tie, void *data, int size, void **res_buf, int *res_len) {
  util_camera_render(&camera, &db, tie, data, size, res_buf, res_len);

#if 0
  gettimeofday(&tv, NULL);
  printf("[Work Units Completed: %.6d  Rays: %.5d k/sec %lld]\r", ++rise_slave_completed, (int)((tfloat)tie->rays_fired / (tfloat)(1000 * (tv.tv_sec - rise_slave_startsec + 1))), tie->rays_fired);
  fflush(stdout);
#endif
}


void rise_slave_mesg(void *mesg, int mesg_len) {
  short		op;

  memcpy(&op, mesg, sizeof(short));

  switch(op) {
    case RISE_OP_CAMERA:
    {
      TIE_3	pos;
      TIE_3	foc;

      memcpy(&pos.v[0], &((char*)mesg)[2], sizeof(tfloat));
      memcpy(&pos.v[1], &((char*)mesg)[2+1*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&pos.v[2], &((char*)mesg)[2+2*sizeof(tfloat)], sizeof(tfloat));

      memcpy(&foc.v[0], &((char*)mesg)[2+3*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&foc.v[1], &((char*)mesg)[2+4*sizeof(tfloat)], sizeof(tfloat));
      memcpy(&foc.v[2], &((char*)mesg)[2+5*sizeof(tfloat)], sizeof(tfloat));

      camera.pos = pos;
      camera.focus = foc;
      /* recompute the camera data */
      util_camera_prep(&camera, &db);

      break;
    }

    default:
      break;
  }
}
