#ifndef SEEN_CMD_H
#define SEEN_CMD_H

#define MAXARGS 9000

struct cmdtab {
  char *ct_name;
  int (*ct_func)();
};

extern int do_cmd();

#endif /* SEEN_CMD_H */
