/*                      O S L _ R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file osl_rt.cpp
 *
 * Brief description
 *
 */

#include "osl-renderer.h"
#include <vector>

/* BRL-CAD headers */
#include "common.h"
#include "bio.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"

using namespace OpenImageIO;

static std::string outputfile = "image.png";
static int w = 1024, h = 768;
static int samples = 4;
static unsigned short Xi[3];

inline double clamp(double x) { return x<0 ? 0 : x>1 ? 1 : x; }

struct Ray2 {
    Vec3 o, d;

    Ray2(Vec3 o_, Vec3 d_)
	: o(o_), d(d_) {}
};

typedef Imath::Vec3<double> Vec3d;

void write_image(float *buffer, int w, int h)
{
    // write image using OIIO
    ImageOutput *out = ImageOutput::create(outputfile);
    ImageSpec spec(w, h, 3, TypeDesc::UINT8);

    out->open(outputfile, spec);
    out->write_image(TypeDesc::FLOAT, buffer);

    out->close();
    delete out;
}

/* Copy of OSL specific (just for tests) */
struct osl_specific {
    long magic;              	 
    struct bu_vls shadername;
};

/**
 * rt_shootray() was told to call this on a hit.
 *
 * This callback routine utilizes the application structure which
 * describes the current state of the raytrace.
 *
 * This callback routine is provided a circular linked list of
 * partitions, each one describing one in and out segment of one
 * region for each region encountered.
 *
 * The 'segs' segment list is unused in this example.
 */
int
hit(struct application *ap, struct partition *PartHeadp, struct seg *UNUSED(segs))
{
    /* iterating over partitions, this will keep track of the current
     * partition we're working on.
     */
    struct partition *pp;

    /* will serve as a pointer for the entry and exit hitpoints */
    struct hit *hitp;

    /* will serve as a pointer to the solid primitive we hit */
    struct soltab *stp;

    /* will contain surface curvature information at the entry */
    struct curvature cur;

    /* will contain our hit point coordinate */
    point_t pt;

    /* will contain normal vector where ray enters geometry */
     vect_t inormal;

    /* will contain normal vector where ray exits geometry */
    vect_t onormal;

    /* iterate over each partition until we get back to the head.
     * each partition corresponds to a specific homogeneous region of
     * material.
     */
    for (pp=PartHeadp->pt_forw; pp != PartHeadp; pp = pp->pt_forw) {

	/* print the name of the region we hit as well as the name of
	 * the primitives encountered on entry and exit.
	 */
	bu_log("\n--- Hit region %s (in %s, out %s)\n",
	       pp->pt_regionp->reg_name,
	       pp->pt_inseg->seg_stp->st_name,
	       pp->pt_outseg->seg_stp->st_name );

	/* entry hit point, so we type less */
	hitp = pp->pt_inhit;

	/* construct the actual (entry) hit-point from the ray and the
	 * distance to the intersection point (i.e., the 't' value).
	 */
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	/* primitive we encountered on entry */
	stp = pp->pt_inseg->seg_stp;

	/* compute the normal vector at the entry point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);

	/* print the entry hit point info */
	rt_pr_hit("  In", hitp);
	VPRINT(   "  Ipoint", pt);
	VPRINT(   "  Inormal", inormal);

	/* This next macro fills in the curvature information which
	 * consists on a principle direction vector, and the inverse
	 * radii of curvature along that direction and perpendicular
	 * to it.  Positive curvature bends toward the outward
	 * pointing normal.
	 */
	RT_CURVATURE(&cur, hitp, pp->pt_inflip, stp);

	/* print the entry curvature information */
	VPRINT("PDir", cur.crv_pdir);
	bu_log(" c1=%g\n", cur.crv_c1);
	bu_log(" c2=%g\n", cur.crv_c2);

	/* exit point, so we type less */
	hitp = pp->pt_outhit;

	/* construct the actual (exit) hit-point from the ray and the
	 * distance to the intersection point (i.e., the 't' value).
	 */
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);

	/* primitive we exited from */
	stp = pp->pt_outseg->seg_stp;

	/* compute the normal vector at the exit point, flipping the
	 * normal if necessary.
	 */
	RT_HIT_NORMAL(onormal, hitp, stp, &(ap->a_ray), pp->pt_outflip);

	/* print the exit hit point info */
	rt_pr_hit("  Out", hitp);
	VPRINT(   "  Opoint", pt);
	VPRINT(   "  Onormal", onormal);
    }

    /* A more complicated application would probably fill in a new
     * local application structure and describe, for example, a
     * reflected or refracted ray, and then call rt_shootray() for
     * those rays.
     */

    /* Hit routine callbacks generally return 1 on hit or 0 on miss.
     * This value is returned by rt_shootray().
     */
    return 1;
}

/**
 * This is a callback routine that is invoked for every ray that
 * entirely misses hitting any geometry.  This function is invoked by
 * rt_shootray() if the ray encounters nothing.
 */
int
miss(struct application *UNUSED(ap))
{
    return 0;
}

int main (int argc, char **argv){

    /* Every application needs one of these.  The "application"
     * structure carries information about how the ray-casting should
     * be performed.  Defined in the raytrace.h header.
     */
    struct application ap;

    /* The "raytrace instance" structure contains definitions for
     * librt which are specific to the particular model being
     * processed.  One copy exists for each model.  Defined in
     * the raytrace.h header and is returned by rt_dirbuild().
     */
    static struct rt_i *rtip;

    /* optional parameter to rt_dirbuild() what can be used to capture
     * a title if the geometry database has one set.
     */
    char title[1024] = {0};

    /* Check for command-line arguments.  Make sure we have at least a
     * geometry file and one geometry object on the command line.
     */
    if (argc < 3) {
	bu_exit(1, "Usage: %s model.g objects...\n", argv[0]);
    }

    /* Load the specified geometry database (i.e., a ".g" file).
     * rt_dirbuild() returns an "instance" pointer which describes the
     * database to be raytraced.  It also gives you back the title
     * string if you provide a buffer.  This builds a directory of the
     * geometry (i.e., a table of contents) in the file.
     */
    rtip = rt_dirbuild(argv[1], title, sizeof(title));
    if (rtip == RTI_NULL) {
	bu_exit(2, "Building the database directory for [%s] FAILED\n", argv[1]);
    }

    /* Walk the geometry trees.  Here you identify any objects in the
     * database that you want included in the ray trace by iterating
     * of the object names that were specified on the command-line.
     */
    while (argc > 2)  {
	if (rt_gettree(rtip, argv[2]) < 0)
	    bu_log("Loading the geometry for [%s] FAILED\n", argv[2]);
	argc--;
	argv++;
    }

    /* This next call gets the database ready for ray tracing.  This
     * causes some values to be precomputed, sets up space
     * partitioning, computes boudning volumes, etc.
     */
    rt_prep_parallel(rtip, 1);

    /* initialize all values in application structure to zero */
    RT_APPLICATION_INIT(&ap);

    /* your application uses the raytrace instance containing the
     * geometry we loaded.  this describes what we're shooting at.
     */
    ap.a_rt_i = rtip;

    /* Stop at the first point of intersection.
     */
    ap.a_onehit = 1;

    /* This is what callback to perform on a hit. */
    ap.a_hit = hit;

    /* This is what callback to perform on a miss. */
    ap.a_miss = miss;

    /* Camera parameters */
    Vec3 cam_o = Vec3(2.78e2, 3.71e2, -4.54e2); // Position
    Vec3 cam_d = Vec3(-2.6237e-15, -0.169642, 0.985506).normalized(); // direction
    Vec3 cam_x = Vec3(w*.5135/h, 0.0, 0.0); // x displacement vector
    Vec3 cam_y = (cam_x.cross(cam_d)).normalized()*.5135; // y displacement vector

    // Pixel matrix
    Color3 *buffer = new Color3[w*h];
    for(int i=0; i<w*h; i++)
	buffer[i] = Color3(0.0f);

    float scale = 10.0f;
    int samps = 1; // number of samples

    /* Initialize the shading system*/
    OSLRenderer oslr;

    /* Initialize each shader that will be used */
    oslr.AddShader("cornell_wall");
    oslr.AddShader("yellow");
    oslr.AddShader("emitter");

    /* Ray trace */
    for(int y=0; y<h; y++) {

	fprintf(stderr, "\rRendering5 (%d spp) %5.2f%%", samps*4, 100.*y/(h-1));

	Xi[0] = 0;
	Xi[1] = 0;
	Xi[2] = y*y*y;

	for(int x=0; x<w; x++) {

	    int i = (h - y - 1)*w + x;

	    // 2x2 subixel with tent filter
	    for(int sy = 0; sy < 2; sy++) {
		for(int sx = 0; sx < 2; sx++) {
		    Vec3 r(0.0f);

		    for(int s=0; s<samps; s++) {
			double r1 = 2*erand48(Xi);
			double r2 = 2*erand48(Xi);

			double dx = (r1 < 1)? sqrt(r1)-1: 1-sqrt(2-r1);
			double dy = (r2 < 1)? sqrt(r2)-1: 1-sqrt(2-r2);

			Vec3 d = cam_x*(((sx+.5 + dx)/2 + x)/w - .5) +
			    cam_y*(((sy+.5 + dy)/2 + y)/h - .5) + cam_d;

			Vec3 dir = d.normalized();
			Vec3 origin = cam_o + d*130;

			Color3 pixel_color(1.0);
			r += pixel_color;

			/* Shoot the ray */
			VMOVE(ap.a_ray.r_dir, dir);
			VMOVE(ap.a_ray.r_pt, origin);

			(void)rt_shootray(&ap);
		    }

		    // normalize
		    r = Vec3(clamp(r.x),
			     clamp(r.y),
			     clamp(r.z));


		    buffer[i] += r*scale*0.25f;
		}
	    }
	    // gamma correction
	    buffer[i] = Vec3(pow(buffer[i].x, 1.0/2.2),
			     pow(buffer[i].y, 1.0/2.2),
			     pow(buffer[i].z, 1.0/2.2));
	}
    }

    write_image((float*)buffer, w, h);
    delete buffer;

    return 0;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
