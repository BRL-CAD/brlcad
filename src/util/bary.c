/*                          B A R Y . C
 * BRL-CAD
 *
 * Copyright (c) 1995-2011 United States Government as represented by
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
/** @file util/bary.c
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include "bu.h"
#include "vmath.h"
#include "raytrace.h"


struct site
{
    struct bu_list l;
    fastf_t s_x;
    fastf_t s_y;
    fastf_t s_z;
};
#define SITE_NULL ((struct site *) 0)
#define SITE_MAGIC 0x73697465
#define s_magic l.magic
#define OPT_STRING "ns:t?"

void print_usage (void)
{
    bu_exit(1, "Usage: 'bary [-nt] [-s \"x y z\"] [file]'\n");
}


void enqueue_site (struct bu_list *sl, fastf_t x, fastf_t y, fastf_t z)
{
    struct site *sp;

    BU_CK_LIST_HEAD(sl);

    sp = (struct site *) bu_malloc(sizeof(struct site), "site structure");
    sp->s_magic = SITE_MAGIC;
    sp->s_x = x;
    sp->s_y = y;
    sp->s_z = z;

    BU_LIST_INSERT(sl, &(sp->l));
}


void show_sites (struct bu_list *sl)
{
    struct site *sp;

    BU_CK_LIST_HEAD(sl);

    for (BU_LIST_FOR(sp, site, sl)) {
	bu_log("I got a site (%g, %g, %g)\n",
	       sp->s_x, sp->s_y, sp->s_z);
    }
}


int read_point (FILE *fp, fastf_t *c_p, int c_len, int normalize, struct bu_vls *tail)
{
    char *cp = NULL;
    fastf_t sum;
    int i;
    int return_code = 1;
    static int line_nm = 0;
    struct bu_vls *bp;

    for (bp = bu_vls_vlsinit();; bu_vls_trunc(bp, 0)) {
	if (bu_vls_gets(bp, fp) == -1) {
	    return_code = EOF;
	    goto wrap_up;
	}

	++line_nm;
	cp = bu_vls_addr(bp);

	while ((*cp == ' ') || (*cp == '\t'))
	    ++cp;

	if ((*cp == '#') || (*cp == '\0'))
	    continue;

	for (i = 0; i < c_len; ++i) {
	    char *endp;

	    c_p[i] = strtod(cp, &endp);
	    if (endp == cp)
		bu_exit (1, "Illegal input at line %d: '%s'\n",
			 line_nm, bu_vls_addr(bp));
	    cp = endp;
	}

	if (normalize) {
	    sum = 0.0;
	    for (i = 0; i < c_len; ++i)
		sum += c_p[i];
	    for (i = 0; i < c_len; ++i)
		c_p[i] /= sum;
	}
	goto wrap_up;
    }

 wrap_up:
    if ((return_code == 1) && (tail != 0)) {
	bu_vls_trunc(tail, 0);
	bu_vls_strcat(tail, cp);
    }
    bu_vls_vlsfree(bp);
    return return_code;
}


int
main (int argc, char **argv)
{
    char *inf_name;
    int ch;
    int i;
    int nm_sites;
    int normalize = 0;	/* Make all weights sum to one? */
    fastf_t *coeff;
    fastf_t x, y, z;
    FILE *infp = NULL;
    struct bu_list site_list;
    struct bu_vls *tail_buf = 0;
    struct site *sp;

    BU_LIST_INIT(&site_list);
    while ((ch = bu_getopt(argc, argv, OPT_STRING)) != -1)
	switch (ch) {
	    case 'n':
		normalize = 1;
		break;
	    case 's':
		if (sscanf(bu_optarg, "%lf %lf %lf", &x, &y, &z) != 3) {
		    bu_log("Illegal site: '%s'\n", bu_optarg);
		    print_usage();
		}
		enqueue_site(&site_list, x, y, z);
		break;
	    case 't':
		if (tail_buf == 0)	 /* Only initialize it once */
		    tail_buf = bu_vls_vlsinit();
		break;
	    case '?':
	    default:
		print_usage();
	}

    switch (argc - bu_optind) {
	case 0:
	    inf_name = "stdin";
	    infp = stdin;
	    break;
	case 1:
	    inf_name = argv[bu_optind++];
	    if ((infp = fopen(inf_name, "r")) == NULL)
		bu_exit (1, "Cannot open file '%s'\n", inf_name);
	    break;
	default:
	    print_usage();
    }

    if (BU_LIST_IS_EMPTY(&site_list)) {
	enqueue_site(&site_list, (fastf_t) 1.0, (fastf_t) 0.0, (fastf_t) 0.0);
	enqueue_site(&site_list, (fastf_t) 0.0, (fastf_t) 1.0, (fastf_t) 0.0);
	enqueue_site(&site_list, (fastf_t) 0.0, (fastf_t) 0.0, (fastf_t) 1.0);
    }

    nm_sites = 0;
    for (BU_LIST_FOR(sp, site, &site_list))
	++nm_sites;

    coeff = (fastf_t *)
	bu_malloc(nm_sites * sizeof(fastf_t), "coefficient array");

    while (read_point(infp, coeff, nm_sites, normalize, tail_buf) != EOF) {
	x = y = z = 0.0;
	i = 0;
	for (BU_LIST_FOR(sp, site, &site_list)) {
	    x += sp->s_x * coeff[i];
	    y += sp->s_y * coeff[i];
	    z += sp->s_z * coeff[i];
	    ++i;
	}
	bu_flog(stdout, "%g %g %g", x, y, z);
	if (tail_buf)
	    bu_flog(stdout, "%s", bu_vls_addr(tail_buf));
	bu_flog(stdout, "\n");
    }
    return 0;
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
