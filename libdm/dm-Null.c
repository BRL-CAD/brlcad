#include "conf.h"
#include "tcl.h"

#include <stdio.h>
#include <sys/time.h>
#include "machine.h"
#include "bu.h"
#include "dm.h"
#include "dm-Null.h"

static void	Nu_input();
void	Nu_void();
int	Nu_int0();
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
  Nu_unsign,
  Nu_unsign,
  Nu_int0,
  Nu_int0,
  Nu_int0,
  0,			/* no displaylist */
  0.0,
  "nu", "Null Display",
  0,
  0,
  0,
  0,
  0
};

int Nu_int0() { return TCL_OK; }
void Nu_void() { ; }
unsigned Nu_unsign() { return TCL_OK; }
