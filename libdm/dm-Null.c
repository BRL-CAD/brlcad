#include "conf.h"
#include "tcl.h"

#define PLOTBOUND       1000.0  /* Max magnification in Rot matrix */
#include <stdio.h>
#include <sys/time.h>
#include "machine.h"
#include "bu.h"
#include "dm.h"
#include "dm-Null.h"

static void	Nu_input();
void	Nu_void();
int	Nu_int0();
struct dm *Nu_open();
unsigned Nu_unsign();

struct dm dm_Null = {
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,
  0,			/* no displaylist */
  0,                    /* no stereo */
  PLOTBOUND,
  "nu",
  "Null Display",
  DM_TYPE_NULL,
  0,
  0,
  0,
  0,
  0,
  0,
  0,
  {0, 0},
  0,
  0,
  0
};

int Nu_int0() { return TCL_OK; }
void Nu_void() { ; }
struct dm *Nu_open(){ return DM_NULL; }
unsigned Nu_unsign() { return TCL_OK; }
