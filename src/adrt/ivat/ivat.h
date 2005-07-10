#ifndef _IVAT_H
#define _IVAT_H

#include "tie.h"
#include "adrt_common.h"


#define	IVAT_OBSERVER_PORT	1982
#define	IVAT_USE_COMPRESSION	1

#define IVAT_OP_RENDER		0
#define IVAT_OP_SHOT		1
#define IVAT_OP_SPALL		2

#define	IVAT_PIXEL_FMT		0	/* 0 == unsigned char, 1 ==  tfloat */

#define IVAT_NET_OP_NOP		0
#define	IVAT_NET_OP_INIT	1
#define	IVAT_NET_OP_FRAME	2
#define IVAT_NET_OP_MESG	3
#define	IVAT_NET_OP_QUIT	4
#define	IVAT_NET_OP_SHUTDOWN	5


#define IVAT_VER_KEY		0
#define IVAT_VER		"1.0.0"
#define IVAT_VER_DETAIL		"IVAT 1.0.0 - U.S. Army Research Lab (2003 - 2005)"

#endif
