/*
 * rla_header.h -- Data structure defining Wavefront's "rla" image file format.
 *
 * Author:      Wesley C. Barris
 *              AHPCRC
 *              Minnesota Supercomputer Center, Inc.
 * Date:        Jan. 17, 1992
 *              Copyright @ 1992, Minnesota Supercomputer Center, Inc.
 *
 * RESTRICTED RIGHTS LEGEND
 *
 * Use, duplication, or disclosure of this software and its documentation
 * by the Government is subject to restrictions as set forth in subdivision
 * { (b) (3) (ii) } of the Rights in Technical Data and Computer Software
 * clause at 52.227-7013.
 */
#ifndef _WINDOW_S_TYPE
#define _WINDOW_S_TYPE
typedef struct {
   short left, right, bottom, top;
} WINDOW_S;
#endif

typedef struct {
   WINDOW_S	window;
   WINDOW_S	active_window;
   short	frame;
   short	storage_type;
   short	num_chan;
   short	num_matte;
   short	num_aux;
   short	aux_mask;
   char		gamma[16];
   char		red_pri[24];
   char		green_pri[24];
   char		blue_pri[24];
   char		white_pt[24];
   long		job_num;
   char		name[128];
   char		desc[128];
   char		program[64];
   char		machine[32];
   char		user[32];
   char		date[20];
   char		aspect[32];
   char		chan[32];
   char		space[128];
} RLA_HEADER;
