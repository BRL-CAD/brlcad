#ifndef SEEN_CMD_H
#define SEEN_CMD_H

#ifdef USE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <time.h>
#include "bu.h"

#define MAXARGS 9000
#define CMD_NULL (int (*)())NULL
#define CMDHIST_NULL (struct bu_cmdhist *)NULL
#define CMDHIST_OBJ_NULL (struct bu_cmdhist_obj *)NULL

struct bu_cmdhist {
  struct bu_list l;
  struct bu_vls h_command;
  struct timeval h_start;
  struct timeval h_finish;
  int h_status;
};

struct bu_cmdhist_obj {
  struct bu_list l;
  struct bu_vls cho_name;
  struct bu_cmdhist cho_head;
  struct bu_cmdhist *cho_curr;
};

extern int bu_cmd();
extern void bu_register_cmds();

#endif /* SEEN_CMD_H */
