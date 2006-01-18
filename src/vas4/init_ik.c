/*                       I N I T _ I K . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2006 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 */
/** @file init_ik.c
 * init_ik.c - initialize Ikonas to RS170
 */

#include <stdio.h>

init_ik(void)
{
	char cmd[100];
	FILE *popen(const char *, const char *), *pipe;


	sprintf(cmd,"fbi > /dev/null");
	pipe = popen(cmd,"w");

/*	fprintf(pipe,"1\n6,7760\n9,456\n10,1015\n14,1\n\n-1\n"); /* */
	fprintf(pipe,"1\n6,4083\n14,1\n\n-1\n");

	pclose(pipe);
	fprintf(stderr,"Ikonas set for RS170 and External SYNC\n");
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
