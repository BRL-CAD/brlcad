/*                V I E W _ B O T _ F A C E S . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2011 United States Government as represented by
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
/** @file rt/view_bot_faces.c
 *
 * Ray Tracing program view module to find visible bot faces
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#include "vmath.h"
#include "raytrace.h"
#include "plot3.h"

#include "./rtuif.h"
#include "./ext.h"


extern char *outputfile;		/* output file name */

extern point_t viewbase_model;

extern int npsw;			/* number of worker PSWs to run */

extern int rpt_overlap;

extern struct bu_vls ray_data_file;  /* file name for ray data output (declared in do.c) */
extern FILE *outfp;

static Tcl_HashTable bots;		/* hash table with a bot_face_list entry for each BOT primitive hit */

/* Viewing module specific "set" variables */
struct bu_structparse view_parse[] = {
    {"",	0, (char *)0,	0,		BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
};


const char title[] = "RT BoT Faces";

void
usage(const char *argv0)
{
    bu_log("Usage:  %s [options] model.g objects... >file.ray\n", argv0);
    bu_log("Options:\n");
    bu_log(" -s #		Grid size in pixels, default 512\n");
    bu_log(" -a Az		Azimuth in degrees	(conflicts with -M)\n");
    bu_log(" -e Elev	Elevation in degrees	(conflicts with -M)\n");
    bu_log(" -M		Read model2view matrix on stdin (conflicts with -a, -e)\n");
    bu_log(" -g #		Grid cell width in millimeters (conflicts with -s)\n");
    bu_log(" -G #		Grid cell height in millimeters (conflicts with -s)\n");
    bu_log(" -J #		Jitter.  Default is off.  Any non-zero number is on\n");
    bu_log(" -o bot_faces_file	Specify output file, list of bot_faces hit (default=stdout)\n");
    bu_log(" -U #		Set use_air boolean to # (default=1)\n");
    bu_log(" -c \"set save_overlaps=1\"     Reproduce FASTGEN behavior for regions flagged as FASTGEN regions\n");
    bu_log(" -c \"set rt_cline_radius=radius\"      Additional radius to be added to CLINE solids\n");
    bu_log(" -x #		Set librt debug flags\n");
}


/*
 * R A Y H I T
 *
 * Rayhit() is called by rt_shootray() when the ray hits one or more objects.
 */
int
rayhit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segp))
{
    register struct partition *pp = PartHeadp->pt_forw;
    Tcl_HashEntry *entry;
    int newPtr;
    struct bu_ptbl *faces;

    if (pp == PartHeadp)
	return 0;		/* nothing was actually hit?? */

    if (ap->a_rt_i->rti_save_overlaps)
	rt_rebuild_overlaps(PartHeadp, ap, 1);

    /* did we hit a BOT?? */
    if (pp->pt_inseg->seg_stp->st_dp->d_major_type != DB5_MAJORTYPE_BRLCAD ||
	pp->pt_inseg->seg_stp->st_dp->d_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	return 0;
    }

    /* this is a BOT, get the hash tabel entry for it */
    bu_semaphore_acquire(BU_SEM_LISTS);
    entry = Tcl_CreateHashEntry(&bots, pp->pt_inseg->seg_stp->st_dp->d_namep, &newPtr);
    if (newPtr) {
	faces = (struct bu_ptbl *)bu_malloc(sizeof(struct bu_ptbl), "faces");
	bu_ptbl_init(faces, 128, "faces");
	Tcl_SetHashValue(entry, (char *)faces);
    } else {
	faces = (struct bu_ptbl *)Tcl_GetHashValue(entry);
    }

    bu_ptbl_ins_unique(faces, (long *)(size_t)pp->pt_inhit->hit_surfno);
    bu_semaphore_release(BU_SEM_LISTS);

    return 0;
}


/*
 * R A Y M I S S
 *
 * Null function -- handle a miss
 * This function is called by rt_shootray(), which is called by
 * do_frame().
 */
int
raymiss(struct application *UNUSED(ap))
{
    return 0;
}


/*
 * V I E W _ I N I T
 *
 * This routine is called by main().
 */

int
view_init(struct application *ap, char *UNUSED(file), char *UNUSED(obj), int minus_o, int UNUSED(minus_F))
{
    /* report air regions */
    use_air = 1;

    if (!minus_o)
	outfp = stdout;

    ap->a_hit = rayhit;
    ap->a_miss = raymiss;
    ap->a_onehit = 1;

    output_is_binary = 0;

    if (!rpt_overlap)
	ap->a_logoverlap = rt_silent_logoverlap;

    /* initialize hash table */
    Tcl_InitHashTable(&bots, TCL_STRING_KEYS);

    return 0;		/* No framebuffer needed */
}


/*
 * V I E W _ 2 I N I T
 *
 * View_2init is called by do_frame(), which in turn is called by
 * main() in rt.c.
 *
 */
void
view_2init(struct application *ap, char *framename)
{
#ifdef HAVE_SYS_STAT_H
    struct stat sb;
    char line[RT_MAXLINE];
#endif

    if (outfp == NULL)
	bu_exit(EXIT_FAILURE, "outfp is NULL\n");

#ifdef HAVE_SYS_STAT_H
    /* read in any existing data */
    if (outfp != NULL && stat(framename, &sb) >= 0 && sb.st_size > 0) {
	Tcl_HashEntry *entry;
	char *bot_name;
	struct bu_ptbl *faces=NULL;
	int newPtr;
	int i, j;

	/* File exists, with partial results */
	while (bu_fgets(line, RT_MAXLINE, outfp)) {
	    if (!strncmp(line, "BOT:", 4)) {
		struct directory *dp;

		/* found a BOT entry, addit to the hash table */
		i = 4;
		while (line[i] != '\0' && isspace(line[i])) i++;
		if (line[i] == '\0') {
		    bu_log("Unexpected EOF found in partial results (%s)\n", outputfile);
		    bu_exit(EXIT_FAILURE, "Unexpected EOF");
		}
		j = i;
		while (line[j] != '\0' && !isspace(line[j])) j++;
		line[j] = '\0';
		if ((dp=db_lookup(ap->a_rt_i->rti_dbip, &line[i], LOOKUP_QUIET)) == RT_DIR_NULL) {
		    bot_name = bu_strdup(&line[i]);
		} else {
		    bot_name = dp->d_namep;
		}
		entry = Tcl_CreateHashEntry(&bots, bot_name, &newPtr);
		if (newPtr) {
		    faces = (struct bu_ptbl *)bu_calloc(1, sizeof(struct bu_ptbl),
							"bot_faces");
		    bu_ptbl_init(faces, 128, "bot faces");
		    Tcl_SetHashValue(entry, (char *)faces);
		} else {
		    faces = (struct bu_ptbl *)Tcl_GetHashValue(entry);
		}
	    } else {
		long int face_num;
		long *fnp;

		if (!faces) {
		    bu_exit(EXIT_FAILURE, "No faces structure while reading partial data!!!\n");
		}
		face_num = atoi(line);
		fnp = (long *)face_num;
		bu_ptbl_ins_unique(faces, fnp);
	    }
	}
    }

#endif
}


/*
 * V I E W _ P I X E L
 *
 * This routine is called from do_run(), and in this case does nothing.
 */
void
view_pixel(struct application *UNUSED(ap))
{
    return;
}


/*
 * V I E W _ E O L
 *
 * View_eol() is called by rt_shootray() in do_run().  In this case,
 * it does nothing.
 */
void view_eol(struct application *UNUSED(ap))
{
}


/*
 * V I E W _ E N D
 *
 * View_end() is called by rt_shootray in do_run().
 *
 */
void
view_end(struct application *UNUSED(ap))
{
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;
    struct bu_ptbl *faces;

    /* rewrite entire output file */
    rewind(outfp);

    entry = Tcl_FirstHashEntry(&bots, &search);

    while (entry) {
	size_t i;

	fprintf(outfp, "BOT: %s\n", Tcl_GetHashKey(&bots, entry));
	faces = (struct bu_ptbl *)Tcl_GetHashValue(entry);
	for (i=0; i<BU_PTBL_LEN(faces); i++) {
	    fprintf(outfp, "\t%lu\n", (unsigned long)BU_PTBL_GET(faces, i));
	}
	entry = Tcl_NextHashEntry(&search);
    }

    fflush(outfp);
}


void view_setup(struct rt_i *UNUSED(rtip))
{
}
void view_cleanup(struct rt_i *UNUSED(rtip))
{
}
void application_init()
{
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
