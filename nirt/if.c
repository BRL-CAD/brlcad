/*      IF.C            */
#ifndef lint
static char RCSid[] = "$Header$";
#endif

/*	INCLUDES	*/
#include <stdio.h>
#include <math.h>
#include <machine.h>
#include <vmath.h>
#include <raytrace.h>
#include "./nirt.h"
#include "./usrfmt.h"

extern outval		ValTab[];
overlap			ovlp_list;

overlap			*find_ovlp();
void			del_ovlp();
void			init_ovlp();

int if_hit(ap, part_head)
struct application	*ap;
struct partition 	*part_head;
{
    struct partition	*part;
    char		*basename();
    fastf_t		ar = azimuth() * deg2rad;
    fastf_t		er = elevation() * deg2rad;
    fastf_t		los;
    int			i;
    overlap		*ovp;	/* the overlap record for this partition */

    fastf_t		get_obliq();


    report(FMT_RAY);
    report(FMT_HEAD);
    for (part = part_head -> pt_forw; part != part_head; part = part -> pt_forw)
    {
	RT_HIT_NORM( part->pt_inhit, part->pt_inseg->seg_stp, &ap->a_ray );
	RT_HIT_NORM( part->pt_outhit, part->pt_outseg->seg_stp, &ap->a_ray );

	/* Update the output values */
	/*
	 *	WARNING -
	 *		  target, grid, direct, az, and el
	 *		  should be updated by the functions
	 *		  in command.c as well
	 */
	for (i = 0; i < 3; ++i)
	{
	    r_entry(i) = part-> pt_inhit -> hit_point[i];
	    r_exit(i) = part-> pt_outhit -> hit_point[i];
	}
	r_entry(D) = r_entry(X) * cos(er) * cos(ar)
		    + r_entry(Y) * cos(er) * sin(ar)
		    + r_entry(Z) * sin(er);
	r_exit(D) = r_exit(X) * cos(er) * cos(ar)
		    + r_exit(Y) * cos(er) * sin(ar)
		    + r_exit(Z) * sin(er);
	ValTab[VTI_LOS].value.fval = r_entry(D) - r_exit(D);
	ValTab[VTI_REG_NAME].value.sval =
	    basename(part -> pt_regionp -> reg_name);
	ValTab[VTI_REG_ID].value.ival = part -> pt_regionp -> reg_regionid;
	ValTab[VTI_OBLIQ_IN].value.fval =
	    get_obliq(ap -> a_ray.r_dir, part -> pt_inhit -> hit_normal);
	ValTab[VTI_OBLIQ_OUT].value.fval =
	    get_obliq(ap -> a_ray.r_dir, part -> pt_outhit -> hit_normal);

	/* Do the printing for this partition */
	report(FMT_PART);

	if ((ovp = find_ovlp(part)) != OVERLAP_NULL)
	{
	    ValTab[VTI_OV_REG1_NAME].value.sval =
		basename(ovp -> reg1 -> reg_name);
	    ValTab[VTI_OV_REG1_ID].value.ival = ovp -> reg1 -> reg_regionid;
	    ValTab[VTI_OV_REG2_NAME].value.sval =
		basename(ovp -> reg2 -> reg_name);
	    ValTab[VTI_OV_REG2_ID].value.ival = ovp -> reg2 -> reg_regionid;
	    ValTab[VTI_OV_SOL_IN].value.sval =
		part -> pt_inseg -> seg_stp -> st_name;
	    ValTab[VTI_OV_SOL_OUT].value.sval =
		part -> pt_outseg -> seg_stp -> st_name;
	    for (i = 0; i < 3; ++i)
	    {
		ov_entry(i) = ovp -> in_point[i];
		ov_exit(i) = ovp -> out_point[i];
	    }
	    ov_entry(D) = target(D) - ovp -> in_dist;
	    ov_exit(D) = target(D) - ovp -> out_dist;
	    ValTab[VTI_OV_LOS].value.fval = ov_entry(D) - ov_exit(D);
	    report(FMT_OVLP);
	    del_ovlp(ovp);
	}
    }
    report(FMT_FOOT);
    if (ovlp_list.forw != &ovlp_list)
	fprintf(stderr, "Previously unreported overlaps.  Shouldn't happen\n");
    return( HIT );
}

int if_miss()
{ 
    report(FMT_RAY);
    report(FMT_MISS);
    return ( MISS );
}

/*
 *			I F _ O V E R L A P
 *
 *  Default handler for overlaps in rt_boolfinal().
 *  Returns -
 *	 0	to eliminate partition with overlap entirely
 *	!0	to retain partition in output list
 *
 *	Stolen out of:	spark.brl.mil:/m/cad/librt/bool.c
 *	Stolen by:	Paul Tanenbaum
 *	Date stolen:	29 March 1990
 */
if_overlap( ap, pp, reg1, reg2 )
register struct application	*ap;
register struct partition	*pp;
struct region			*reg1;
struct region			*reg2;

{
    overlap	*new_ovlp;

    if ((new_ovlp = (overlap *) malloc(sizeof(overlap))) == OVERLAP_NULL)
    {
	fflush(stdout);
	fprintf(stderr, "if_overlap(): Ran out of memory\n");
	exit(1);
    }

    new_ovlp -> ap = ap;
    new_ovlp -> pp = pp;
    new_ovlp -> reg1 = reg1;
    new_ovlp -> reg2 = reg2;
    new_ovlp -> in_dist = pp -> pt_inhit -> hit_dist;
    new_ovlp -> out_dist = pp -> pt_outhit -> hit_dist;
    VJOIN1(new_ovlp -> in_point, ap->a_ray.r_pt, pp->pt_inhit->hit_dist,
	ap->a_ray.r_dir );
    VJOIN1(new_ovlp -> out_point, ap->a_ray.r_pt, pp->pt_outhit->hit_dist,
	ap->a_ray.r_dir );

    /* Insert the new overlap into the list of overlaps */
    new_ovlp -> forw = ovlp_list.forw;
    new_ovlp -> backw = &ovlp_list;
    new_ovlp -> forw -> backw = new_ovlp;
    ovlp_list.forw = new_ovlp;

    return(1);
}

fastf_t get_obliq (ray, normal)

vect_t	ray;
vect_t	normal;

{
    double	cos_obl;
    fastf_t	obliquity;

    cos_obl = abs(VDOT(ray, normal) * MAGNITUDE(normal) / MAGNITUDE(ray));
    if (cos_obl < 1.001)
    { 
	if (cos_obl > 1)
		cos_obl = 1;
	obliquity = acos(cos_obl);
    }
    else
    {
	fflush(stdout);
	fprintf (stderr, "Error:  cos(obliquity) > 1\n");
	exit(1);
    }

    /* convert obliquity to degrees */
    obliquity = abs(obliquity * 180/PI); 
    if (obliquity > 90 && obliquity <= 180)
	    obliquity = 180 - obliquity;
    else if (obliquity > 180 && obliquity <= 270)
	    obliquity = obliquity - 180;
    else if (obliquity > 270 && obliquity <= 360)
	    obliquity = 360 - obliquity;
    
    return (obliquity);
}

overlap *find_ovlp (pp)

struct partition	*pp;

{
    overlap	*op;

    for (op = ovlp_list.forw; op != &ovlp_list; op = op -> forw)
    {
	if (((pp -> pt_inhit -> hit_dist <= op -> in_dist)
	    && (op -> in_dist <= pp -> pt_outhit -> hit_dist)) ||
	    ((pp -> pt_inhit -> hit_dist <= op -> out_dist)
	    && (op -> in_dist <= pp -> pt_outhit -> hit_dist)))
	    break;
    }
    return ((op == &ovlp_list) ? OVERLAP_NULL : op);
}

void del_ovlp (op)

overlap	*op;

{
    op -> forw -> backw = op -> backw;
    op -> backw -> forw = op -> forw;
    free(op);
}

void init_ovlp()
{
    ovlp_list.forw = ovlp_list.backw = &ovlp_list;
}
