/*                       C O M M A N D . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file nirt/command.c
 *
 * process nirt commands
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "vmath.h"
#include "raytrace.h"

#include "./nirt.h"
#include "./usrfmt.h"


char local_u_name[65];
double base2local;		/* from db_i struct, not fastf_t */
double local2base;		/* from db_i struct, not fastf_t */

extern fastf_t bsphere_diameter;
extern int do_backout;
extern int silent_flag;
extern struct application ap;
extern struct rt_i *rti_tab[];	/* For use w/ and w/o air */
extern struct resource res_tab[];	/* For use w/ and w/o air */
extern com_table ComTab[];
extern outval ValTab[];
extern int overlap_claims;
extern char *ocname[];
extern int nirt_debug;
extern int need_prep;


void
bot_minpieces(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    long new_lvalue;
    int i=0;

    ctp = ctp; /* quell warning */

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current rt_bot_minpieces */
	bu_log("rt_bot_minpieces = %zu\n", rt_bot_minpieces);
	return;
    }

    new_lvalue = atol(buffer);

    if (new_lvalue < 0) {
	bu_log("Error: rt_bot_minpieces cannot be less than 0\n");
	return;
    }

    if ((size_t)new_lvalue != rt_bot_minpieces) {
	rt_bot_minpieces = (size_t)new_lvalue;
	need_prep = 1;
    }
}

void
bot_mintie(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    long new_lvalue;
    int i=0;

    ctp = ctp; /* quell warning */

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current rt_bot_mintie */
	bu_log("rt_bot_mintie = %zu\n", rt_bot_mintie);
	return;
    }

    new_lvalue = atol(buffer);

    if (new_lvalue < 0) {
	bu_log("Error: rt_bot_mintie cannot be less than 0\n");
	return;
    }

    if ((size_t)new_lvalue != rt_bot_mintie) {
	rt_bot_mintie = (size_t)new_lvalue;
	need_prep = 1;
    }
}

void
az_el(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    extern int str_dbl();  /* function to convert string to double */
    int i = 0;      /* current position on the *buffer */
    int rc = 0;     /* the return code value from str_dbl()   */
    double az;
    double el;

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current az and el values */
	bu_log("(az, el) = (%4.2f, %4.2f)\n",
	       azimuth(), elevation());
	return;
    }
    if ((rc = str_dbl(buffer+i, &az)) == 0) {
	/* get az value */
	com_usage(ctp);
	return;
    }
    if (fabs(az) > 360) {
	/* check for valid az value */

	bu_log("Error:  |azimuth| <= 360\n");
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &el)) == 0) {
	/* get el value */
	com_usage(ctp);
	return;
    }
    if (fabs(el) > 90) {
	/* check for valid el value */
	bu_log("Error:  |elevation| <= 90\n");
	return;
    }
    i += rc;
    if (*(buffer+i) != '\0') {
	/* check for garbage at the end of the line */
	com_usage(ctp);
	return;
    }
    azimuth() = az;
    elevation() = el;
    ae2dir();
}

void
sh_esc(char *buffer)
{
    int ret;
    static char *shell = "";
    static char *last_cmd = "";

    while (isspace(*buffer)) {
	++buffer;
    }

    if (*buffer == '!') {
	ret = system(last_cmd);
	if (ret == -1)
	    perror("system");
    } else if (*buffer) {
	ret = system(buffer);
	if (ret == -1)
	    perror("system");
	last_cmd = buffer;
    } else {
	if ((*shell == '\0') && (shell = getenv("SHELL")) == 0) {
#ifndef _WIN32
	    shell = DFLT_SHELL;
#else
	    shell = "cmd.exe";
#endif
	}
	ret = system(shell);
	if (ret == -1)
	    perror("system");
    }
}

void
grid_coor(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    extern int str_dbl();  /* function to convert string to double */
    int i = 0;
    int rc = 0;    /* the return code value from str_dbl() */
    vect_t Gr;

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current grid coordinates */
	bu_log("(h, v, d) = (%4.2f, %4.2f, %4.2f)\n",
	       grid(HORZ) * base2local,
	       grid(VERT) * base2local,
	       grid(DIST) * base2local);
	return;
    }
    if ((rc = str_dbl(buffer+i, &Gr[HORZ])) == 0) {
	/* get horz coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &Gr[VERT])) == 0) {
	/* get vert coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* if there is no dist coor, set default */
	grid(HORZ) = Gr[HORZ] * local2base;
	grid(VERT) = Gr[VERT] * local2base;
	grid2targ();
	return;
    }
    if ((rc = str_dbl(buffer+i, &Gr[DIST])) == 0) {
	/* set dist coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    if (*(buffer+i) != '\0') {
	/* check for garbage at the end of the line */
	com_usage(ctp);
	return;
    }
    grid(HORZ) = Gr[HORZ] * local2base;
    grid(VERT) = Gr[VERT] * local2base;
    grid(DIST) = Gr[DIST] * local2base;
    grid2targ();
}

void
target_coor(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    extern int str_dbl();  /* function to convert string to double */
    int i = 0;
    int rc = 0;     /* the return code value from str_dbl() */
    vect_t Tar;	    /* Target x, y and z */

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current target coors */
	bu_log("(x, y, z) = (%4.2f, %4.2f, %4.2f)\n",
	       target(X) * base2local,
	       target(Y) * base2local,
	       target(Z) * base2local);
	return;
    }
    if ((rc = str_dbl(buffer+i, &Tar[X])) == 0) {
	/* get target x coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &Tar[Y])) == 0) {
	/* get target y coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &Tar[Z])) == 0) {
	/* get target z coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    if (*(buffer+i) != '\0') {
	/* check for garbage at the end of the line */
	com_usage(ctp);
	return;
    }
    target(X) = Tar[X] * local2base;
    target(Y) = Tar[Y] * local2base;
    target(Z) = Tar[Z] * local2base;
    targ2grid();
}

void
dir_vect(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    extern int str_dbl();  /* function to convert string to double */
    int i = 0;
    int rc = 0;    /* the return code value from str_dbl() */
    vect_t Dir;	   /* Direction vector x, y and z */

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current direct coors */
	bu_log("(x, y, z) = (%4.2f, %4.2f, %4.2f)\n",
	       direct(X), direct(Y), direct(Z));
	return;
    }
    if ((rc = str_dbl(buffer+i, &Dir[X])) == 0) {
	/* get direct x coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &Dir[Y])) == 0) {
	/* get direct y coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    while (isspace(*(buffer+i)))
	++i;
    if ((rc = str_dbl(buffer+i, &Dir[Z])) == 0) {
	/* get direct z coor */
	com_usage(ctp);
	return;
    }
    i += rc;
    if (*(buffer+i) != '\0') {
	/* check for garbage at the end of the line */
	com_usage(ctp);
	return;
    }
    VUNITIZE(Dir);
    direct(X) = Dir[X];
    direct(Y) = Dir[Y];
    direct(Z) = Dir[Z];
    dir2ae();
}

void
quit()
{
    if (silent_flag != SILENT_YES)
	(void) fputs("Quitting...\n", stdout);
    bu_exit (0, NULL);
}

void
show_menu()
{
    com_table *ctp;

    for (ctp = ComTab; ctp->com_name; ++ctp)
	(void) bu_log("%*s %s\n", -14, ctp->com_name, ctp->com_desc);
}

void
shoot(char *buffer, com_table *ctp, struct rt_i *rtip)
{
    int i;
    double bov = 0.0;	/* back out value */

    extern void init_ovlp();

    /* quellage */
    buffer = buffer;
    ctp = ctp;

    if (need_prep) {
	if (rtip) rt_clean(rtip);
	do_rt_gettrees(rtip, NULL, 0, &need_prep);
    }

    if (do_backout) {
	point_t ray_point;
	vect_t ray_dir;
	vect_t center_bsphere;
	fastf_t dist_to_target;
	vect_t dvec;
	fastf_t delta;

	for (i = 0; i < 3; ++i) {
	    ray_point[i] = target(i);
	    ray_dir[i] = direct(i);
	}

	if (bsphere_diameter < 0)
	    set_diameter(rtip);

	/*
	 * calculate the distance from a plane normal to the ray direction through the center of 
	 * the bounding sphere and a plane normal to the ray direction through the aim point.
	 */
	VADD2SCALE(center_bsphere, rtip->mdl_max, rtip->mdl_min, 0.5);

	dist_to_target = DIST_PT_PT(center_bsphere, ray_point); 

	VSUB2(dvec, ray_point, center_bsphere);
	VUNITIZE(dvec);
	delta = dist_to_target*VDOT(ray_dir, dvec);

	/*
	 * this should put us about a bounding sphere radius in front of the bounding sphere
	 */
	bov = bsphere_diameter + delta;
    }

    for (i = 0; i < 3; ++i) {
	target(i) = target(i) + (bov * -direct(i));
	ap.a_ray.r_pt[i] = target(i);
	ap.a_ray.r_dir[i] = direct(i);
    }

    if (nirt_debug & DEBUG_BACKOUT) {
	bu_log("Backing out %g units to (%g %g %g), shooting dir is (%g %g %g)\n",
	       bov * base2local,
	       ap.a_ray.r_pt[0] * base2local,
	       ap.a_ray.r_pt[1] * base2local,
	       ap.a_ray.r_pt[2] * base2local,
	       V3ARGS(ap.a_ray.r_dir));
    }

    init_ovlp();
    (void) rt_shootray(&ap);

    /* Restore pre-backout target values */
    if (do_backout) {
	for (i = 0; i < 3; ++i) {
	    target(i) = target(i) - (bov * -direct(i));
	}
    }

}

void
use_air(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    int new_use = 0;      /* current position on the *buffer */
    char response[128];
    char *rp = response;
    char db_title[TITLE_LEN+1];	/* title from MGED database */
    struct rt_i *my_rtip;

    extern char *db_name;		/* Name of MGED database file */

    while (isspace(*buffer))
	++buffer;
    if (*buffer == '\0') {
	/* display current value of use_of_air */
	bu_log("use_air = %d\n", ap.a_rt_i->useair);
	return;
    }
    if (!isdigit(*buffer)) {
	com_usage(ctp);
	return;
    }
    while (isdigit(*buffer)) {
	new_use *= 10;
	new_use += *buffer++ - '0';
    }
    if (new_use && (new_use != 1)) {
	bu_log("Warning: useair=%d specified, will set to 1\n",
	       new_use);
	new_use = 1;
    }
    if (rti_tab[new_use] == RTI_NULL) {
	bu_log(" Air %s in the current directory of database objects.\n",
	       new_use ? "is not included" : "is included");
	bu_log(
	    " To set useair=%d requires building/prepping another directory.\n",
	    new_use);
	bu_log(" Do you want to do that now (y|n)[n]? ");
	bu_fgets(response, sizeof(response), stdin);
	while ((*rp == ' ') || (*rp == '\t'))
	    ++rp;
	if ((*rp != 'y') && (*rp != 'Y')) {
	    bu_log("useair remains %d\n", ap.a_rt_i->useair);
	    return;
	}
	bu_log("Building the directory...");
	if ((my_rtip = rt_dirbuild(db_name, db_title, TITLE_LEN)) == RTI_NULL) {
	    bu_log("Could not load file %s\n", db_name);
	    printusage();
	    bu_exit(1, NULL);
	}
	rti_tab[new_use] = my_rtip;
	my_rtip->useair = new_use;
	my_rtip->rti_save_overlaps = (overlap_claims > 0);

	bu_log("Prepping the geometry...");
	do_rt_gettrees(my_rtip, NULL, 0, &need_prep);
    }
    ap.a_rt_i = rti_tab[new_use];
    ap.a_resource = &res_tab[new_use];
    set_diameter(ap.a_rt_i);
}

void
nirt_units(char *buffer, com_table *ctp, struct rt_i *rtip)
{
    double tmp_dbl;
    int i = 0;      /* current position on the *buffer */

    double mk_cvt_factor();

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current destination */
	bu_log("units = '%s'\n", local_u_name);
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "?")) {
	com_usage(ctp);
	return;
    } else if (BU_STR_EQUAL(buffer + i, "default")) {
	base2local = rtip->rti_dbip->dbi_base2local;
	local2base = rtip->rti_dbip->dbi_local2base;
	bu_strlcpy(local_u_name, bu_units_string(base2local), sizeof(local_u_name));
    } else {
	tmp_dbl = bu_units_conversion(buffer + i);
	if (tmp_dbl <= 0.0) {
	    bu_log("Invalid unit specification: '%s'\n", buffer + i);
	    return;
	}
	bu_strlcpy(local_u_name, buffer + i, sizeof(local_u_name));
	local2base = tmp_dbl;
	base2local = 1.0 / tmp_dbl;
    }
}

void
do_overlap_claims(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    int i = 0;      /* current position on the *buffer */
    int j;
    double mk_cvt_factor();

    while (isspace(*(buffer+i)))
	++i;
    if (*(buffer+i) == '\0') {
	/* display current destination */
	bu_log("overlap_claims = '%s'\n", ocname[overlap_claims]);
	return;
    }

    if (BU_STR_EQUAL(buffer + i, "?")) {
	com_usage(ctp);
	return;
    }
    for (j = OVLP_RESOLVE; j <= OVLP_RETAIN; ++j) {
	char numeral[4];
	int k;

	sprintf(numeral, "%d", j);
	if ((BU_STR_EQUAL(buffer + i, ocname[j]))
	    || (BU_STR_EQUAL(buffer + i, numeral))) {
	    overlap_claims = j;
	    for (k = 0; k < 2; ++k)
		if (rti_tab[k] != RTI_NULL)
		    rti_tab[k]->rti_save_overlaps = (j > 0);
	    return;
	}
    }

    bu_log("Invalid overlap_claims specification: '%s'\n", buffer + i);
}

void
cm_attr(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    while (isascii(*buffer) && isspace(*buffer)) buffer++;

    if (strlen(buffer) == 0) {
	com_usage(ctp);
	return;
    }

    if (! strncmp(buffer, "-p", 2)) {
	attrib_print();
	return;
    }

    if (! strncmp(buffer, "-f", 2)) {
	attrib_flush();
	return;
    }

    attrib_add(buffer, &need_prep);
}

void
cm_debug(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    char *cp = buffer;

    /* This is really icky -- should have argc, argv interface */
    while (*cp && isascii(*cp) && isspace(*cp))  cp++;
    if (*cp == '\0') {
	/* display current value */
	bu_printb("debug ", nirt_debug, DEBUG_FMT);
	bu_log("\n");
	return;
    }

    /* Set a new value */
    if (sscanf(cp, "%x", (unsigned int *)&nirt_debug) == 1) {
	bu_printb("debug ", nirt_debug, DEBUG_FMT);
	bu_log("\n");
    } else {
	com_usage(ctp);
    }
}

void
cm_libdebug(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    char *cp = buffer;

    /* This is really icky -- should have argc, argv interface */

    while (*cp && isascii(*cp) && isspace(*cp))
	cp++;

    if (*cp == '\0') {
	/* display current value */
	bu_printb("libdebug ", RT_G_DEBUG, RT_DEBUG_FMT);
	bu_log("\n");
	return;
    }

    /* Set a new value */
    if (sscanf(cp, "%x", (unsigned int *)&rt_g.debug) == 1) {
	bu_printb("libdebug ", RT_G_DEBUG, RT_DEBUG_FMT);
	bu_log("\n");
    } else {
	com_usage(ctp);
    }
}

void
backout(char *buffer, com_table *ctp, struct rt_i *UNUSED(rtip))
{
    /* quellage */
    ctp = ctp;

    while (isspace(*buffer))
	++buffer;
    if (*buffer == '\0') {
	/* display current value of do_backout */
	bu_log("backout = %d\n", do_backout);
	return;
    }
    if (!isdigit(*buffer)) {
	bu_log("backout must be set to 0 (off) or 1 (on)\n");
	return;
    } else {
	if (*buffer == '0') {
	    do_backout = 0;
	} else {
	    do_backout = 1;
	}
    }
    return;
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
