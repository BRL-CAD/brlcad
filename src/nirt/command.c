/*                       C O M M A N D . C
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
/** @file command.c
 *
 */

/*      COMMAND.C       */ 
#ifndef lint
static const char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "common.h"



#include <stdio.h>
#include <ctype.h>
#if HAVE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "../librt/debug.h"
#include "./nirt.h"
#include "./usrfmt.h"

char		local_u_name[64];
double		base2local;		/* from db_i struct, not fastf_t */
double		local2base;		/* from db_i struct, not fastf_t */

extern fastf_t			bsphere_diameter;
extern int			do_backout;
extern int			silent_flag;
extern struct application	ap;
extern struct rt_i		*rti_tab[];	/* For use w/ and w/o air */
extern struct resource		res_tab[];	/* For use w/ and w/o air */
extern com_table		ComTab[];
extern outval			ValTab[];
extern int			overlap_claims;
extern char			*ocname[];
extern int			nirt_debug;

extern void printusage(void);
extern void do_rt_gettrees(struct rt_i *rtip, char **object_name, int nm_objects);

void
bot_minpieces(char *buffer, com_table *ctp )
{
	int new_value;
	int i=0;

	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')     /* display current rt_bot_minpieces */
	{
		bu_log( "rt_bot_minpieces = %d\n", rt_bot_minpieces );
		return;
	}

	new_value = atoi( buffer );

	if( new_value < 0 ) {
		bu_log( "Error: rt_bot_minpieces cannot be less than 0\n" );
		return;
	}

	if( new_value != rt_bot_minpieces ) {
		rt_bot_minpieces = new_value;
		need_prep = 1;
	}
}

void az_el(buffer, ctp)
char			*buffer;
com_table		*ctp;
{
	extern int 	str_dbl();  /* function to convert  string to  double */
	int 		i = 0;      /* current position on the *buffer        */
	int		rc = 0;     /* the return code value from str_dbl()   */
	double		az;
	double  	el;

	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')     /* display current az and el values */
	{
		bu_log("(az, el) = (%4.2f, %4.2f)\n",
		    azimuth(), elevation());
		return;
	}
	if ((rc = str_dbl(buffer+i, &az)) == 0)  /* get az value */
	{
		com_usage(ctp);
		return;
	}
	if (abs(az) > 360)       /* check for valid az value */
	{
		bu_log("Error:  |azimuth| <= 360\n"); 
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &el)) == 0)  /* get el value */
	{
		com_usage(ctp);
		return;
	}
	if (abs(el) > 90)       /* check for valid el value */
	{
		bu_log("Error:  |elevation| <= 90\n"); 
		return;
	}
	i += rc; 
	if (*(buffer+i) != '\0')  /* check for garbage at the end of the line */
	{
		com_usage(ctp);
		return;
	}
	azimuth() = az;     
	elevation() = el;
	ae2dir();
}

void sh_esc (buffer)
char	*buffer;
{
  static char	*shell = "";
  static char	*last_cmd = "";

  while (isspace(*buffer)) {
    ++buffer;
  }

  if (*buffer == '!') {
    (void) system(last_cmd);
  } else if (*buffer) {
    (void) system(buffer);
    last_cmd = buffer;
  } else {
    if ((*shell == '\0') && (shell = getenv("SHELL")) == 0) {
#ifndef _WIN32
      shell = DFLT_SHELL;
#else
      shell = "cmd.exe";
#endif
    }
    (void) system(shell);
  }
}

void grid_coor(buffer, ctp)
char			*buffer;
com_table		*ctp;
{
	extern int 	str_dbl();  /* function to convert  string to  double */
	int 		i = 0;
	int		rc = 0;    /* the return code value from str_dbl() */
	vect_t	        Gr;

	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')    /* display current grid coordinates */
	{
		bu_log("(h,v,d) = (%4.2f, %4.2f, %4.2f)\n",
			grid(HORZ) * base2local,
			grid(VERT) * base2local,
			grid(DIST) * base2local);
		return;
	}
	if ((rc = str_dbl(buffer+i, &Gr[HORZ])) == 0) /* get horz coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &Gr[VERT])) == 0) /* get vert coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')   /* if there is no dist coor, set default */
	{
		grid(HORZ) = Gr[HORZ] * local2base;
		grid(VERT) = Gr[VERT] * local2base;
		grid2targ();
		return;
	}
	if ((rc = str_dbl(buffer+i, &Gr[DIST])) == 0) /* set dist coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	if (*(buffer+i) != '\0') /* check for garbage at the end of the line */
	{
		com_usage(ctp);
		return;
	}
	grid(HORZ) = Gr[HORZ] * local2base;
	grid(VERT) = Gr[VERT] * local2base;
	grid(DIST) = Gr[DIST] * local2base;
	grid2targ();
}

void target_coor(buffer, ctp)
char			*buffer;
com_table		*ctp;
{
	extern int 	str_dbl();  /* function to convert string to double */
	int 		i = 0;
	int		rc = 0;     /* the return code value from str_dbl() */
	vect_t		Tar;	    /* Target x, y and z          	    */

	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')         /* display current target coors */
	{
		bu_log("(x,y,z) = (%4.2f, %4.2f, %4.2f)\n",
			    target(X) * base2local,
			    target(Y) * base2local,
			    target(Z) * base2local);
		return;
	}
	if ((rc = str_dbl(buffer+i, &Tar[X])) == 0)  /* get target x coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &Tar[Y])) == 0)  /* get target y coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &Tar[Z])) == 0)    /* get target z coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	if (*(buffer+i) != '\0') /* check for garbage at the end of the line */
	{
		com_usage(ctp);
		return;
	}
	target(X) = Tar[X] * local2base;
	target(Y) = Tar[Y] * local2base;
	target(Z) = Tar[Z] * local2base;
	targ2grid();
}	


void dir_vect(buffer, ctp)
char			*buffer;
com_table		*ctp;
{
	extern int 	str_dbl();  /* function to convert  string to  double */
	int 		i = 0;
	int		rc = 0;    /* the return code value from str_dbl() */
	vect_t		Dir;	   /* Direction vector x, y and z          */

	while (isspace(*(buffer+i)))
		++i;
	if (*(buffer+i) == '\0')         /* display current direct coors */
	{
		bu_log("(x,y,z) = (%4.2f, %4.2f, %4.2f)\n",
			    direct(X), direct(Y), direct(Z));
		return;
	}
	if ((rc = str_dbl(buffer+i, &Dir[X])) == 0)  /* get direct x coor */
	{
		com_usage(ctp);
		return;
        }
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &Dir[Y])) == 0)  /* get direct y coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	while (isspace(*(buffer+i)))
		++i;
	if ((rc = str_dbl(buffer+i, &Dir[Z])) == 0)    /* get direct z coor */
	{
		com_usage(ctp);
		return;
	}
	i += rc; 
	if (*(buffer+i) != '\0') /* check for garbage at the end of the line */
	{
		com_usage(ctp);
		return;
	}
	VUNITIZE( Dir );
	direct(X) = Dir[X];
	direct(Y) = Dir[Y];
	direct(Z) = Dir[Z];
        dir2ae();
}

void quit()
{
	if (silent_flag != SILENT_YES)
	    (void) fputs("Quitting...\n", stdout);
	exit (0);
}

void show_menu(buffer)
char	*buffer;
{
	com_table	*ctp;

	for (ctp = ComTab; ctp -> com_name; ++ctp)
	    (void) bu_log("%*s %s\n", -14, ctp -> com_name, ctp -> com_desc);
}

void shoot(buffer, ctp)
char			*buffer;
int			ctp;
{
    int		i;

    extern void	init_ovlp();

    if (need_prep) {
	if (rtip) rt_clean(rtip);
	do_rt_gettrees(rtip, NULL, 0);
    }



    if (do_backout)
    {
	backout();
#if 0
	do_backout = 0;
#endif
    }

    for (i = 0; i < 3; ++i)
    {
	ap.a_ray.r_pt[i] = target(i);
	ap.a_ray.r_dir[i] = direct(i);
    }

    init_ovlp();
    (void) rt_shootray( &ap );
}

void use_air(buffer, ctp)

char			*buffer;
com_table		*ctp;

{
    int			new_use = 0;      /* current position on the *buffer */
    char		response[128];
    char		*rp = response;
    char		db_title[TITLE_LEN+1];	/* title from MGED database */
    struct rt_i		*rtip;

    extern char	*db_name;		/* Name of MGED database file */

    while (isspace(*buffer))
	    ++buffer;
    if (*buffer == '\0')     /* display current value of use_of_air */
    {
	bu_log("use_air = %d\n", ap.a_rt_i -> useair);
	return;
    }
    if (!isdigit(*buffer))
    {
	com_usage(ctp);
	return;
    }
    while (isdigit(*buffer))
    {
	new_use *= 10;
	new_use += *buffer++ - '0';
    }
    if (new_use && (new_use != 1))
    {
	bu_log("Warning: useair=%d specified, will set to 1\n",
	    new_use);
	new_use = 1;
    }
    if (rti_tab[new_use] == RTI_NULL)
    {
	bu_log(" Air %s in the current directory of database objects.\n",
	    new_use ? "is not included" : "is included");
	bu_log(
	    " To set useair=%d requires building/prepping another directory.\n",
	    new_use);
	bu_log(" Do you want to do that now (y|n)[n]? ");
	fgets(response, sizeof(response), stdin);
	while ((*rp == ' ') || (*rp == '\t'))
	    ++rp;
	if ((*rp != 'y') && (*rp != 'Y'))
	{
	    bu_log("useair remains %d\n", ap.a_rt_i -> useair);
	    return;
	}
	bu_log("Building the directory...");
	if ((rtip = rt_dirbuild( db_name , db_title, TITLE_LEN )) == RTI_NULL)
	{
	    bu_log("Could not load file %s\n", db_name);
	    printusage();
	    exit(1);
	}
	rti_tab[new_use] = rtip;
	rtip -> useair = new_use;
	rtip -> rti_save_overlaps = (overlap_claims > 0);

	bu_log("Prepping the geometry...");
	do_rt_gettrees(rtip, NULL, 0);
    }
    ap.a_rt_i = rti_tab[new_use];
    ap.a_resource = &res_tab[new_use];
    set_diameter(ap.a_rt_i);
}

void nirt_units (buffer, ctp)

char		*buffer;
com_table	*ctp;

{
    double		tmp_dbl;
    int			i = 0;      /* current position on the *buffer */
    extern struct rt_i	*rtip;

    double		mk_cvt_factor();

    while (isspace(*(buffer+i)))
	    ++i;
    if (*(buffer+i) == '\0')     /* display current destination */
    {
	bu_log("units = '%s'\n", local_u_name);
	return;
    }
    
    if (strcmp(buffer + i, "?") == 0)
    {
	com_usage(ctp);
	return;
    }
    else if (strcmp(buffer + i, "default") == 0)
    {
	base2local = rtip -> rti_dbip -> dbi_base2local;
	local2base = rtip -> rti_dbip -> dbi_local2base;
	strncpy(local_u_name, bu_units_string(base2local), 64);
    }
    else
    {
	if ((tmp_dbl = bu_units_conversion(buffer + i)) == 0.0)
	{
	    bu_log("Invalid unit specification: '%s'\n", buffer + i);
	    return;
	}
	strncpy(local_u_name, buffer + i, 64);
	local2base = tmp_dbl;
	base2local = 1.0 / tmp_dbl;
    }
}

void do_overlap_claims (buffer, ctp)

char		*buffer;
com_table	*ctp;

{
    int			i = 0;      /* current position on the *buffer */
    int			j;
    double		mk_cvt_factor();

    while (isspace(*(buffer+i)))
	    ++i;
    if (*(buffer+i) == '\0')     /* display current destination */
    {
	bu_log("overlap_claims = '%s'\n", ocname[overlap_claims]);
	return;
    }
    
    if (strcmp(buffer + i, "?") == 0)
    {
	com_usage(ctp);
	return;
    }
    for (j = OVLP_RESOLVE; j <= OVLP_RETAIN; ++j)
    {
	char	numeral[4];
	int	k;

	sprintf(numeral, "%d", j);
	if ((strcmp(buffer + i, ocname[j]) == 0)
	  || (strcmp(buffer + i, numeral) == 0))
	{
	    overlap_claims = j;
	    for (k = 0; k < 2; ++k)
		if (rti_tab[k] != RTI_NULL)
		    rti_tab[k] -> rti_save_overlaps = (j > 0);
	    return;
	}
    }

    bu_log("Invalid overlap_claims specification: '%s'\n", buffer + i);
}

void
cm_attr(buffer, ctp)
char		*buffer;
com_table	*ctp;
{
    while (isascii(*buffer) && isspace(*buffer)) buffer++;

    if (strlen(buffer) == 0) {
	com_usage(ctp);
	return;
    }

    if (! strncmp(buffer, "-p", 2) ) {
	attrib_print();
	return;
    }

    if (! strncmp(buffer, "-f", 2) ) {
	attrib_flush();
	return;
    }

    attrib_add(buffer);
}


void
cm_debug(buffer, ctp)
char		*buffer;
com_table	*ctp;
{
	register char	*cp = buffer;

	/* This is really icky -- should have argc, argv interface */
	while( *cp && isascii(*cp) && isspace(*cp) )  cp++;
	if (*cp == '\0')
	{
	    /* display current value */
	    bu_printb( "debug ", nirt_debug, DEBUG_FMT );
	    bu_log("\n");
	    return;
	}

	/* Set a new value */
	if (sscanf( cp, "%x", (unsigned int *)&nirt_debug ) == 1)
	{
	    bu_printb( "debug ", nirt_debug, DEBUG_FMT );
	    bu_log("\n");
	}
	else
	    com_usage(ctp);
}

void
cm_libdebug(buffer, ctp)
char		*buffer;
com_table	*ctp;
{
	register char	*cp = buffer;

	/* This is really icky -- should have argc, argv interface */
	while( *cp && isascii(*cp) && isspace(*cp) )  cp++;
	if (*cp == '\0')
	{
	    /* display current value */
	    bu_printb( "libdebug ", RT_G_DEBUG, RT_DEBUG_FMT );
	    bu_log("\n");
	    return;
	}

	/* Set a new value */
	if (sscanf( cp, "%x", (unsigned int *)&rt_g.debug ) == 1)
	{
	    bu_printb( "libdebug ", RT_G_DEBUG, RT_DEBUG_FMT );
	    bu_log("\n");
	}
	else
	    com_usage(ctp);
}

void backout(buffer, ctp)
char			*buffer;
int			ctp;
{
    int		i;

    int		(*phc)();	/* Previous hit callback */
    int		(*pmc)();	/* Previous miss callback */
    int		(*poc)();	/* Previous overlap callback */
    int		if_bhit();	/* Backout hit callback */
    int		if_bmiss();	/* Backout miss callback */
    int		if_boverlap();	/* Backout overlap callback */
    /*
     *	Record previous callbacks
     */
    phc = ap.a_hit;
    pmc = ap.a_miss;
    poc = ap.a_overlap;

    /*
     *	Prepare to fire the backing-out ray
     */
    for (i = 0; i < 3; ++i)
    {
	ap.a_ray.r_pt[i] = target(i);
	ap.a_ray.r_dir[i] = -direct(i);
    }
    ap.a_hit = if_bhit;
    ap.a_miss = if_bmiss;
    ap.a_overlap = if_boverlap;
    if (nirt_debug & DEBUG_BACKOUT)
	bu_log("Backing out from (%g %g %g) via (%g %g %g)\n",
	       ap.a_ray.r_pt[0] * base2local,
	       ap.a_ray.r_pt[1] * base2local,
	       ap.a_ray.r_pt[2] * base2local,
	       V3ARGS(ap.a_ray.r_dir));

    (void) rt_shootray( &ap );

    /*
     *	Reset the callbacks the way we found them
     */
    ap.a_hit = phc;
    ap.a_miss = pmc;
    ap.a_overlap = poc;
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
