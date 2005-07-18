#ifndef _ISST_H
#define _ISST_H

#include "tie.h"
#include "adrt_common.h"


#define	ISST_OBSERVER_PORT	1982
#define	ISST_USE_COMPRESSION	1

#define ISST_OP_RENDER		0
#define ISST_OP_SHOT		1
#define ISST_OP_SPALL		2

#define	ISST_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  tfloat */

#define ISST_NET_OP_NOP		0
#define	ISST_NET_OP_INIT	1
#define	ISST_NET_OP_FRAME	2
#define ISST_NET_OP_MESG	3
#define	ISST_NET_OP_QUIT	4
#define	ISST_NET_OP_SHUTDOWN	5


#define ISST_VER_KEY		0
#define ISST_VER		"1.0.0"
#define ISST_VER_DETAIL		"ISST 1.0.0 - U.S. Army Research Lab (2003 - 2005)"

#endif
