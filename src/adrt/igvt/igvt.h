#ifndef _IGVT_H
#define _IGVT_H

#include "tie.h"
#include "common.h"


#define	IGVT_OBSERVER_PORT	1982
#define	IGVT_USE_COMPRESSION	0

#define	IGVT_OP_CAMERA		0
#define IGVT_OP_SHOTLINE	1

#define	IGVT_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  tfloat */

#define IGVT_NET_OP_NOP		0
#define	IGVT_NET_OP_INIT	1
#define	IGVT_NET_OP_FRAME	2
#define	IGVT_NET_OP_QUIT	3
#define	IGVT_NET_OP_SHUTDOWN	4

#define IGVT_OBS_OP_NOP		0	/* Do Nothing */
#define IGVT_OBS_OP_RM		1	/* Rendering Method */
#define IGVT_OBS_OP_COR		2	/* Center Of Rotation */


#define IGVT_SLA_OP_RM		0


#define IGVT_VER_KEY		0
#define IGVT_VER		"1.0.0"
#define IGVT_VER_DETAIL		"IGVT 1.0.0 - U.S. Army Research Lab (2003 - 2005)"

#endif
