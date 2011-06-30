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

#include "liboslrend.h"
#include <vector>

/* BRL-CAD headers */
#include "common.h"
#include "bio.h"
#include "vmath.h"
#include "raytrace.h"
#include "shadefuncs.h"

//#define DEB

using namespace OpenImageIO;

static std::string outputfile = "image.png";
static int w = 1024, h = 768;
static int samples = 4;
static unsigned short Xi[3];

static bool inside;

static int vmajor = 1;
static int vminor = 4;

OSLRenderer *oslr = NULL;

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

/* Get shadername from regioname */
const char* get_shadername(const char *regioname){

    if(!strcmp(regioname, "/all.g/back_wall.r"))
	return "blue";
    if(!strcmp(regioname, "/all.g/ceiling.r"))
	return "blue";
    if(!strcmp(regioname, "/all.g/floor.r"))
	return "red";
    if(!strcmp(regioname, "/all.g/left_wall.r"))
	return "blue";
    if(!strcmp(regioname, "/all.g/right_wall.r"))
	return "blue";
    // light
    if(!strcmp(regioname, "/all.g/light.r")){
	return "emitter";
    }
    // boxes
    if(!strcmp(regioname, "/all.g/short_box.r"))
	return "yellow";
    if(!strcmp(regioname, "/all.g/tall_box.r"))
	return "glass";
    //glass ball (DEBUG)
    if(!strcmp(regioname, "/all.g/ball.r")){
	return "glass2";
    }

    fprintf(stderr, "shader not found\n");
}

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
	/*

	/* Get shadername from the region */
	const char *shadername = get_shadername(pp->pt_regionp->reg_name);

	/**
	 * Fill in render information 
	 *
	 */

	RenderInfo info;

	/* Set hit point */
	if(ap->a_flag == 1){
	    hitp = pp->pt_outhit;
	}
	else {
	    hitp = pp->pt_inhit;
	}
	VJOIN1(pt, ap->a_ray.r_pt, hitp->hit_dist, ap->a_ray.r_dir);
	VMOVE(info.P, pt);
	
#ifdef DEB
	fprintf(stderr, "---------\n");
	VPRINT("Hit a sphere in ", info.P);
#endif
	
	/* Set normal at the point */
	stp = pp->pt_inseg->seg_stp;
	RT_HIT_NORMAL(inormal, hitp, stp, &(ap->a_ray), pp->pt_inflip);
	VMOVE(info.N, inormal);
      
	/* Set incidence ray direction */
	VMOVE(info.I, ap->a_ray.r_dir);
    
	/* U-V mapping stuff */
	info.u = 0;
	info.v = 0;
	VSETALL(info.dPdu, 0.0f);
	VSETALL(info.dPdv, 0.0f);
    
	/* x and y pixel coordinates */
	info.screen_x = ap->a_x;
	info.screen_y = ap->a_y;

	info.depth = ap->a_level;
	info.surfacearea = 1.0f;
    
	info.shadername = shadername;

	info.doreflection = 0;

	inside = false;
 
	Color3 weight = oslr->QueryColor(&info);
	
	if(info.doreflection){

	    /* Fire another ray */
	    struct application new_ap;
	    RT_APPLICATION_INIT(&new_ap);

	    new_ap.a_rt_i = ap->a_rt_i;
	    new_ap.a_onehit = 1;
	    new_ap.a_hit = ap->a_hit;
	    new_ap.a_miss = ap->a_miss;
	    new_ap.a_level = ap->a_level + 1;
	    new_ap.a_flag = 0;

	    VMOVE(new_ap.a_ray.r_dir, info.out_ray.dir);
	    VMOVE(new_ap.a_ray.r_pt, info.out_ray.origin);

	    /* Check if the out ray is internal */
	    Vec3 out_ray;
	    VMOVE(out_ray, new_ap.a_ray.r_dir);
	    Vec3 normal;
	    VMOVE(normal, inormal);

	    /* This next ray is from refraction */
	    if (normal.dot(out_ray) < 0.0f){

	     	Vec3 tmp;
	     	VSCALE(tmp, info.out_ray.dir, 1e-4);
	     	VADD2(new_ap.a_ray.r_pt, new_ap.a_ray.r_pt, tmp);
		new_ap.a_onehit = 1;
		new_ap.a_refrac_index = 1.5;
		new_ap.a_flag = 1;
	    }
	

#ifdef DEB

	    VPRINT("Exit point ", info.out_ray.origin);
	    VPRINT("Exit ray ", info.out_ray.dir);
	    VPRINT("Next point ", new_ap.a_ray.r_pt);
	    VPRINT("Next ray ", new_ap.a_ray.r_dir);
	    fprintf(stderr, "---------\n");
	    
#endif

	    rt_shootray(&new_ap);
	    
	    Color3 rec;
	    VMOVE(rec, new_ap.a_color);
	    
	    weight = weight*rec;
	    VMOVE(ap->a_color, weight);

	} else {
	    VMOVE(ap->a_color, weight);
	}
    }

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
	bu_exit(1, "Usage: %s [OPTIONS] model.g objects ...\n", argv[0]);
    }

    int samps = 1;                          /* default number of samples */
    /* Process options */
    if(argv[1][0] == '-'){
	if(strcmp(argv[1], "-s") == 0){
	    sscanf(argv[2], "%d", &samps);
	}
	argc--;
	argc--;
	argv++;
	argv++;
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
    
#if 0

    /* Code to find the direction from the orientation. NOT WORKING! */

    /* Data from script cornell.rt */
    fastf_t viewsize = 6.0e3;
    quat_t quat;
    quat[0] = 0.0;
    quat[1] = 9.961946980917455e-01;
    quat[2] = 8.715574274765824e-02;
    quat[3] = 0.0;
    vect_t eye_model;
    eye_model[0] = 2.780000000000000e+02;
    eye_model[1] = 3.709445330007912e+02;
    eye_model[2] = -4.544232590366239e+02;

  
    /* Piece copied from do.c @ l644 */
    mat_t rotscale;
    quat_quat2mat(rotscale, quat);
    rotscale[15] = viewsize;

    mat_t xlate;
    MAT_IDN(xlate);
    MAT_DELTAS_VEC_NEG(xlate, eye_model);

    mat_t model2view, view2model;
    bn_mat_mul(model2view, rotscale, xlate);
    bn_mat_inv(view2model, model2view);

    vect_t work, work1;
    vect_t zdir, xdir, cam_dir;
    VSET(work, 0.0, 0.0, 1.0);       /* view z-direction */
    MAT4X3VEC(zdir, view2model, work);
    VSET(work1, 1.0, 0.0, 0.0);      /* view x-direction */
    MAT4X3VEC(xdir, view2model, work1);

    VADD2(cam_dir, zdir, xdir);
    VREVERSE(cam_dir, cam_dir);

    /* Camera parameters */
    Vec3 cam_o; // Position
    Vec3 cam_d = Vec3(-2.6237e-15, -0.169642, 0.985506).normalized(); // direction

    VMOVE(cam_o, eye_model);
    cam_d = cam_d.normalized();

    Vec3 cam_x = Vec3(w*.5135/h, 0.0, 0.0); // x displacement vector
    Vec3 cam_y = (cam_x.cross(cam_d)).normalized()*.5135; // y displacement vector

    cam_o = cam_o;

#else

    Vec3 cam_o = Vec3(2.780000000000000e+02, 3.709445330007912e+02, -4.544232590366239e+02);
    Vec3 cam_d = Vec3(-2.6237e-15, -0.169642, 0.985506).normalized(); // direction
    Vec3 cam_x = Vec3(w*.5135/h, 0.0, 0.0);
    Vec3 cam_y = (cam_x.cross(cam_d)).normalized()*.5135;

    cam_o = cam_o - cam_d*600;

#endif

#ifdef DEB

    /* Debug:
       Shoot a given ray several times in a glass unit ball sphere.
     */
    
    fprintf(stderr, "DEBUGGING\n");

    cam_o = Vec3(-10.0, 0.0, 0.0);
    cam_d = Vec3(+1.0, 0.0, 0.0);
    
    /* Initialize the shading system*/
    oslr = new OSLRenderer();
    oslr->AddShader("glass2");
    
    VMOVE(ap.a_ray.r_dir, cam_d);
    VMOVE(ap.a_ray.r_pt, cam_o);
    
    for(int i=0; i<1; i++)
	rt_shootray(&ap);

    return 0;
  
#endif

    // Pixel matrix
    Color3 *buffer = new Color3[w*h];
    for(int i=0; i<w*h; i++)
	buffer[i] = Color3(0.0f);

    float scale = 10.0f;

    /* Initialize the shading system*/
    oslr = new OSLRenderer();
    
    /* Initialize each shader that may be used */
    oslr->AddShader("cornell_wall");
    oslr->AddShader("yellow");
    oslr->AddShader("emitter");
    oslr->AddShader("mirror");
    oslr->AddShader("blue");
    oslr->AddShader("red");
    oslr->AddShader("glass");

    /* Ray trace */
    for(int y=0; y<h; y++) {

	fprintf(stderr, "\rRendering %d.%d (%d samples) %5.2f%%", vmajor, vminor, samps*4, 100.*y/(h-1));

	Xi[0] = 0;
	Xi[1] = 0;
	Xi[2] = y*y*y;

	for(int x=0; x<w; x++) {

	    int i = y*w + x;

	    // 2x2 subixel with tent filter
	    for(int sy = 0; sy < 2; sy++) {
		for(int sx = 0; sx < 2; sx++) {
		    Vec3 r(0.0f);

		    for(int s=0; s<samps; s++) {
			double r1 = 2*erand48(Xi);
			double r2 = 2*erand48(Xi);

#if 0
			double dx = (r1 < 1)? sqrt(r1)-1: 1-sqrt(2-r1);
			double dy = (r2 < 1)? sqrt(r2)-1: 1-sqrt(2-r2);
			
			Vec3 d = cam_x*(((sx+.5 + dx)/2 + x)/w - .5) +
			    cam_y*(((sy+.5 + dy)/2 + y)/h - .5) + cam_d;

#else
			/* Shoot many rays in the same direction */
			Vec3 d = cam_x*(1.0*x/w - .5) +
			    cam_y*((1.0*y)/h - .5) + cam_d;
#endif

			Vec3 dir = d.normalized();
			Vec3 origin = cam_o + d*130;

			Color3 pixel_color(0.0f);

			/* Shoot the ray */
			VMOVE(ap.a_ray.r_dir, dir);
			VMOVE(ap.a_ray.r_pt, origin);

			if (rt_shootray(&ap))
			    VMOVE(pixel_color, ap.a_color);

			r = r + pixel_color*(1.0f/samps);
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
