
#ifndef __SWPROC_H__
#define __SWPROC_H__

#define SWPROC_END     0
#define SWPROC_ARG     1
#define SWPROC_OPT     2
#define SWPROC_SWITCH  3

struct SwprocConf {
  int eType;
  const char *zSwitch;
  const char *zDefault;
  const char *zTrue;
};
typedef struct SwprocConf SwprocConf;

int SwprocRt(Tcl_Interp *, int, Tcl_Obj *CONST[], SwprocConf *, Tcl_Obj **);
void SwprocCleanup(Tcl_Obj **, int);

int SwprocInit(Tcl_Interp *);

#endif
