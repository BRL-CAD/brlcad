/*      COMMAND.C       */ 
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include "conf.h"

#include <stdio.h>
#include <ctype.h>
#if USE_STRING_H
# include <string.h>
#else
# include <strings.h>
#endif

#include "machine.h"
#include "vmath.h"
#include "raytrace.h"
#include "externs.h"
#include "../librt/debug.h"
#include "./nirt.h"
#include "./usrfmt.h"

char		*local_unit[] = {
		    "none", "mm", "cm", "m", "in", "ft", "unknown"
		};
char		local_u_name[64];
double		base2local;		/* from db_i struct, not fastf_t */
double		local2base;		/* from db_i struct, not fastf_t */

extern fastf_t			bsphere_diameter;
extern int			do_backout;
extern int			silent_flag;
extern struct application	ap;
extern struct rt_i		*rti_tab[];	/* For use w/ and w/o air */
extern struct resource		res_tab[];	/* For use w/ and w/o air */
extern struct nirt_obj		object_list;
extern com_table		ComTab[];
extern outval			ValTab[];
extern int			nirt_debug;

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

	while (isspace(*buffer))
	    ++buffer;
	if (*buffer == '!')
	    (void) system(last_cmd);
	else if (*buffer)
	{
	    (void) system(buffer);
	    last_cmd = buffer;
	}
	else
	{
	    if ((*shell == '\0') && (shell = getenv("SHELL")) == 0)
		shell = DFLT_SHELL;
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
	    (void) bu_log("%*s %s\n", -10, ctp -> com_name, ctp -> com_desc);
}

void shoot(buffer, ctp)
char			*buffer;
int			ctp;
{
    int		i;

    extern void	init_ovlp();

    if (do_backout)
    {
	backout();
	do_backout = 0;
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
    struct nirt_obj	*op;

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
	gets(response);
	while ((*rp == ' ') || (*rp == '\t'))
	    ++rp;
	if ((*rp != 'y') && (*rp != 'Y'))
	{
	    bu_log("useair remains %d\n", ap.a_rt_i -> useair);
	    return;
	}
	bu_log("Building the directory...");fflush(stderr);
	if ((rtip = rt_dirbuild( db_name , db_title, TITLE_LEN )) == RTI_NULL)
	{
	    bu_log("Could not load file %s\n", db_name);
	    printusage();
	    exit(1);
	}
	rti_tab[new_use] = rtip;
	rtip -> useair = new_use;

	bu_log("Prepping the geometry...");fflush(stderr);
	for (op = &object_list; op -> obj_next != NULL; op = op -> obj_next)
	    do_rt_gettree( rtip, op -> obj_name, 0);
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
	strncpy(local_u_name, local_unit[rtip -> rti_dbip -> dbi_localunit],
	    64);
	base2local = rtip -> rti_dbip -> dbi_base2local;
	local2base = rtip -> rti_dbip -> dbi_local2base;
    }
    else
    {
	if ((tmp_dbl = mk_cvt_factor(buffer + i)) == 0.0)
	{
	    bu_log("Invalid unit specification: '%s'\n", buffer + i);
	    return;
	}
	strncpy(local_u_name, buffer + i, 64);
	local2base = tmp_dbl;
	base2local = 1.0 / tmp_dbl;
    }
}
static struct cvt_tab {
	double	val;
	char	name[32];
} mk_cvt_tab[] = {
	1.0e-7,		"angstrom",
	1.0e-7,		"angstroms",
	1.0e-7,		"decinanometer",
	1.0e-7,		"decinanometers",
	1.0e-6,		"nm",
	1.0e-6,		"nanometer",
	1.0e-6,		"nanometers",
	1.0e-3,		"micron",
	1.0e-3,		"microns",
	1.0e-3,		"micrometer",
	1.0e-3,		"micrometers",
	1.0,		"mm",
	1.0,		"millimeter",
	1.0,		"millimeters",
	10.0,		"cm",
	10.0,		"centimeter",
	10.0,		"centimeters",
	1000.0,		"m",
	1000.0,		"meter",
	1000.0,		"meters",
	1000000.0,	"km",
	1000000.0,	"kilometer",
	1000000.0,	"kilometers",
	25.4,		"in",
	25.4,		"inche",
	25.4,		"inches",
	304.8,		"ft",
	304.8,		"foot",
	304.8,		"feet",
	456.2,		"cubit",
	456.2,		"cubits",
	914.4,		"yd",
	914.4,		"yard",
	914.4,		"yards",
	5029.2,		"rd",
	5029.2,		"rod",
	5029.2,		"rods",
	1609344.0,	"mi",
	1609344.0,	"mile",
	1609344.0,	"miles",
	1852000.0,	"nmile",
	1852000.0,	"nautical mile",
	0.0,		"",			/* LAST ENTRY */
};

/*
 *			M K _ C V T _ F A C T O R
 *
 *  Given a string representation of a unit of distance (eg, "feet"),
 *  return the number which will convert that unit into milimeters.
 *
 *  Returns -
 *	0.0	error
 *	>0.0	success
 */
double
mk_cvt_factor(str)
char	*str;
{
	register char	*ip, *op;
	register int	c;
	register struct cvt_tab	*tp;
	char		ubuf[64];

	if( strlen(str) >= sizeof(ubuf)-1 )  str[sizeof(ubuf)-1] = '\0';

	/* Copy the given string, making it lower case */
	ip = str;
	op = ubuf;
	while( (c = *ip++) )  {
		if( !isascii(c) )
			*op++ = '_';
		else if( isupper(c) )
			*op++ = tolower(c);
		else
			*op++ = c;
	}
	*op = '\0';

	/* Search for this string in the table */
	for( tp=mk_cvt_tab; tp->name[0]; tp++ )  {
		if( ubuf[0] != tp->name[0] )  continue;
		if( strcmp( ubuf, tp->name ) != 0 )  continue;
		return( tp->val );
	}
	return(0.0);		/* Unable to find it */
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
	    rt_printb( "libdebug ", rt_g.debug, DEBUG_FORMAT );
	    bu_log("\n");
	    return;
	}

	/* Set a new value */
	if (sscanf( cp, "%x", &rt_g.debug ) == 1)
	{
	    rt_printb( "libdebug ", rt_g.debug, DEBUG_FORMAT );
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
    point_t	point;
    vect_t	direction;

    int		(*phc)();	/* Previous hit callback */
    int		(*pmc)();	/* Previous miss callback */
    int		if_bhit();	/* Backout hit callback */
    int		if_bmiss();	/* Backout miss callback */


    /*
     *	Record previous callbacks
     */
    phc = ap.a_hit;
    pmc = ap.a_miss;

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
    if (nirt_debug & DEBUG_BACKOUT)
	bu_log("Backing out from (%g %g %g) via (%g %g %g)\n",
	    V3ARGS(ap.a_ray.r_pt), V3ARGS(ap.a_ray.r_dir));

    (void) rt_shootray( &ap );

    /*
     *	Reset the callbacks the way we found them
     */
    ap.a_hit = phc;
    ap.a_miss = pmc;
}
