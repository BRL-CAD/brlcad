#include <stdio.h>
#include <tk.h>

TkGLX_Init(interp, w)
     Tcl_Interp *interp;
     Tk_Window w;
{
  TkGLX_CmdInit(interp);
  TkGLX_WidgetInit(interp, w);
}

