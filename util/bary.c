/*
 *				B A R Y . C
 *
 *  Author -
 *	Paul J. Tanenbaum
 *  
 *  Source -
 *	The U. S. Army Research Laboratory
 *	Aberdeen Proving Ground, Maryland  21005-5068  USA
 *  
 *  Distribution Notice -
 *	Re-distribution of this software is restricted, as described in
 *	your "Statement of Terms and Conditions for the Release of
 *	The BRL-CAD Package" agreement.
 *
 *  Copyright Notice -
 *	This software is Copyright (C) 1995 by the United States Army
 *	in all countries except the USA.  All rights reserved.
 */
#ifndef lint
static char RCSid[] = "@(#)$Header$ (ARL)";
#endif

#include <stdio.h>
#include <math.h>
#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "rtlist.h"

struct site
{
    struct bu_list	l;
    fastf_t		s_x;
    fastf_t		s_y;
    fastf_t		s_z;
};
#define	SITE_NULL	((struct site *) 0)
#define	SITE_MAGIC	0x73697465
#define s_magic		l.magic

void print_usage (void)
{
#define OPT_STRING	"ns:t?"

    bu_log("Usage: 'bary [-nt] [-s \"x y z\"] [file]'\n");
}

void enqueue_site (sl, x, y, z)

struct bu_list	*sl;
fastf_t		x, y, z;

{
    struct site	*sp;

    BU_CK_LIST_HEAD(sl);

    sp = (struct site *) bu_malloc(sizeof(struct site), "site structure");
    sp -> s_magic = SITE_MAGIC;
    sp -> s_x = x;
    sp -> s_y = y;
    sp -> s_z = z;

    BU_LIST_INSERT(sl, &(sp -> l));
}

void show_sites (sl)

struct bu_list	*sl;

{
    struct site	*sp;

    BU_CK_LIST_HEAD(sl);

    for (BU_LIST_FOR(sp, site, sl))
    {
	bu_log("I got a site (%g, %g, %g)\n",
	    sp -> s_x, sp -> s_y, sp -> s_z);
    }
}

int read_point (fp, c_p, c_len, normalize, tail)

FILE		*fp;
fastf_t		*c_p;
int		c_len;
int		normalize;
struct bu_vls	*tail;

{
    char		*cp;
    fastf_t		sum;
    int			i;
    int			return_code = 1;
    static int		line_nm = 0;
    struct bu_vls	*bp;

    for (bp = bu_vls_vlsinit(); ; bu_vls_trunc(bp, 0))
    {
	if (bu_vls_gets(bp, fp) == -1)
	{
	    return_code = EOF;
	    goto wrap_up;
	}

	++line_nm;
	cp = bu_vls_addr(bp);

	while ((*cp == ' ') || (*cp == '\t'))
	    ++cp;
	
	if ((*cp == '#') || (*cp == '\0'))
	    continue;

	for (i = 0; i < c_len; ++i)
	{
	    char	*endp;

	    c_p[i] = strtod(cp, &endp);
	    if (endp == cp)
	    {
		bu_log("Illegal input at line %d: '%s'\n",
		    line_nm, bu_vls_addr(bp));
		exit (1);
	    }
	    cp = endp;
	}

	if (normalize)
	{
	    sum = 0.0;
	    for (i = 0; i < c_len; ++i)
		sum += c_p[i];
	    for (i = 0; i < c_len; ++i)
		c_p[i] /= sum;
	}
	goto wrap_up;
    }

    wrap_up:
	if ((return_code == 1) && (tail != 0))
	{
	    bu_vls_trunc(tail, 0);
	    bu_vls_strcat(tail, cp);
	}
	bu_vls_vlsfree(bp);
	return (return_code);
}

main (argc, argv)

int	argc;
char	*argv[];

{
    char		*inf_name;
    int			ch;
    int			i;
    int			nm_sites;
    int			normalize = 0;	/* Make all weights sum to one? */
    fastf_t		*coeff;
    fastf_t		x, y, z;
    FILE		*infp;
    struct bu_list	site_list;
    struct bu_vls	*tail_buf = 0;
    struct site		*sp;

    extern int	optind;			/* index from getopt(3C) */
    extern char	*optarg;		/* index from getopt(3C) */

    BU_LIST_INIT(&site_list);
    while ((ch = getopt(argc, argv, OPT_STRING)) != EOF)
	switch (ch)
	{
	    case 'n':
		normalize = 1;
		break;
	    case 's':
		if (sscanf(optarg, "%lf %lf %lf", &x, &y, &z) != 3)
		{
		    bu_log("Illegal site: '%s'\n", optarg);
		    print_usage();
		    exit (1);
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
		exit (ch != '?');
	}

    switch (argc - optind)
    {
	case 0:
	    inf_name = "stdin";
	    infp = stdin;
	    break;
	case 1:
	    inf_name = argv[optind++];
	    if ((infp = fopen(inf_name, "r")) == NULL)
	    {
		bu_log ("Cannot open file '%s'\n", inf_name);
		exit (1);
	    }
	    break;
	default:
	    print_usage();
	    exit (1);
    }

    if (BU_LIST_IS_EMPTY(&site_list))
    {
	enqueue_site(&site_list, (fastf_t) 1.0, (fastf_t) 0.0, (fastf_t) 0.0);
	enqueue_site(&site_list, (fastf_t) 0.0, (fastf_t) 1.0, (fastf_t) 0.0);
	enqueue_site(&site_list, (fastf_t) 0.0, (fastf_t) 0.0, (fastf_t) 1.0);
    }

    nm_sites = 0;
    for (BU_LIST_FOR(sp, site, &site_list))
	++nm_sites;

    coeff = (fastf_t *)
		bu_malloc(nm_sites * sizeof(fastf_t), "coefficient array");

    while (read_point(infp, coeff, nm_sites, normalize, tail_buf) != EOF)
    {
	x = y = z = 0.0;
	i = 0;
	for (BU_LIST_FOR(sp, site, &site_list))
	{
	    x += sp -> s_x * coeff[i];
	    y += sp -> s_y * coeff[i];
	    z += sp -> s_z * coeff[i];
	    ++i;
	}
	bu_flog(stdout, "%g %g %g", x, y, z);
	if (tail_buf)
	    bu_flog(stdout, "%s", bu_vls_addr(tail_buf));
	bu_flog(stdout, "\n");
    }
}
