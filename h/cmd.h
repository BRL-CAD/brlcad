#ifndef SEEN_CMD_H
#define SEEN_CMD_H

#include <sys/time.h>
#include <time.h>
#include "bu.h"

#define MAXARGS 9000
#define CMD_NULL (int (*)())NULL

struct cmdtab {
  char *ct_name;
  int (*ct_func)();
};

struct cmdhist {
  struct bu_list l;
  struct bu_vls h_command;
  struct timeval h_start;
  struct timeval h_finish;
  int h_status;
};

extern int do_cmd();
extern void register_cmds();

#endif /* SEEN_CMD_H */
