#include "conf.h"
#include "tcl.h"

#define PLOTBOUND       1000.0  /* Max magnification in Rot matrix */
#include <stdio.h>
#ifndef WIN32
#include <sys/time.h>
#endif
#include "machine.h"
#include "bu.h"
#include "vmath.h"
#include "dm.h"
#include "dm-Null.h"

void	Nu_void(void);
int	Nu_int0(void);
struct dm *Nu_open(void);
unsigned Nu_unsign(void);
static int     Nu_fg(struct dm *, unsigned char, unsigned char, unsigned char, int strict);
static int     Nu_bg(struct dm *, unsigned char, unsigned char, unsigned char);

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
  Nu_fg,
  Nu_bg,
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
  {0, 0, 0, 0, 0},		/* bu_vls path name*/
  {0, 0, 0, 0, 0},		/* bu_vls full name drawing window */
  {0, 0, 0, 0, 0},		/* bu_vls short name drawing window */
  {0, 0, 0},			/* bg color */
  {0, 0, 0},			/* fg color */
  {0.0, 0.0, 0.0},		/* clipmin */
  {0.0, 0.0, 0.0},		/* clipmax */
  0,				/* no debugging */
  0,				/* no perspective */
  0,				/* no lighting */
  0,				/* no transparency */
  0,				/* no zbuffer */
  0,				/* no zclipping */
  1,                            /* clear back buffer after drawing and swap */
  0				/* Tcl interpreter */
};

int Nu_int0(void) { return TCL_OK; }
void Nu_void(void) { ; }
struct dm *Nu_open(void) { return DM_NULL; }
unsigned Nu_unsign(void) { return TCL_OK; }
static int Nu_fg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b, int strict) { return TCL_OK; }
static int Nu_bg(struct dm *dmp, unsigned char r, unsigned char g, unsigned char b) { return TCL_OK; }
