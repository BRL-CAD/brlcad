/*                   I G E S _ E X T E R N . H
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
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
/** @file iges_extern.h
 *
 */

#define		CARDLEN		71 /* length of data portion in Global records */
#define		PARAMLEN	63 /* length of data portion in Parameter records */

extern int bu_debug;

extern int do_projection;
extern char eor; /* IGES end of record delimeter */
extern char eof; /* IGES end of field delimeter */
extern char card[256]; /* input buffer, filled by readrec */
extern fastf_t scale,inv_scale; /* IGES file scale factor and inverse */
extern fastf_t conv_factor; /* Conversion factor from IGES file units to mm */
extern mat_t *identity; /* identity matrix */
extern int units; /* IGES file units code */
extern int counter; /* keep track of where we are in the "card" buffer */
extern int pstart; /* record number where parameter section starts */
extern int dstart; /* record number where directory section starts */
extern int totentities; /* total number of entities in the IGES file */
extern int dirarraylen;	/* number of elements in the "dir" array */
extern int reclen; /* IGES file record length (in bytes) */
extern int currec; /* current record number in the "card" buffer */
extern int ntypes; /* Number of different types of IGES entities recognized by
			this code */
extern int brlcad_att_de; /* DE sequence number for BRL-CAD attribute
		definition entity */
extern int do_bots; /* flag indicating NMG solids should be written as BOT solids */
extern FILE *fd; /* file pointer for IGES file */
extern struct rt_wdb *fdout; /* file pointer for BRL-CAD output file */
extern char brlcad_file[]; /* name of brlcad output file (".g" file) */
extern struct iges_directory **dir; /* Directory array */
extern struct reglist *regroot; /* list of regions created from solids of revolution */
extern struct types typecount[]; /* Count of how many entities of each type actually
				appear in the IGES file */
extern char operator[]; /* characters representing operators: 'u', '+', and '-' */
extern struct iges_edge_list *edge_root;
extern struct iges_vertex_list *vertex_root;
extern struct rt_tol tol;
extern char *solid_name;
extern struct file_list *curr_file;
extern struct file_list iges_list;
extern struct name_list *name_root;

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
