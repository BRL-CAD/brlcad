/*
 *                        D O E V E N T . C
 *
 * An event handler for MGED.
 *
 * Author -
 *     Robert G. Parker
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1988 by the United States Army.
 *	All rights reserved.
 */

#include "conf.h"
#include "tk.h"
#include "machine.h"
#include "externs.h"
#include "bu.h"
#include "vmath.h"
#include "./mged_dm.h"

int
doEvent(clientData, eventPtr)
ClientData clientData;
XEvent *eventPtr;
{
  register struct dm_list *save_dm_list;
  int status;

  if(eventPtr->type == DestroyNotify)
    return TCL_OK;

  save_dm_list = curr_dm_list;
  GET_DM_LIST(curr_dm_list, (unsigned long)eventPtr->xany.window);

  /* it's an event for a window that I'm not handling */
  if(curr_dm_list == DM_LIST_NULL){
    curr_dm_list = save_dm_list;
    return TCL_OK;
  }

  /* calling the display manager specific event handler */
  status = eventHandler(clientData, eventPtr);
  curr_dm_list = save_dm_list;

  return status;
}
