#include <tcl.h>
#include "hv.h"

int Et_AppInit(Tcl_Interp *interp){
  extern int Tkhtml_Init(Tcl_Interp*);
  return Tkhtml_Init(interp);
}
