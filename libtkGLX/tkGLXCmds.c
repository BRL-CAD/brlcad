#include <stdio.h>
#include <string.h>
#include <tcl.h>
#include <gl/gl.h>
#include "descnames.h"


  /* syntax:
     glx_getgdesc ?-screen screen_num? param_name
     glx_getgdesc ?-screen screen_num? param_number

     glx_viewport width height
     glx_viewport left right bottom top
     */

static int TkGLX_getgdescCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));

static int TkGLX_viewportCmd _ANSI_ARGS_((ClientData clientData,
		    Tcl_Interp *interp, int argc, char **argv));


TkGLX_CmdInit(interp)
     Tcl_Interp *interp;
{
  Tcl_CreateCommand(interp, "glx_getgdesc", TkGLX_getgdescCmd,
		    (ClientData) NULL, (void (*)()) NULL);

  Tcl_CreateCommand(interp, "glx_viewport", TkGLX_viewportCmd,
		    (ClientData) NULL, (void (*)()) NULL);
}

  
static int TkGLX_getgdescCmd(clientData, interp, argc, argv)
     ClientData clientData;	/* Main window associated with
				 * interpreter. */
     Tcl_Interp *interp;	/* Current interpreter. */
     int argc;			/* Number of arguments. */
     char **argv;		/* Argument strings. */
{
  int screen_num, save_num  = -1;
  struct _tkglx_gldesc *p;
  int ret = TCL_OK;
  char *sp, *np, *s;
  char ns[1024];
  int i, val;
  char str[1024];

  if(argc >= 2 && strcmp(argv[1], "-screen") == 0){
    screen_num = atoi(argv[2]);

    if(screen_num >= getgdesc(GD_NSCRNS)){
      Tcl_AppendResult(interp, "invalid screen number", (char *)NULL);
      return(TCL_ERROR);
    }

    save_num = getwscrn();
    scrnselect(screen_num);
    argc -= 2; argv += 2;
  }

  if(argc == 1){
    for(p = TkGLX_gldesc; p->name != (char *)NULL; p++){
      sprintf(str, "%s %d", p->name, getgdesc(p->value));

#if TCL_MAJOR_VERSION >= 7
      Tcl_AppendElement(interp, str);
#else
      Tcl_AppendElement(interp, str, 0);
#endif
    }
  }
  else {
    for(i = 1; i < argc; i++){
      s = argv[i];

      val = -1000;

      /* check to see if the input is numerical */
      if(sscanf(s, "%d", &val) != 1){

	/* if not, proceed with string matching */
	np = ns;
	if(strlen(s) < 3 || strncasecmp(s, "GD_", 3) != 0){
	  *np++ = 'G';
	  *np++ = 'D';
	  *np++ = '_';
	}
	
	for(sp = s; *sp != '\0'; sp++){
	  if(isspace(*sp)) 
	    continue;
	  *np++ = toupper(*sp);
	}
	*np = '\0';
	
	for(p = TkGLX_gldesc; p->name != (char *)NULL; p++){
	  if(strcasecmp(p->name, ns) == 0){
	    val = p->value;
	    break;
	  }
	}
      }

      if(val == -1000){
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "parameter \"", s, "\" (", 
			 ns, ") matches no gl parameter names", 
			 (char *)NULL);
	ret = TCL_ERROR;
	goto cleanup;
      }
      
      sprintf(str, "%d", getgdesc(val));

#if TCL_MAJOR_VERSION >= 7
      Tcl_AppendElement(interp, str);
#else
      Tcl_AppendElement(interp, str, 0);
#endif
    }
  }

 cleanup:

  if(save_num != -1){
    scrnselect(save_num);
  }

  return(ret);
}

static int TkGLX_viewportCmd(clientData, interp, argc, argv)
     ClientData clientData;	/* Main window associated with
				 * interpreter. */
     Tcl_Interp *interp;	/* Current interpreter. */
     int argc;			/* Number of arguments. */
     char **argv;		/* Argument strings. */
{
  Screencoord left, right, bottom, top;

  if(argc != 3 && argc != 5){
    Tcl_AppendResult(interp, "wrong # args: should be: \"",
		     argv[0], " width height\" or \"", 
		     argv[0], "left right bottom top\"",
		     (char *)NULL);
    return(TCL_ERROR);
  }

  if(argc == 3){
    left = 0;
    right  = atoi(argv[1]) - 1;
    bottom = 0;
    top    = atoi(argv[2]) - 1;
  }
  else {
    left   = atoi(argv[1]);
    right  = atoi(argv[2]);
    bottom = atoi(argv[3]);
    top    = atoi(argv[4]);
  }

  viewport(left, right, bottom, top);
  return(TCL_OK);
}


    
