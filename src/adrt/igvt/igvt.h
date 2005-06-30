#ifndef _IGVT_H
#define _IGVT_H

#include "tie.h"
#include "adrt_common.h"


#define	IGVT_OBSERVER_PORT	1982
#define	IGVT_USE_COMPRESSION	1

#define IGVT_OP_RENDER		0
#define IGVT_OP_SHOT		1
#define IGVT_OP_SPALL		2

#define	IGVT_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  tfloat */

#define IGVT_NET_OP_NOP		0
#define	IGVT_NET_OP_INIT	1
#define	IGVT_NET_OP_FRAME	2
#define IGVT_NET_OP_MESG	3
#define	IGVT_NET_OP_QUIT	4
#define	IGVT_NET_OP_SHUTDOWN	5


#define IGVT_VER_KEY		0
#define IGVT_VER		"1.0.0"
#define IGVT_VER_DETAIL		"IGVT 1.0.0 - U.S. Army Research Lab (2003 - 2005)"

#endif
