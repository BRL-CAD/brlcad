/*                     A R T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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
/*********************************************************************
 *
 * This software is released under the MIT license.
 *
 * Copyright (c) 2010-2013 Francois Beaune, Jupiter Jazz Limited
 * Copyright (c) 2014-2018 Francois Beaune, The appleseedhq Organization
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
/** @file art/art.cpp
 *
 * COMPILATION NOTES
 * -----------------
 * Once you have Appleseed and a corresponding Boost installed, run
 * BRL-CAD's CMake with Appleseed enabled:
 *
 * cmake .. -DAppleseed_ROOT=/path/to/appleseed -DBOOST_ROOT=/path/to/boost
 *
 * (appleseed root path should contain bin, lib and include dirs)
 *
 * On Linux, if using the prebuilt binary, you'll need:
 * export LD_LIBRARY_PATH=/path/to/appleseed/lib
 *
 * On Mac, if using the prebuild binary, you'll need:
 * DYLD_FALLBACK_LIBRARY_PATH=/path/to/appleseed/lib:/usr/lib
 *
 *
 * SHADER IMPLEMENTATION NOTES
 * ---------------------------
 * default shader "as_disney_material" is pathed from appleseed root
 * specified when building and pulled from default .oso folder user
 * specified shaders are pathed from build/output/shaders
 */

#include "common.h"
/* avoid redefining the appleseed sleep function */
#undef sleep

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#  include <malloc.h>	/* _heapchk for ARTDBG */
#endif

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic push
#endif
#if defined(__clang__)
#  pragma clang diagnostic push
#endif
#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic ignored "-Wall"
#  pragma GCC diagnostic ignored "-Wextra"
#endif
#if defined(__clang__)
#  pragma clang diagnostic ignored "-Wdocumentation"
#  pragma clang diagnostic ignored "-Wfloat-equal"
#  pragma clang diagnostic ignored "-Wunused-parameter"
#  pragma clang diagnostic ignored "-Wpedantic"
#  pragma clang diagnostic ignored "-Wignored-qualifiers"
#endif


// appleseed.renderer headers. Only include header files from renderer/api/.
#include "renderer/api/bsdf.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/material.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/surfaceshader.h"
#include "renderer/api/utility.h"
#include "renderer/api/shadergroup.h"

// appleseed.foundation headers.
#include "foundation/core/appleseed.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/math/vector.h"
#include "foundation/containers/dictionary.h"
#include "foundation/log/consolelogtarget.h"
#include "foundation/memory/autoreleaseptr.h"
#include "foundation/utility/searchpaths.h"

// appleseed header for ITileCallback - needs to be organized somewhere
#include "renderer/kernel/rendering/itilecallback.h"

#if defined(__GNUC__) && !defined(__clang__)
#  pragma GCC diagnostic pop
#endif
#if defined(__clang__)
#  pragma clang diagnostic pop
#endif


#include "vmath.h"	/* vector math macros */
#include "raytrace.h"	/* librt interface definitions */
#include "bu/app.h"
#include "bu/getopt.h"
#include "bu/vls.h"
#include "bu/version.h"
#include "art.h"
#include "rt/tree.h"
#include "ged.h"
#include "ged/commands.h"
#include "ged/defines.h"
#include "rt/db_fullpath.h"
#include "rt/calc.h"
#include "optical/defines.h"
#include "icv.h"		/* image save/convert */
#include "bu/file.h"	/* bu_file_delete */
#include "bu/mime.h"	/* BU_MIME_IMAGE_AUTO */
#include "bu/str.h"	/* BU_STR_EQUIV */

#include "brlcad_ident.h"

#include "dm.h"
#include "tile.h"


#define VERBOSE_STATS 0x00000008


struct resource* resources;
size_t samples = 25;
size_t light_intensity = 200; // make ambient light match rt

/* Light-source regions discovered while walking the .g database in
 * register_region().  build_project() turns each into an Appleseed light at
 * its model-space centroid; if none are found it falls back to a single
 * default light.  Replaces the previous single hardcoded PointLight. */
struct ArtLight {
    std::string name;
    double pos[3];
    float color[3];
    double bright;	/* raw "bright"/lt_intensity from shader (default 1.0) */
    double fract;	/* "fract"/lt_fraction; <0 => derive from bright */
};
static std::vector<ArtLight> art_lights;

static void
art_heapchk(const char* where)
{
#ifdef _WIN32
    int r = _heapchk();
    bu_log("ARTDBG: heapchk[%s] = %d (%s)\n", where, r,
	   (r == _HEAPOK) ? "OK" : "CORRUPT");
#else
    (void)where;
#endif
}
//size_t light_intensity = 30.0;
const char* global_title_file;
asf::auto_release_ptr<asr::Project> project_ptr;
struct fb* fbp = FB_NULL;


extern "C" {
    FILE* outfp = NULL;
    extern char* framebuffer;
    extern char* outputfile;
    extern char** objv;
    extern fastf_t aspect;
    extern fastf_t azimuth, elevation;
    extern fastf_t eye_backoff; /* dist from eye to center */
    extern fastf_t rt_perspective;
    extern fastf_t viewsize;
    extern int matflag;
    extern int objc;
    extern int rt_verbosity;
    extern mat_t Viewrotscale;
    extern point_t eye_model; /* model-space location of eye */
    extern ssize_t npsw;
    extern size_t width, height;
    extern struct bu_ptbl* cmd_objs;
    extern struct command_tab rt_do_tab[];
    extern mat_t model2view;
    extern mat_t view2model;
    extern point_t viewbase_model;

    /* Options parsed by the shared rt/opt.c but NOT honored by art's
     * Appleseed backend.  Declared here so art_validate_options() can
     * detect when a user set one and warn instead of silently ignoring
     * it.  All are defined in rt/opt.c (which art links); worker.c-only
     * globals (fullfloat_mode, reproject_mode) are deliberately omitted
     * since art does not link worker.c.
     */
    extern int lightmodel;		/* -l  (art is always a path tracer) */
    extern int hypersample;		/* -H */
    extern unsigned int jitter;		/* -J */
    extern int stereo;			/* -S */
    extern int use_air;			/* -U */
    extern double airdensity;		/* -m */
    extern const char *densityfile;	/* -d */
    extern int do_kut_plane;		/* -k */
    extern int sub_grid_mode;		/* -j */
    extern int cell_newsize;		/* -g / -G */
    extern int benchmark;		/* -B */
    extern int incr_mode;		/* -i */
    extern int full_incr_mode;		/* -i (fully incremental) */
    extern int top_down;		/* -t */
    extern int random_mode;		/* random-order rendering */
    extern int opencl_mode;		/* -z */
    extern int doubles_out;		/* -O */
    extern int output_is_binary;	/* +t  (default 1; 0 => text output) */
    extern int Query_one_pixel;		/* -Q */
    extern char *string_pix_start;	/* -b */
    extern int desiredframe;		/* -D */
    extern int finalframe;		/* -K (default -1) */

    size_t n_free;
    size_t n_malloc;		/* Totals at last check */
    size_t n_realloc;
    struct application APP;
    struct icv_image* bif = NULL;

    void option(const char *cat, const char *opt, const char *des, int verbose);
    void usage(const char* argv0, int verbose);
    int get_args(int argc, const char* argv[]);
}


static void
color_hook(const struct bu_structparse *sp, const char *name, void *UNUSED(base), const char *value, void *UNUSED(data))
{
    struct bu_color color = BU_COLOR_INIT_ZERO;

    BU_CK_STRUCTPARSE(sp);

    if (!sp || !name || !value || sp->sp_count != 3 || bu_strcmp("%f", sp->sp_fmt))
	bu_bomb("color_hook(): invalid arguments");

    if (!bu_color_from_str(&color, value)) {
	bu_log("ERROR: invalid color string: '%s'\n", value);
	VSETALL(color.buc_rgb, 0.0);
    }

    VMOVE(background, color.buc_rgb);
}


// holds application specific parameters
extern "C" {
    struct bu_structparse view_parse[] = {
	{"%d", 1, "samples", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%d", 1, "s", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%f", 3, "background", 0, color_hook, NULL, NULL},
	{"%f", 3, "bg", 0, color_hook, NULL, NULL},
	{"%d", 1, "light_intensity", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%d", 1, "li", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"",	0, (char *)0,	0,	BU_STRUCTPARSE_FUNC_NULL, NULL, NULL }
    };
}


/* Initializes module specific options
 * NOTE: to have an accurate usage() menu, we overwrite the indexes of all the
 * options from rt/usage.cpp which we don't support
 */
static void
init_defaults(void)
{
    /* Set the byte offsets at run time */

    light_intensity *= AmbientIntensity; // multiplying by factor

    view_parse[0].sp_offset = bu_byteoffset(samples);
    view_parse[1].sp_offset = bu_byteoffset(samples);
    view_parse[2].sp_offset = bu_byteoffset(background[0]);
    view_parse[3].sp_offset = bu_byteoffset(background[0]);
    view_parse[4].sp_offset = bu_byteoffset(light_intensity);
    view_parse[5].sp_offset = bu_byteoffset(light_intensity);

    // default output file name
    outputfile = (char*)"art.png";

    // blue background of scene
    background[0] = 0.75;
    background[1] = 0.80;
    background[2] = 1.0;

    option("", "-o filename", "Render to specified image file (e.g., image.png or image.pix)", 0);
    option("", "-F framebuffer", "Render to a framebuffer (defaults to a window)", 100);
    // option("", "-s #", "Square image size (default: 512 - implies 512x512 image)", 100);
    // option("", "-w # -n #", "Image pixel dimensions as width and height", 100);
    option("", "-C #/#/#", "Set background image color to R/G/B values (default: 191/204/255)", 0);
    // option("", "-W", "Set background image color to white", 0);
    option("", "-R", "Disable reporting of overlaps", 100);
    // option("", "-? or -h", "Display help", 1);

    option("Raytrace", "-a # -e #", "Azimuth and elevation in degrees (default: -a 0 -e 0)", 0);
    option("Raytrace", "-p #", "Perspective angle, degrees side to side (0 <= # < 180)", 100);
    // option("Raytrace", "-E #", "Set perspective eye distance from model (default: 1.414)", 100);
    option("Raytrace", "-H #", "Specify number of hypersample rays per pixel (default: 0)", 100);
    option("Raytrace", "-J #", "Specify a \"jitter\" pattern (default: 0 - no jitter)", 100);
    option("Raytrace", "-P #", "Specify number of processors to use (default: all available)", 100);
    option("Raytrace", "-T # or -T #/#", "Tolerance as distance or distance/angular", 100);

    option("Advanced", "-c \"command\"", "Run a semicolon-separated list of rt commands", 0);
    option("Advanced", "-M", "Read matrix + commands on stdin (RT 'saveview' scripts)", 100);
    option("Advanced", "-D #", "Specify starting frame number (ending is specified via -K #)", 100);
    option("Advanced", "-K #", "Specify ending frame number (starting is specified via -D #)", 100);
    option("Advanced", "-g #", "Specify grid cell (pixel) width, in millimeters", 100);
    option("Advanced", "-G #", "Specify grid cell (pixel) height, in millimeters", 100);
    option("Advanced", "-S", "Enable stereo rendering", 100);
    option("Advanced", "-U #", "Turn on air region rendering (default: 0 - off)", 100);
    option("Advanced", "-V #", "View (pixel) aspect ratio (width/height)", 100);
    option("Advanced", "-j xmin, xmax, ymin, ymax", "Only render pixels within the specified sub-rectangle", 100);
    option("Advanced", "-k xdir,ydir,zdir,dist | x,y,z,nx,ny,nz", "Specify a cutting plane for the entire render scene", 100);

    option("Developer", "-v [#]", "Specify or increase RT verbosity", 100);
    option("Developer", "-X #", "Specify RT debugging flags", 100);
    option("Developer", "-x #", "Specify librt debugging flags", 100);
    option("Developer", "-N #", "Specify libnmg debugging flags", 100);
    option("Developer", "-! #", "Specify libbu debugging flags", 100);
    option("Developer", "-, #", "Specify space partitioning algorithm", 100);
    option("Developer", "-B", "Disable randomness for \"benchmark\"-style repeatability", 100);
    option("Developer", "-b \"x y\"", "Only shoot one ray at pixel coordinates (quotes required)", 100);
    option("Developer", "-Q x, y", "Shoot one pixel with debugging; compute others without", 100);
}


/* Warn about options that the shared rt/opt.c parses but that art's
 * Appleseed backend does not act on.  Because art links the same opt.c as
 * rt, every rt flag parses successfully; without this pass those flags
 * would be silently dropped and an rt user copying a command line would
 * get a plausible-but-wrong image with no indication anything was ignored.
 *
 * Each check compares a global against its rt/opt.c default; a mismatch
 * means the user actually passed the flag.  These are warnings (not fatal)
 * so existing scripts still render -- but nothing is dropped silently.
 * Returns the number of ignored options detected.
 */
static int
art_validate_options(void)
{
    int n = 0;

#define ART_WARN(msg) do { bu_log("art: warning: %s\n", (msg)); n++; } while (0)

    /* Fundamental model differences: no path-tracer equivalent. */
    if (lightmodel != 0)
	ART_WARN("-l lighting-model selection is ignored; art always renders "
		 "with the Appleseed global-illumination path tracer "
		 "(this includes -l7 photon mapping and -l8 heat-graph)");

    if (hypersample != 0 || jitter != 0)
	ART_WARN("-H hypersample / -J jitter are ignored; control art's "
		 "path-tracer quality with -c \"set samples=<N>\" instead "
		 "(current default is 25)");

    if (use_air != 0 || airdensity != 0.0 || densityfile != NULL)
	ART_WARN("-U air-region handling, -m atmospheric haze, and -d density "
		 "file are ignored; art has no participating-media model");

    if (stereo != 0)
	ART_WARN("-S stereo rendering is not supported and is ignored");

    /* Not-yet-implemented plumbing (mappable onto Appleseed in future). */
    if (do_kut_plane != 0)
	ART_WARN("-k cutting plane is not yet supported and is ignored");

    if (sub_grid_mode != 0)
	ART_WARN("-j sub-rectangle rendering is not yet supported and is ignored");

    if (cell_newsize != 0)
	ART_WARN("-g / -G grid cell sizing is ignored; art derives resolution "
		 "from -s / -w / -n only");

    if (incr_mode != 0 || full_incr_mode != 0)
	ART_WARN("-i incremental/progressive rendering is not supported and is ignored");

    if (benchmark != 0)
	ART_WARN("-B benchmark mode is ignored; art output is not guaranteed "
		 "bit-reproducible");

    if (top_down != 0)
	ART_WARN("-t top-down scan order is ignored (art renders in tiles)");

    if (random_mode != 0)
	ART_WARN("random-order rendering is ignored (art uses Appleseed's sampler)");

    if (opencl_mode != 0)
	ART_WARN("-z OpenCL acceleration is ignored by art");

    /* Output-format divergences: art writes 8-bit images via Appleseed. */
    if (doubles_out != 0)
	ART_WARN("-O double-precision (.dpix) output is not supported; "
		 "art writes 8-bit images only");

    if (output_is_binary == 0)
	ART_WARN("+t text output is not supported and is ignored");

    /* Diagnostics with no path-tracer analogue. */
    if (Query_one_pixel != 0 || string_pix_start != NULL)
	ART_WARN("-b / -Q single-pixel debugging is not supported and is ignored");

    /* Animation: art only loops frames when driven by an -M/stdin script
     * (rt_do_tab's "end" command is remapped to art_cm_end).  In plain flag
     * mode the -D/-K frame range has no effect. */
    if (!matflag && (desiredframe != 0 || finalframe != -1))
	ART_WARN("-D / -K frame ranges are ignored in flag mode; multi-frame "
		 "animation requires an -M stdin script");

#undef ART_WARN

    if (n)
	bu_log("art: %d rt option(s) above were parsed but not applied; "
	       "the rendered image may differ from rt's.\n", n);

    return n;
}


// Define shorter namespaces for convenience.
namespace asf = foundation;
namespace asr = renderer;

/* Extract the BRL-CAD "light" shader's intensity controls from a region's
 * shader string.  We cannot use bu_struct_parse() here: liboptical's
 * light_parse table is static, and bu_struct_parse() aborts on the first key
 * it doesn't know (angle, target, visible, ...), so it would fail on real
 * light specs.  Instead tokenize manually, which is robust to unknown keys.
 *
 * Recognizes bright|b|inten -> bright, and fract|f -> fract, matching the
 * aliases in liboptical/sh_light.c.  Defaults mirror that shader: bright=1.0,
 * fract=-1.0 (meaning "derive it later").
 */
static void
art_parse_light_intensity(const struct bu_vls* shader, double* bright, double* fract)
{
    *bright = 1.0;
    *fract = -1.0;

    const char* s = bu_vls_cstr(shader);
    if (bu_strncmp(s, "light", 5) == 0)
	s += 5;		/* skip the shader name */

    struct bu_vls norm = BU_VLS_INIT_ZERO;
    bu_vls_strcpy(&norm, s);

    /* turn key/value separators into spaces so one whitespace tokenizer
     * handles both "key=value" and list "{key value}" forms */
    for (char* cp = bu_vls_addr(&norm); *cp; cp++) {
	if (*cp == '=' || *cp == '{' || *cp == '}' || *cp == ';' || *cp == ',')
	    *cp = ' ';
    }

    size_t maxtok = bu_vls_strlen(&norm) / 2 + 2;
    char** av = (char**)bu_calloc(maxtok + 1, sizeof(char*), "art light argv");
    size_t ac = bu_argv_from_string(av, maxtok, bu_vls_addr(&norm));

    for (size_t i = 0; i + 1 < ac; i++) {
	if (BU_STR_EQUAL(av[i], "bright") || BU_STR_EQUAL(av[i], "b") || BU_STR_EQUAL(av[i], "inten"))
	    *bright = atof(av[i + 1]);
	else if (BU_STR_EQUAL(av[i], "fract") || BU_STR_EQUAL(av[i], "f"))
	    *fract = atof(av[i + 1]);
    }

    bu_free(av, "art light argv");
    bu_vls_free(&norm);
}


/* db_walk_tree() callback to register all regions within the scene
 * using either a disney shader with rgb color set on combination regions
 * or specified material OSL optical shader
 */
static int
register_region(struct db_tree_state* tsp,
		const struct db_full_path* pathp,
		const struct rt_comb_internal* combp,
		void* data)
{
    if (!pathp || !combp || !data)
	return 1;
    // We open the db using the region path to get objects name
    struct directory* dp = DB_FULL_PATH_CUR_DIR(pathp);
    if (!dp)
	return 1;

    const char* name;
    name = dp->d_namep;

    // Strip the objects name to get correct bounding box
    struct bu_vls path = BU_VLS_INIT_ZERO;
    db_path_to_vls(&path, pathp);
    const char* name_full;
    std::string conversion_temp = bu_vls_cstr(&path);
    bu_vls_free(&path);
    name_full = conversion_temp.c_str();
    bu_log("name: %s\n", conversion_temp.c_str());

    // get objects bounding box
    struct ged* gedp;
    gedp = ged_open("db", tsp->ts_dbip->dbi_filename, 1);
    point_t min;
    point_t max;
    int ret = rt_obj_bounds(gedp->ged_result_str, gedp->dbip, 1, (const char**)&name_full, 1, min, max);

    bu_log("ged: %i | min: %f %f %f | max: %f %f %f\n", ret, V3ARGS(min), V3ARGS(max));

    /* If this region uses the BRL-CAD "light" shader, treat it as a light
     * source rather than renderable geometry: record its centroid and color
     * for build_project() to turn into an Appleseed light.  We intentionally
     * do NOT emit geometry for it -- otherwise the light solid would be an
     * opaque blob shadowing its own light.
     *
     * NOTE: position (region centroid) and color (region rgb) are honored;
     * per-light "bright"/"fract" intensity from the shader string is not yet
     * mapped -- all discovered lights share the light_intensity multiplier.
     */
    if (bu_vls_strlen(&combp->shader) >= 5 &&
	bu_strncmp(bu_vls_cstr(&combp->shader), "light", 5) == 0) {
	ArtLight L;
	L.name = std::string(name) + "_light";
	L.pos[0] = (min[X] + max[X]) * 0.5;
	L.pos[1] = (min[Y] + max[Y]) * 0.5;
	L.pos[2] = (min[Z] + max[Z]) * 0.5;
	if (combp->rgb_valid) {
	    L.color[0] = (float)(combp->rgb[0] / 255.0);
	    L.color[1] = (float)(combp->rgb[1] / 255.0);
	    L.color[2] = (float)(combp->rgb[2] / 255.0);
	} else {
	    L.color[0] = L.color[1] = L.color[2] = 1.0f;
	}
	art_parse_light_intensity(&combp->shader, &L.bright, &L.fract);
	art_lights.push_back(L);
	bu_log("art: light source '%s' at %g %g %g (bright=%g)\n",
	       name, L.pos[0], L.pos[1], L.pos[2], L.bright);
	ged_close(gedp);
	return 0;
    }

    /*
      create object paramArray to pass to constructor
      NOTE: we will need to eventually match brl geometry to appleseed plugin
    */
    renderer::ParamArray geometry_parameters = asr::ParamArray()
	.insert("database_path", name)
	.insert("object_path", name_full)
	.insert("object_count", objc)
	.insert("minX", min[X])
	.insert("minY", min[Y])
	.insert("minZ", min[Z])
	.insert("maxX", max[X])
	.insert("maxY", max[Y])
	.insert("maxZ", max[Z]);


    asf::auto_release_ptr<renderer::Object> brlcad_object(
	new BrlcadObject{
	    name,
	    geometry_parameters,
	    &APP, resources});

    // typecast our scene using the callback function 'data'
    asr::Scene* scene = static_cast<asr::Scene*>(data);

    // create assembly for current object
    std::string assembly_name = std::string(name) + "_object_assembly";
    asf::auto_release_ptr<asr::Assembly> assembly(
	asr::AssemblyFactory().create(
	    assembly_name.c_str(),
	    asr::ParamArray()));


    // create a shader group
    std::string shader_name = std::string(name) + "_shader";
    asf::auto_release_ptr<asr::ShaderGroup> shader_grp(
	asr::ShaderGroupFactory().create(
	    shader_name.c_str(),
	    asr::ParamArray()));

    // choose input shader
    // change to plastic to look more like rt
    const char* shader = (char*)"as_plastic";
    asr::ParamArray shader_params = asr::ParamArray();

    // check if shader was set new way
    // extract material if set and check for shader in optical properties
    struct directory* dp1;
    char* mat_name = bu_vls_strdup(&combp->material);
    dp1 = db_lookup(tsp->ts_dbip, mat_name, LOOKUP_QUIET);
    struct bu_vls m = BU_VLS_INIT_ZERO;
    struct rt_db_internal intern;
    struct rt_material_internal* material_ip;
    if (dp1 != RT_DIR_NULL) {
	if (rt_db_get_internal(&intern, dp1, tsp->ts_dbip, NULL) >= 0) {
	    if (intern.idb_minor_type == DB5_MINORTYPE_BRLCAD_MATERIAL) {
		material_ip = (struct rt_material_internal*)intern.idb_ptr;
		bu_vls_printf(&m, "%s", bu_avs_get(&material_ip->opticalProperties, "OSL"));
		if (!BU_STR_EQUAL(bu_vls_cstr(&m), "(null)")) {
		    shader = bu_vls_cstr(&m);
		    bu_log("material->optical->OSL: %s\n", shader);
		}
	    }
	}
    }

    // check for color assignment, if set add to param array
    struct bu_vls v=BU_VLS_INIT_ZERO;
    if (combp->rgb_valid) {
	bu_vls_printf(&v, "color %f %f %f\n", combp->rgb[0]/255.0, combp->rgb[1]/255.0, combp->rgb[2]/255.0);
	const char* color = bu_vls_cstr(&v);
	shader_params.insert("in_color", color);
    }

    // check if shader was set old way
    // send this to mapping function -> disney params
    /* values acceptable with phong implementation:
     * specular reflectance sp
     * diffuse reflectance di
     * roughness rms
     * transparency tr
     * transmission re
     * refraction index ri
     * extinction ex
     */
    // char* ptr;
    // char* temp_in = (char*)"   ";
    // if (bu_vls_strlen(&combp->shader) > 0) {
    //   if ((ptr=strstr(bu_vls_addr(&combp->shader), "plastic")) != NULL) {
    //     // bu_log("shader: %s\n", bu_vls_addr(&combp->shader));
    //     shader = "as_plastic";
    //   }
    //   if ((ptr=strstr(bu_vls_addr(&combp->shader), "glass")) != NULL) {
    //     // bu_log("shader: %s\n", bu_vls_addr(&combp->shader));
    //     shader = "as_glass";
    //   }
    //   // check for override parameters
    //   if (((ptr=strstr(bu_vls_addr(&combp->shader), "{")) != NULL)) {
    //     if (((ptr=strstr(bu_vls_addr(&combp->shader), "tr")) != NULL)) {
    //       int i = 3;
    // 	    while (ptr != NULL && ptr[i] != ' ') {
    //         temp_in[i-3] = ptr[i];
    //         i++;
    //       }
    //       struct bu_vls t=BU_VLS_INIT_ZERO;
    //       bu_vls_printf(&t, "tr %s\n", temp_in);
    //       const char* tr = bu_vls_cstr(&t);
    //       shader_params.insert("in_tr", tr);
    //     }
    //   }
    // }

    /* This uses an already compiled .oso shader in the form of
       type
       shader name
       layer
       paramArray
    */
    shader_grp->add_shader(
	"shader",
	shader,
	"shader_in",
	shader_params
	);
    bu_vls_free(&v);

    /* import non compiled .osl shader in the form of
       type
       name
       layer
       source
       paramArray
       note: this relies on appleseed triggering on osl compiler
    */
    // shader_grp->add_source_shader(
    //   "shader",
    //   shader_name.c_str(),
    //   "shader_in",
    //   "shader_in",
    //   asr::ParamArray()
    //);

    // add material2surface so we can map input shader to object surface
    shader_grp->add_shader(
	"surface",
	"as_closure2surface",
	"close",
	asr::ParamArray()
	);

    // connect the two shader nodes within the group
    shader_grp->add_connection(
	"shader_in",
	"out_outColor",
	"close",
	"in_input"
	);

    // add the shader group to the assembly
    assembly->shader_groups().insert(
	shader_grp
	);

    // Create a physical surface shader and insert it into the assembly.
    // This is technically not needed with the current shader implementation
    assembly->surface_shaders().insert(
	asr::PhysicalSurfaceShaderFactory().create(
	    "Material_mat_surface_shader",
	    asr::ParamArray()
	    .insert("lighting_samples", "1")
	    )
	);

    // create a material with our shader_group
    std::string material_mat = shader_name + "_mat";
    assembly->materials().insert(
	asr::OSLMaterialFactory().create(
	    material_mat.c_str(),
	    asr::ParamArray()
	    .insert("osl_surface", shader_name.c_str())
	    .insert("surface_shader", "Material_mat_surface_shader")
	    )
	);

    // insert object into object array in assembly
    assembly->objects().insert(brlcad_object);

    // create an instance for our newly created object within the assembly
    const std::string instance_name = std::string(assembly_name) + "_brlcad_inst";
    assembly->object_instances().insert(
	asr::ObjectInstanceFactory::create(
	    instance_name.c_str(),
	    asr::ParamArray(),
	    name,
	    asf::Transformd::identity(),
	    asf::StringDictionary()
	    .insert("default", material_mat.c_str())
	    ));

    // add assembly to assemblies array in scene
    scene->assemblies().insert(assembly);

    // finally, we add an instance to use the assembly in the render
    std::string assembly_inst_name = assembly_name + "_inst";
    asf::auto_release_ptr<asr::AssemblyInstance> assembly_instance(
	asr::AssemblyInstanceFactory::create(
	    assembly_inst_name.c_str(),
	    asr::ParamArray(),
	    assembly_name.c_str()
	    )
	);
    assembly_instance->transform_sequence()
	.set_transform(
	    0.0f,
	    asf::Transformd::identity());
    scene->assembly_instances().insert(assembly_instance);

    art_heapchk(name);
    return 0;
}


static void
do_ae(double azim, double elev)
{
    vect_t temp;
    vect_t diag;
    mat_t toEye;
    struct rt_i* rtip = APP.a_rt_i;

    if (rtip == NULL)
	return;

    if (rtip->stats.nsolids <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no primitives active\n");

    if (rtip->stats.nregions <= 0)
	bu_exit(EXIT_FAILURE, "ERROR: no regions active\n");

    if (rtip->mdl_max[X] >= INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit minimum\n");
	VSETALL(rtip->mdl_min, -1);
    }
    if (rtip->mdl_max[X] <= -INFINITY) {
	bu_log("do_ae: infinite model bounds? setting a unit maximum\n");
	VSETALL(rtip->mdl_max, 1);
    }

    /*
     * Enlarge the model RPP just slightly, to avoid nasty effects
     * with a solid's face being exactly on the edge NOTE: This code
     * is duplicated out of librt/tree.c/rt_prep(), and has to appear
     * here to enable the viewsize calculation to match the final RPP.
     */
    rtip->mdl_min[X] = floor(rtip->mdl_min[X]);
    rtip->mdl_min[Y] = floor(rtip->mdl_min[Y]);
    rtip->mdl_min[Z] = floor(rtip->mdl_min[Z]);
    rtip->mdl_max[X] = ceil(rtip->mdl_max[X]);
    rtip->mdl_max[Y] = ceil(rtip->mdl_max[Y]);
    rtip->mdl_max[Z] = ceil(rtip->mdl_max[Z]);

    MAT_IDN(Viewrotscale);
    bn_mat_angles(Viewrotscale, 270.0 + elev, 0.0, 270.0 - azim);

    /* Look at the center of the model */
    MAT_IDN(toEye);
    toEye[MDX] = -((rtip->mdl_max[X] + rtip->mdl_min[X]) / 2.0);
    toEye[MDY] = -((rtip->mdl_max[Y] + rtip->mdl_min[Y]) / 2.0);
    toEye[MDZ] = -((rtip->mdl_max[Z] + rtip->mdl_min[Z]) / 2.0);

    /* Fit a sphere to the model RPP, diameter is viewsize, unless
     * viewsize command used to override.
     */
    if (viewsize <= 0) {
	VSUB2(diag, rtip->mdl_max, rtip->mdl_min);
	viewsize = MAGNITUDE(diag);
	if (aspect > 1) {
	    /* don't clip any of the image when autoscaling */
	    viewsize *= aspect;
	}
    }

    /* sanity check: make sure viewsize still isn't zero in case
     * bounding box is empty, otherwise bn_mat_int() will bomb.
     */
    if (viewsize < 0 || ZERO(viewsize)) {
	viewsize = 2.0; /* arbitrary so Viewrotscale is normal */
    }

    Viewrotscale[15] = viewsize * 0.5; /* view scale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);
    VSET(temp, 0, 0, eye_backoff);
    MAT4X3PNT(eye_model, view2model, temp);
}


static asf::auto_release_ptr<asr::Project>
build_project(const char* file, const char* UNUSED(objects))
{
    /* If user gave no sizing info at all, use 512 as default */
    struct bu_vls dimensions = BU_VLS_INIT_ZERO;
    if (width <= 0)
	width = 512;
    if (height <= 0)
	height = 512;

    // Create an empty project.
    asf::auto_release_ptr<asr::Project> project(asr::ProjectFactory::create("test project"));
    project->search_paths().push_back_explicit_path("build/Debug");
    // add precompiled shaders from appleseed
    char root[MAXPATHLEN];
    project->search_paths().push_back_explicit_path(bu_dir(root, MAXPATHLEN, APPLESEED_ROOT, "shaders/appleseed", NULL));
    project->search_paths().push_back_explicit_path(bu_dir(root, MAXPATHLEN, APPLESEED_ROOT, "shaders/max", NULL));
    // add path for materialX converted shaders
    project->search_paths().push_back_explicit_path(bu_dir(root, MAXPATHLEN, BU_DIR_INIT, "output/shaders", NULL));

    // Add default configurations to the project.
    project->add_default_configurations();

    // Set the number of samples. This is the main quality parameter: the higher the
    // number of samples, the smoother the image but the longer the rendering time.
    // we overwrite via command line -c "set"
    project->configurations()
	.get_by_name("final")->get_parameters()
	.insert_path("uniform_pixel_renderer.samples", samples)
	.insert_path("rendering_threads", (size_t)npsw);
    project->configurations()
	.get_by_name("interactive")->get_parameters()
	.insert_path("rendering_threads", "1"); /* no multithreading - for debug rendering on appleseed */

    // Create a scene.
    asf::auto_release_ptr<asr::Scene> scene(asr::SceneFactory::create());
    // asr::Scene scene(asr::SceneFactory::create());

    // Create an assembly.
    asf::auto_release_ptr<asr::Assembly> assembly(
	asr::AssemblyFactory().create(
	    "assembly",
	    asr::ParamArray()));

    //------------------------------------------------------------------------
    // Materials
    //------------------------------------------------------------------------

    // walk the db to register all regions
    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    struct db_i* dbip = db_open(file, DB_OPEN_READONLY);
    state.ts_dbip = dbip;

    /* discovered lights are collected by register_region() during the walk */
    art_lights.clear();

    if (getenv("ART_SKIP_OBJECTS")) {
	bu_log("ARTDBG: skipping all region registration (experiment)\n");
    } else
    if (objc) {
	db_walk_tree(APP.a_rt_i->rti_dbip, objc, (const char**)objv, 1, &state, register_region, NULL, NULL, reinterpret_cast<void*>(scene.get()));
    }
    if (!getenv("ART_SKIP_OBJECTS") && cmd_objs) {

	size_t cmdobjc = BU_PTBL_LEN(cmd_objs);
	const char** cmdobjv = (const char**)cmd_objs->buffer;
	if (cmdobjc) {
	    db_walk_tree(APP.a_rt_i->rti_dbip, (int)cmdobjc, cmdobjv, 1, &state, register_region, NULL, NULL, reinterpret_cast<void*>(scene.get()));
	}
    }

    //------------------------------------------------------------------------
    // Light
    //------------------------------------------------------------------------

    if (art_lights.empty()) {
	// No light-source regions found in the .g database: fall back to a
	// single default point light so the scene is still lit (this preserves
	// the previous behavior for scenes without explicit lights).
	static const float LightRadiance[] = { 1.0f, 1.0f, 1.0f };
	light_intensity *= AmbientIntensity; // multiplying by factor
	// FIXME
	assembly->colors().insert(
	    asr::ColorEntityFactory::create(
		"light_intensity",
		asr::ParamArray()
		.insert("color_space", "srgb")
		.insert("multiplier", light_intensity),
		asr::ColorValueArray(3, LightRadiance)));

	// Create a point light called "light" and insert it into the assembly.
	asf::auto_release_ptr<asr::Light> light(
	    asr::PointLightFactory().create(
		"light",
		asr::ParamArray()
		.insert("intensity", "light_intensity")));
	light->set_transform(
	    asf::Transformd::from_local_to_parent(
		asf::Matrix4d::make_translation(asf::Vector3d(0.6, 2.0, 1.0))));
	assembly->lights().insert(light);
    } else {
	// Normalize intensities the way liboptical/sh_light.c does: a light's
	// fraction is bright/max_bright unless it explicitly set "fract", so the
	// brightest light gets the full light_intensity and dimmer lights scale
	// down proportionally.
	double max_bright = 0.0;
	for (size_t i = 0; i < art_lights.size(); i++) {
	    if (art_lights[i].bright > max_bright)
		max_bright = art_lights[i].bright;
	}
	if (max_bright <= 0.0)
	    max_bright = 1.0;

	// Create one Appleseed point light per BRL-CAD light-source region,
	// positioned at the region's model-space centroid with its color.
	for (size_t i = 0; i < art_lights.size(); i++) {
	    const ArtLight& L = art_lights[i];
	    double fract = (L.fract > 0.0) ? L.fract : (L.bright / max_bright);
	    double multiplier = (double)light_intensity * fract;

	    std::string color_name = L.name + "_color";
	    assembly->colors().insert(
		asr::ColorEntityFactory::create(
		    color_name.c_str(),
		    asr::ParamArray()
		    .insert("color_space", "srgb")
		    .insert("multiplier", multiplier),
		    asr::ColorValueArray(3, L.color)));

	    asf::auto_release_ptr<asr::Light> light(
		asr::PointLightFactory().create(
		    L.name.c_str(),
		    asr::ParamArray()
		    .insert("intensity", color_name.c_str())));
	    light->set_transform(
		asf::Transformd::from_local_to_parent(
		    asf::Matrix4d::make_translation(
			asf::Vector3d(L.pos[0], L.pos[1], L.pos[2]))));
	    assembly->lights().insert(light);
	}
	bu_log("art: created %zu light(s) from .g light-source region(s)\n",
	       art_lights.size());
    }

    //------------------------------------------------------------------------
    // Assembly instance
    //------------------------------------------------------------------------

    // Create an instance of the assembly and insert it into the scene.
    asf::auto_release_ptr<asr::AssemblyInstance> assembly_instance(
	asr::AssemblyInstanceFactory::create(
	    "assembly_inst",
	    asr::ParamArray(),
	    "assembly"));
    assembly_instance
	->transform_sequence()
	.set_transform(
	    0.0f,
	    asf::Transformd::identity());
    scene->assembly_instances().insert(assembly_instance);

    // Insert the assembly into the scene.
    scene->assemblies().insert(assembly);

    //------------------------------------------------------------------------
    // Environment
    //------------------------------------------------------------------------

    float float_bgcolor[ELEMENTS_PER_VECT] = {0};
    VMOVE(float_bgcolor, background);

    // Create a color called "sky_radiance" and insert it into the scene
    // to set the background color. By default we use a blue
    // background { 0.75f, 0.80f, 1.0f } *see line 153*. This can be
    // updated while running using -C and -W

    scene->colors().insert(
	asr::ColorEntityFactory::create(
	    "sky_radiance",
	    asr::ParamArray()
	    .insert("color_space", "srgb")
	    .insert("multiplier", AmbientIntensity),
	    asr::ColorValueArray(3, float_bgcolor)));

    // Create an environment EDF called "sky_edf" and insert it into the scene.
    scene->environment_edfs().insert(
	asr::ConstantEnvironmentEDFFactory().create(
	    "sky_edf",
	    asr::ParamArray()
	    .insert("radiance", "sky_radiance")));

    // Create an environment shader called "sky_shader" and insert it into the scene.
    scene->environment_shaders().insert(
	asr::EDFEnvironmentShaderFactory().create(
	    "sky_shader",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")));

    // Create an environment called "sky" and bind it to the scene.
    scene->set_environment(
	asr::EnvironmentFactory::create(
	    "sky",
	    asr::ParamArray()
	    .insert("environment_edf", "sky_edf")
	    .insert("environment_shader", "sky_shader")));

    //------------------------------------------------------------------------
    // Camera
    //------------------------------------------------------------------------

    // declare camera outside of if-else scope
    asf::auto_release_ptr<asr::Camera> camera;

    // check for perspective
    if (rt_perspective > 0.0) {
	double dim_factor = 1.0;
	struct bu_vls fov = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fov, "%lf", rt_perspective);
        // Create a pinhole camera with film dimensions
	if(width < height) {
	    bu_vls_sprintf(&dimensions, "%f %f", dim_factor * (double) width / height, dim_factor);
	} else {
	    bu_vls_sprintf(&dimensions, "%f %f", dim_factor * (double) height / width, dim_factor);
	}
        asf::auto_release_ptr<asr::Camera> pinhole(
	    asr::PinholeCameraFactory().create(
	        "camera",
	        asr::ParamArray()
	        .insert("film_dimensions", bu_vls_cstr(&dimensions))
	        .insert("horizontal_fov", bu_vls_cstr(&fov))
		));
        camera = pinhole;
    } else {
        // Create an orthographic camera with film dimensions
        bu_vls_sprintf(&dimensions, "%f %f", 2.0, 2.0);
        asf::auto_release_ptr<asr::Camera> ortho(
	    asr::OrthographicCameraFactory().create(
		"camera",
		asr::ParamArray()
		.insert("film_dimensions", bu_vls_cstr(&dimensions))
		));
        camera = ortho;
    }

    /*
      bu_log("EYE: %lf, %lf, %lf\n", V3ARGS(eye_model));
      bu_log("VIEWSIZE: %lf\n", viewsize);
      bn_mat_print("VIEWROTSCALE: ", Viewrotscale);
      bn_mat_print("model2view: ", model2view);
      bn_mat_print("view2model BEFORE: ", view2model);
    */

    /* appleseed's not happy with translation being non-homogenized */
    if (rt_perspective > 0.0) {
	view2model[MDX] = eye_model[X];
	view2model[MDY] = eye_model[Y];
	view2model[MDZ] = eye_model[Z];
	view2model[MSA] = 1.0;
    }

    camera->transform_sequence().set_transform(
	0.0f,
	asf::Transformd::from_local_to_parent(
	    asf::Matrix<double, 4, 4>::from_array(view2model)
	    ));

    float time0;
    asf::Transformd t;
    camera->transform_sequence().get_transform(0, time0, t);

    /*
    const asf::Matrix<double, 4, 4>& l2p = t.get_local_to_parent();
    const asf::Matrix<double, 4, 4>& p2l = t.get_parent_to_local();
    mat_t ml2p, mp2l;
    MAT_COPY(ml2p, l2p);
    MAT_COPY(mp2l, p2l);
      bn_mat_print("view2model AFTER: ", view2model);
      bn_mat_print("L2P TRANSFORM: ", ml2p);
      bn_mat_print("P2L TRANSFORM: ", mp2l);
    */

    // Bind the camera to the scene.
    scene->cameras().insert(camera);

    //------------------------------------------------------------------------
    // Frame
    //------------------------------------------------------------------------

    // Create a frame and bind it to the project.
    bu_vls_sprintf(&dimensions, "%zd %zd", width, height);
    project->set_frame(
	asr::FrameFactory::create(
	    "beauty",
	    asr::ParamArray()
	    .insert("camera", "camera")
	    .insert("tile_size", "32 32")
	    .insert("resolution", bu_vls_cstr(&dimensions))));

    // Bind the scene to the project.
    project->set_scene(scene);

    art_heapchk("build_project end");
    return project;
}


static int
fb_setup() {
    /* Framebuffer is desired */
    size_t xx, yy;
    int zoom;

    /* Ask for a fb big enough to hold the image, at least 512. */
    /* This is so MGED-invoked "postage stamps" get zoomed up big
     * enough to see.
     */
    xx = yy = 512;
    if (width > xx || height > yy) {
	xx = width;
	yy = height;
    }
    bu_semaphore_acquire(BU_SEM_SYSCALL);
    fbp = fb_open(framebuffer, (int)xx, (int)yy);
    bu_semaphore_release(BU_SEM_SYSCALL);
    if (fbp == FB_NULL) {
	fprintf(stderr, "rt:  can't open frame buffer\n");
	return 12;
    }

    bu_semaphore_acquire(BU_SEM_SYSCALL);
    /* If fb came out smaller than requested, do less work */
    if ((size_t)fb_getwidth(fbp) < width)
	width = fb_getwidth(fbp);
    if ((size_t)fb_getheight(fbp) < height)
	height = fb_getheight(fbp);

    /* If the fb is lots bigger (>= 2X), zoom up & center */
    if (width > 0 && height > 0) {
	zoom = (int)(fb_getwidth(fbp) / width);
	if ((size_t)fb_getheight(fbp) / height < (size_t)zoom)
	    zoom = (int)(fb_getheight(fbp) / height);
    }
    else {
	zoom = 1;
    }
    (void)fb_view(fbp, (int)(width / 2), (int)(height / 2), (int)zoom, (int)zoom);
    bu_semaphore_release(BU_SEM_SYSCALL);
    return 0;
}


extern "C" void
view_setup(struct rt_i* UNUSED(rtip))
{
    bu_bomb("In view setup, Don't call me!");
}


extern "C" void
view_2init(struct application* UNUSED(ap), char* UNUSED(framename))
{
    bu_bomb("In 2init, Don't call me!");
}


extern "C" void
view_end(struct application* UNUSED(ap)) {
    bu_bomb("In end, Don't call me!");
}


extern "C" void
view_cleanup(struct rt_i* UNUSED(rtip))
{
    bu_bomb("in cleanup, Don't call me!");
}


extern "C" void
do_run(int UNUSED(a), int UNUSED(b))
{
    bu_bomb("in run, Don't call me!");
}


static int
art_cm_clean(const int UNUSED(argc), const char** UNUSED(argv))
{
    return 0;
}


extern "C" void
memory_summary(void)
{
    if (rt_verbosity & VERBOSE_STATS) {
	size_t mdelta = bu_n_malloc - n_malloc;
	size_t fdelta = bu_n_free - n_free;
	bu_log("Additional #malloc=%zu, #free=%zu, #realloc=%zu (%zu retained)\n",
	       mdelta,
	       fdelta,
	       bu_n_realloc - n_realloc,
	       mdelta - fdelta);
    }
    n_malloc = bu_n_malloc;
    n_free = bu_n_free;
    n_realloc = bu_n_realloc;
}


static void
def_tree(struct rt_i* rtip)
{
    struct bu_vls times = BU_VLS_INIT_ZERO;

    RT_CK_RTI(rtip);

    rt_prep_timer();
    if (rt_gettrees(rtip, objc, (const char**)objv, (int)npsw) < 0) {
	bu_log("rt_gettrees(%s) FAILED\n", (objv && objv[0]) ? objv[0] : "ERROR");
    }
    (void)rt_get_timer(&times, NULL);

    if (rt_verbosity & VERBOSE_STATS)
	bu_log("GETTREE: %s\n", bu_vls_addr(&times));
    bu_vls_free(&times);
    memory_summary();
}


/* Write the rendered Appleseed frame to 'filename', honoring the file
 * extension the same way rt does -- via libicv -- so BRL-CAD-native formats
 * (.pix, .bw, .ppm, ...) work in addition to the formats Appleseed writes
 * natively.  Output goes to exactly the requested path; unlike the older
 * code there is no forced "output/" prefix, matching rt's -o semantics.
 *
 * Appleseed can only emit formats its image writer understands (png, exr,
 * ...).  For anything else we let Appleseed produce a temporary PNG -- its
 * color pipeline is trustworthy -- and convert that to the requested format
 * with libicv (icv reads the sRGB 8-bit PNG and writes by extension).
 */
static void
art_save_image(asr::Project& project, const char* filename)
{
    const char* dot = strrchr(filename, '.');

    /* Formats Appleseed writes directly: skip the round-trip. */
    if (dot && (BU_STR_EQUIV(dot, ".png") || BU_STR_EQUIV(dot, ".exr"))) {
	project.get_frame()->write_main_image(filename);
	return;
    }

    /* Otherwise render to a temporary PNG and convert via libicv. */
    struct bu_vls tmpname = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&tmpname, "%s.tmp.png", filename);
    project.get_frame()->write_main_image(bu_vls_cstr(&tmpname));

    icv_image_t* img = icv_read(bu_vls_cstr(&tmpname), BU_MIME_IMAGE_AUTO, 0, 0);
    if (img) {
	if (icv_write(img, filename, BU_MIME_IMAGE_AUTO) != 0)
	    bu_log("art: warning: could not write '%s'; PNG left at '%s'\n",
		   filename, bu_vls_cstr(&tmpname));
	else
	    (void)bu_file_delete(bu_vls_cstr(&tmpname));
	icv_destroy(img);
    } else {
	bu_log("art: warning: could not read temporary render '%s'; "
	       "leaving it in place\n", bu_vls_cstr(&tmpname));
    }
    bu_vls_free(&tmpname);
}


static int
art_cm_end(const int UNUSED(argc), const char** UNUSED(argv))
{
    struct bu_vls str = BU_VLS_INIT_ZERO;

    if (APP.a_rt_i && BU_LIST_IS_EMPTY(&APP.a_rt_i->HeadRegion)) {
	def_tree(APP.a_rt_i);		/* Load the default trees */
    }

#if 0
    vect_t work, temp;
    mat_t toEye;

    if (viewsize <= 0.0)
	bu_exit(EXIT_FAILURE, "viewsize <= 0");
    /* model2view takes us to eye_model location & orientation */
    MAT_IDN(toEye);
    MAT_DELTAS_VEC_NEG(toEye, eye_model);
    Viewrotscale[15] = 0.5 * viewsize;	/* Viewscale */
    bn_mat_mul(model2view, Viewrotscale, toEye);
    bn_mat_inv(view2model, model2view);

    VSET(work, 0, 0, 1);

    MAT3X3VEC(temp, view2model, work);

    bn_ae_vec(&azimuth, &elevation, temp);
    bu_log(
	"View: %g azimuth, %g elevation off of front view\n",
	azimuth, elevation);
#endif

    //if (Viewrotscale[15] <= 0.0) {
    //RENDERER_LOG_INFO("CALLING DO AEEEEEEEEEE\n");

    do_ae(azimuth, elevation);
    // }
    asf::auto_release_ptr<asr::Project> project(build_project(global_title_file, bu_vls_cstr(&str)));

    // Create the master renderer.
    asr::DefaultRendererController renderer_controller;
    asf::SearchPaths resource_search_paths;
    std::unique_ptr<asr::MasterRenderer> renderer(
	new asr::MasterRenderer(
	    project.ref(),
	    project->configurations().get_by_name("final")->get_inherited_parameters(),
	    resource_search_paths));

    // Render the frame.
    renderer->render(renderer_controller);

    // Save the frame to disk to the requested output path, honoring its
    // extension via libicv.
    art_save_image(project.ref(), outputfile);

    // Save the project to disk.
    asr::ProjectFileWriter::write(project.ref(), "output/objects.appleseed");

    // Make sure to delete the master renderer before the project and the logger / log target.
    renderer.reset();

    // clean up resources


    return 0;
}


int
main(int argc, char **argv)
{
    bu_setlinebuf(stdout);
    bu_setlinebuf(stderr);

    bu_log("%s%s%s%s\n",
	   brlcad_ident("BRL-CAD Appleseed Ray Tracing (ART)"),
	   rt_version(),
	   bn_version(),
	   bu_version()
	);

    // Create a log target that outputs to stderr, and binds it to the renderer's global logger.
    // Eventually you will probably want to redirect log messages to your own target. For this
    // you will need to implement foundation::ILogTarget (foundation/utility/log/ilogtarget.h).
    asf::ILogTarget* log_target(asf::create_console_log_target(stderr));
    asr::global_logger().add_target(log_target);

    rt_perspective = 85; // setting the default

    // Print appleseed's version string.
    RENDERER_LOG_INFO("%s\n", asf::Appleseed::get_synthetic_version_string());

    struct rt_i* rtip;
    const char *title_file = NULL;
    //const char *title_obj = NULL;	/* name of file and first object */
    struct bu_vls str = BU_VLS_INIT_ZERO;
    //int objs_free_argv = 0;

    bu_setprogname(argv[0]);

    // initialize options and overload menu before parsing
    init_defaults();

    /* Process command line options */
    int i = get_args(argc, (const char**)argv);
    if (i < 0) {
	usage(argv[0], 0);
	return 1;
    }
    // explicitly asking for help
    else if (i == 0) {
	usage(argv[0], 99);
	return 0;
    }

    /* Warn about any rt options that parsed but that art will not honor,
     * so nothing is silently dropped. */
    art_validate_options();

    if (bu_optind >= argc) {
	RENDERER_LOG_INFO("%s: BRL-CAD geometry database not specified\n", argv[0]);
	usage(argv[0], 0);
	return 1;
    }

    title_file = argv[bu_optind];
    global_title_file = title_file;
    //title_obj = argv[bu_optind + 1];
    if (!objv) {
	objc = argc - bu_optind - 1;
	if (objc) {
	    objv = (char **)&(argv[bu_optind+1]);
	} else {
	    /* No objects in either input file or argv - try getting objs from
	     * command processing.  Initialize the table. */
	    BU_GET(cmd_objs, struct bu_ptbl);
	    bu_ptbl_init(cmd_objs, 8, "initialize cmdobjs table");
	    if (!matflag) {
		// log and gracefully exit for now
		bu_exit(EXIT_FAILURE, "No Region specified\n");
	    }
	}
    } else {
	//objs_free_argv = 1;
    }


    resources = static_cast<resource*>(bu_calloc(1, sizeof(resource) * MAX_PSW, "appleseed"));
    char title[1024] = { 0 };

    /* load the specified geometry database */
    rtip = rt_dirbuild(title_file, title, sizeof(title));
    if (rtip == RTI_NULL) {
	RENDERER_LOG_INFO("building the database directory for [%s] FAILED\n", title_file);
	return -1;
    }

    for (int ic = 0; ic < MAX_PSW; ic++) {
	rt_init_resource(&resources[ic], ic, rtip);
	RT_CK_RESOURCE(&resources[ic]);
    }

    /* print optional title */
    if (title[0]) {
	RENDERER_LOG_INFO("database title: %s\n", title);
    }

    /* initialize values in application struct */
    RT_APPLICATION_INIT(&APP);

    /* configure raytrace application */
    APP.a_rt_i = rtip;
    APP.a_onehit = 1;
    APP.a_hit = brlcad_hit;
    APP.a_miss = brlcad_miss;

    if (objv) {

	/* include objects from database */
	if (rt_gettrees(rtip, objc, (const char**)objv, (int)npsw) < 0) {
	    RENDERER_LOG_INFO("loading the geometry for [%s...] FAILED\n", objv[0]);
	    return -1;
	}

	/* prepare database for raytracing */
	if (rtip->needprep)
	    rt_prep_parallel(rtip, 1);
    }

    if (matflag) {
	char* buf;
	int nret;
	rt_do_tab[6].ct_func = art_cm_end;
	rt_do_tab[13].ct_func = art_cm_clean;

	while ((buf = rt_read_cmd(stdin)) != (char*)0) {
	    RENDERER_LOG_INFO("cmd: %s \n", buf);
	    nret = rt_do_cmd(APP.a_rt_i, buf, rt_do_tab);
	    bu_free(buf, "rt_read_cmd command buffer");
	    if (nret < 0)
		break;
	}
	bu_free(resources, "appleseed");
	return 0;
    }


    do_ae(azimuth, elevation);
    // RENDERER_LOG_INFO("View model: (%f, %f, %f)", eye_model[0], eye_model[2], -eye_model[1]);

    bu_vls_from_argv(&str, objc, (const char**)objv);

    // Build the project.
    asf::auto_release_ptr<asr::Project> project(build_project(title_file, bu_vls_cstr(&str)));

    if (framebuffer && !fbp) {
	RENDERER_LOG_INFO("FRAMEBUFFER IS ON\n");
	fb_setup();
    }
    ArtTileCallback artcallback;
    // Create the master renderer.
    asr::DefaultRendererController renderer_controller;
    asf::SearchPaths resource_search_paths;
    if (framebuffer) { // if -F is called
        std::unique_ptr<asr::MasterRenderer> renderer(
	    new asr::MasterRenderer(
		project.ref(),
		project->configurations().get_by_name("final")->get_inherited_parameters(),
		resource_search_paths,
		&artcallback));

        // Render the frame.
        art_heapchk("pre-render");
        bu_log("ARTDBG: pre-render\n");
        renderer->render(renderer_controller);
        bu_log("ARTDBG: post-render\n");

        // Save the frame to disk to the requested output path, honoring its
        // extension via libicv.
        art_save_image(project.ref(), outputfile);

        // Save the project to disk.
        asr::ProjectFileWriter::write(project.ref(), "output/objects.appleseed");

        if (fbp != FB_NULL) {
	    fb_close(fbp);
        }

        // Make sure to delete the master renderer before the project and the logger / log target.
        renderer.reset();
    } else { // if -F is not specified
        std::unique_ptr<asr::MasterRenderer> renderer(
	    new asr::MasterRenderer(
		project.ref(),
		project->configurations().get_by_name("final")->get_inherited_parameters(),
		resource_search_paths));

        // Render the frame.
        art_heapchk("pre-render");
        bu_log("ARTDBG: pre-render\n");
        renderer->render(renderer_controller);
        bu_log("ARTDBG: post-render\n");

        // Save the frame to disk to the requested output path, honoring its
        // extension via libicv.
        art_save_image(project.ref(), outputfile);

        // Save the project to disk.
        asr::ProjectFileWriter::write(project.ref(), "output/objects.appleseed");

        if (fbp != FB_NULL) {
	    fb_close(fbp);
        }

        // Make sure to delete the master renderer before the project and the logger / log target.
        renderer.reset();
    }
    // clean up resources
    bu_free(resources, "appleseed");

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
