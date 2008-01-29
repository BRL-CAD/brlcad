#ifndef _ADRT_H
#define _ADRT_H

#define	ADRT_PORT		1982
#define	ADRT_USE_COMPRESSION	1
#define ADRT_MAX_WORKSPACE_NUM	64

/* Pixel Format: 0 = unsigned char, 1 =  tfloat */
#define	ADRT_PIXEL_FMT		0

/* these are contained in an ADRT_NETOP_WORK message */
enum
{
  ADRT_WORK_INIT = 100,
  ADRT_WORK_STATUS,
  ADRT_WORK_FRAME_ATTR,
  ADRT_WORK_FRAME,
  ADRT_WORK_SHOTLINE,
  ADRT_WORK_SPALL,
  ADRT_WORK_SELECT,
  ADRT_WORK_MINMAX
};

/* top level messages */
enum
{
  ADRT_NETOP_NOP = 200,
  ADRT_NETOP_INIT,
  ADRT_NETOP_REQWID,
  ADRT_NETOP_LOAD,
  ADRT_NETOP_WORK,
  ADRT_NETOP_MESG,
  ADRT_NETOP_QUIT,
  ADRT_NETOP_SHUTDOWN
};

#define ADRT_VER_KEY		0
#define ADRT_VER_DETAIL		"ADRT - U.S. Army Research Lab (2003 - 2006)"

#define ADRT_MESH_HIT	0x1
#define ADRT_MESH_SELECT	0x2

#define ADRT_NAME_SIZE 256

#endif
