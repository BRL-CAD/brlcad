#include "conf.h"
#include "tcl.h"

#define PLOTBOUND       1000.0  /* Max magnification in Rot matrix */
#include <stdio.h>
#include <sys/time.h>
#include "machine.h"
#include "bu.h"
#include "vmath.h"
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
  Nu_int0,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,
  0,				/* no displaylist */
  0,				/* no stereo */
  PLOTBOUND,			/* zoom-in limit */
  1,				/* bound flag */
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
  0,
  0,
  0,
  0,				/* clipmin */
  0,				/* clipmax */
  0,				/* no debugging */
  0,				/* no perspective */
  0,				/* no lighting */
  0,				/* no zbuffer */
  0				/* no zclipping */
};

int Nu_int0() { return TCL_OK; }
void Nu_void() { ; }
struct dm *Nu_open(){ return DM_NULL; }
unsigned Nu_unsign() { return TCL_OK; }
