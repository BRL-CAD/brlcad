/*                        V R L I N K . C
 * BRL-CAD
 *
 * Copyright (c) 1992-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file vrlink.c
 *
 *  Author -
 *	Michael John Muuss
 *
 *  Source -
 *	SECAD/VLD Computing Consortium
 *	The U. S. Army Ballistic Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5066
 */
#ifndef lint
static const char RCSid[] = "@(#)$Header$ (BRL)";
#endif

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "tcl.h"

#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "bu.h"
#include "raytrace.h"
#include "pkg.h"
#include "./ged.h"
#include "./mged_dm.h"

extern int		(*cmdline_hook)();	/* cmd.c */

#if 0
extern point_t		eye_pos_scr;		/* dozoom.c */
#endif

static struct pkg_conn	*vrmgr;			/* PKG connection to VR mgr */
static char		*vr_host = "None";	/* host running VR mgr */
static char		*tcp_port = "5555";	/* "gedd", remote mged */

#define VRMSG_ROLE	1	/* from MGED: Identify role of machine */
#define VRMSG_CMD	2	/* to MGED: Command to process */
#define VRMSG_EVENT	3	/* from MGED: device event */
#define VRMSG_POV	4	/* from MGED: point of view info */
#define VRMSG_VLIST	5	/* transfer binary vlist block */
#define VRMSG_CMD_REPLY	6	/* from MGED: reply to VRMSG_CMD */

void	ph_cmd(register struct pkg_conn *pc, char *buf);
void	ph_vlist(register struct pkg_conn *pc, char *buf);
static struct pkg_switch pkgswitch[] = {
	{ VRMSG_CMD,		ph_cmd,		"Command" },
	{ VRMSG_VLIST,		ph_vlist,	"Import vlist" },
	{ 0,			0,		(char *)0 }
};

#if 0
/*
 *			P K G _ S E N D _ V L S
 */
int
pkg_send_vls( type, vp, pc )
int		type;
struct bu_vls	*vp;
struct pkg_conn	*pc;
{
	BU_CK_VLS(vp);
	if(!pc)  {
		bu_log("pkg_send_vls:  NULL pointer\n");
		return -1;
	}
	return pkg_send( type, bu_vls_addr(vp), bu_vls_strlen(vp)+1, pc );
}
#endif

/*
 *  Called from cmdline() for now.
 *  Returns -
 *	!0	Print prompt for user.  Should always be this.
 *	 0	Don't print a prompt
 */
int
vr_event_hook(struct bu_vls *vp)
{
	BU_CK_VLS(vp);

	if( vrmgr == PKC_NULL )  {
		cmdline_hook = 0;	/* Relinquish this hook */
		return 1;
	}

	if( pkg_send_vls( VRMSG_EVENT, vp, vrmgr ) < 0 )  {
		bu_log("event: pkg_send VRMSG_EVENT failed, disconnecting\n");
		pkg_close(vrmgr);
		vrmgr = PKC_NULL;
		cmdline_hook = 0;	/* Relinquish this hook */
	}
	return 1;
}

/*
 *  Called from the Tk event handler
 */
void
vr_input_hook(void)
{
	int	val;

	val = pkg_suckin(vrmgr);
	if( val < 0 ) {
		bu_log("pkg_suckin() error\n");
	} else if( val == 0 )  {
		bu_log("vrmgr sent us an EOF\n");
	}
	if( val <= 0 )  {
		Tcl_DeleteFileHandler(vrmgr->pkc_fd);
		pkg_close(vrmgr);
		vrmgr = PKC_NULL;
		return;
	}
	if( pkg_process( vrmgr ) < 0 )
		bu_log("vrmgr:  pkg_process error encountered\n");
}

/*
 *  Called from ged.c refresh().
 */
void
vr_viewpoint_hook(void)
{
	struct bu_vls	str;
	static struct bu_vls	old_str;
	quat_t		orient;

	if( vrmgr == PKC_NULL )  {
		cmdline_hook = 0;	/* Relinquish this hook */
		return;
	}

	bu_vls_init_if_uninit(&old_str);
	bu_vls_init(&str);

	quat_mat2quat(orient, view_state->vs_vop->vo_rotation);

	/* Need to send current viewpoint to VR mgr */
	/* XXX more will be needed */
	/* Eye point, quaturnion for orientation */
	bu_vls_printf(&str, "pov {%e %e %e}   {%e %e %e %e}   %e   {%e %e %e}  %e\n",
		      -view_state->vs_vop->vo_center[MDX],
		      -view_state->vs_vop->vo_center[MDY],
		      -view_state->vs_vop->vo_center[MDZ],
		      V4ARGS(orient),
		      view_state->vs_vop->vo_scale,
		      V3ARGS(view_state->vs_vop->vo_eye_pos),
		      view_state->vs_vop->vo_perspective);

	if( strcmp( bu_vls_addr(&old_str), bu_vls_addr(&str) ) == 0 )  {
		bu_vls_free( &str );
		return;
	}

	if( pkg_send_vls( VRMSG_POV, &str, vrmgr ) < 0 )  {
		bu_log("viewpoint: pkg_send VRMSG_POV failed, disconnecting\n");
		pkg_close(vrmgr);
		vrmgr = PKC_NULL;
		viewpoint_hook = 0;	/* Relinquish this hook */
	}
	bu_vls_trunc( &old_str, 0 );
	bu_vls_vlscat( &old_str, &str );
	bu_vls_free( &str );
}


/*
 *			F _ V R M G R
 *
 *  Establish a network link to the VR manager, using libpkg.
 *
 *  Syntax:  vrmgr host role
 */
int
f_vrmgr(ClientData clientData, Tcl_Interp *interp, int argc, char **argv)
{
	struct bu_vls	str;
	char		*role;

	if(argc < 3){
	  struct bu_vls vls;

	  bu_vls_init(&vls);
	  bu_vls_printf(&vls, "help vrmgr");
	  Tcl_Eval(interp, bu_vls_addr(&vls));
	  bu_vls_free(&vls);
	  return TCL_ERROR;
	}

	bu_vls_init(&str);

	if( vrmgr != PKC_NULL )  {
	  Tcl_AppendResult(interp, "Closing link to VRmgr ",
			   vr_host, "\n", (char *)NULL);
		pkg_close( vrmgr );
		vrmgr = PKC_NULL;
		vr_host = "none";
	}

	vr_host = bu_strdup(argv[1]);
	role = argv[2];

	if( strcmp( role, "master" ) == 0 )  {
	} else if( strcmp( role, "slave" ) == 0 )  {
	} else if( strcmp( role, "overview" ) == 0 )  {
	} else {
	   Tcl_AppendResult(interp, "role '", role, "' unknown, must be master/slave/overview\n",
			    (char *)NULL);
	   return TCL_ERROR;
	}

	vrmgr = pkg_open( vr_host, tcp_port, "tcp", "", "",
		pkgswitch, NULL );
	if( vrmgr == PKC_ERROR )  {
	  Tcl_AppendResult(interp, "mged/f_vrmgr: unable to contact ", vr_host,
			   ", port ", tcp_port, "\n", (char *)NULL);
	  vrmgr = PKC_NULL;
	  return TCL_ERROR;
	}

	bu_vls_from_argv( &str, argc-2, (const char **)argv+2 );

	/* Send initial message declaring our role */
	if( pkg_send_vls( VRMSG_ROLE, &str, vrmgr ) < 0 )  {
	  Tcl_AppendResult(interp, "pkg_send VRMSG_ROLE failed, disconnecting\n", (char *)NULL);
	  pkg_close(vrmgr);
	  vrmgr = NULL;
	  return TCL_ERROR;
	}

	/* Establish appropriate hooks */
	if( strcmp( role, "master" ) == 0 )  {
		viewpoint_hook = vr_viewpoint_hook;
	} else if( strcmp( role, "slave" ) == 0 )  {
		cmdline_hook = vr_event_hook;
	} else if( strcmp( role, "overview" ) == 0 )  {
		/* No hooks required, just listen */
	}
	Tcl_CreateFileHandler(vrmgr->pkc_fd, TCL_READABLE,
			     (Tcl_FileProc (*))vr_input_hook, (ClientData)NULL);
	bu_vls_free( &str );

	return TCL_OK;
}

/*
 *			P H _ C M D
 *
 *  Package handler for incomming commands.  Do whatever he says,
 *  and send a reply back.
 */
void
ph_cmd(register struct pkg_conn *pc, char *buf)
{
	int		status;

	status = Tcl_Eval(interp, buf);

	if( pkg_2send( VRMSG_CMD_REPLY,
		(status == TCL_OK) ? "Y" : "N", 1,
		interp->result, strlen(interp->result)+1, pc ) < 0 )  {
		bu_log("ph_cmd: pkg_2send reply to vrmgr failed, disconnecting\n");
		pkg_close(vrmgr);
		vrmgr = PKC_NULL;
		cmdline_hook = 0;	/* Relinquish this hook */
	}
	if(buf) (void)free(buf);
}

/*
 *			P H _ V L I S T
 *
 *  Package handler for incomming vlist.
 *  Install whatever phantom solids he wants.
 */
void
ph_vlist(register struct pkg_conn *pc, char *buf)
{
	struct bu_list	vhead;
	struct bu_vls	name;

	bu_vls_init(&name);

	BU_LIST_INIT( &vhead );

	rt_vlist_import( &vhead, &name, (unsigned char *)buf );

	invent_solid( bu_vls_addr(&name), &vhead, 0x0000FF00L, 0 );

	bu_vls_free( &name );
	if(buf) (void)free(buf);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
