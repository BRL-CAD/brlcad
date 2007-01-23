/*                          D M - P S . H
 * BRL-CAD
 *
 * Copyright (c) 1993-2007 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @addtogroup libdm */
/** @{ */
/** @file dm-ps.h
 *
 */
#ifndef SEEN_DM_PS
#define SEEN_DM_PS

/*
 * Display coordinate conversion:
 *  GED is using -2048..+2047,
 *  and we define the Postscript file to use 0..4095
 */
#define	GED_TO_PS(x)	((int)((x)+2048))

struct ps_vars {
  struct bu_list l;
  FILE *ps_fp;
  char ttybuf[BUFSIZ];
  vect_t clipmin;
  vect_t clipmax;
  struct bu_vls fname;
  struct bu_vls font;
  struct bu_vls title;
  struct bu_vls creator;
  fastf_t scale;
  int linewidth;
  int zclip;
  int debug;
};

extern struct ps_vars head_ps_vars;

#endif /* SEEN_DM_PS */
/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

