#ifndef SEEN_CMD_H
#define SEEN_CMD_H

#include <sys/time.h>
#include <time.h>
#include "bu.h"

#define MAXARGS 9000
#define CMD_NULL (int (*)())NULL

struct bu_cmdtab {
  char *ct_name;
  int (*ct_func)();
};

struct bu_cmdhist {
  struct bu_list l;
  struct bu_vls h_command;
  struct timeval h_start;
  struct timeval h_finish;
  int h_status;
};

extern int bu_cmd();
extern void bu_register_cmds();

#endif /* SEEN_CMD_H */
