#include "dispatcher.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "adrt_common.h"
#include "brlcad_config.h"
#include "ivat.h"
#include "tienet.h"

#include <unistd.h>

#ifdef HAVE_SYS_SYSINFO_H
#include <sys/sysinfo.h>
#elif defined(HAVE_SYS_SYSCTL_H)
#include <sys/sysctl.h>
#endif


int	ivat_dispatcher_progress_delta;
int	ivat_dispatcher_progress;
int	ivat_dispatcher_interval;
int	ivat_dispatcher_lastsave;


void	ivat_dispatcher_init(void);
void	ivat_dispatcher_free(void);
void	ivat_dispatcher_generate(common_db_t *db, void *data, int data_len);
void	ivat_dispatcher_result(void *res_buf, int res_len);


void ivat_dispatcher_init() {
}


void ivat_dispatcher_free() {
}


void ivat_dispatcher_generate(common_db_t *db, void *data, int data_len) {
  int i, n;
  common_work_t work;
  void *mesg;

  mesg = malloc(sizeof(common_work_t) + data_len);
  tienet_master_begin();

  work.size_x = db->env.tile_w;
  work.size_y = db->env.tile_h;
  work.format = COMMON_BIT_DEPTH_24;

  memcpy(&((char *)mesg)[sizeof(common_work_t)], data, data_len);
  for(i = 0; i < db->env.img_vh; i += db->env.tile_w) {
    for(n = 0; n < db->env.img_vw; n += db->env.tile_h) {
      work.orig_x = n;
      work.orig_y = i;
      memcpy(&((char *)mesg)[0], &work, sizeof(common_work_t));
      tienet_master_push(mesg, sizeof(common_work_t)+data_len);
    }
  }

  tienet_master_end();
  free(mesg);
}
