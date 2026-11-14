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
 * Once you have Appleseed and its dependencies installed, run
 * BRL-CAD's CMake with Appleseed enabled:
 *
 * cmake .. -DAppleseed_ROOT=/path/to/appleseed
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
 * The default as_plastic material returns a closure that is connected to
 * as_closure2surface.  Both shaders come from the selected Appleseed install.
 */

#include "common.h"
/* avoid redefining the appleseed sleep function */
#undef sleep

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cctype>
#include <cstddef>
#include <cmath>
#include <exception>
#include <filesystem>
#include <iomanip>
#include <locale>
#include <limits>
#include <optional>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
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
#include "renderer/api/aov.h"
#include "renderer/api/camera.h"
#include "renderer/api/color.h"
#include "renderer/api/environment.h"
#include "renderer/api/environmentedf.h"
#include "renderer/api/environmentshader.h"
#include "renderer/api/frame.h"
#include "renderer/api/light.h"
#include "renderer/api/log.h"
#include "renderer/api/object.h"
#include "renderer/api/project.h"
#include "renderer/api/rendering.h"
#include "renderer/api/scene.h"
#include "renderer/api/texture.h"
#include "renderer/api/utility.h"

// appleseed.foundation headers.
#include "foundation/core/appleseed.h"
#include "foundation/image/canvasproperties.h"
#include "foundation/math/matrix.h"
#include "foundation/math/scalar.h"
#include "foundation/math/transform.h"
#include "foundation/math/vector.h"
#include "foundation/containers/dictionary.h"
#include "foundation/log/consolelogtarget.h"
#include "foundation/log/ilogtarget.h"
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
#include "materials.h"
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
#include "rt/mater.h"
#include "rt/calc.h"
#include "optical/defines.h"
#include "icv.h"		/* image save/convert */
#include "bu/file.h"	/* bu_file_delete */
#include "bu/path.h"
#include "bu/mime.h"	/* BU_MIME_IMAGE_AUTO */
#include "bu/str.h"	/* BU_STR_EQUIV */

#include "brlcad_ident.h"

#include "dm.h"
#include "tile.h"
#include "render_control.h"
#include "aov_output.h"
#include "display_exposure.h"


#define VERBOSE_STATS 0x00000008


static int inspect_materials = 0;
static int render_progress = 1;

static int samples = 25;
static double light_intensity = 200.0;

namespace {

enum class ArtBackgroundSource { Inherit, Color, Environment };

static const char *
art_background_source_name(ArtBackgroundSource source)
{
    switch (source) {
	case ArtBackgroundSource::Color: return "color";
	case ArtBackgroundSource::Environment: return "environment";
	case ArtBackgroundSource::Inherit: return "inherit";
    }
    return "inherit";
}

struct ArtEnvironmentControls {
    std::string map;
    std::string map_colorspace = "linear";
    double rotation = 0.0;
    ArtBackgroundSource background_source = ArtBackgroundSource::Inherit;
    std::optional<std::array<double, 3>> environment_color;
    std::optional<std::array<double, 3>> background_color;
    std::optional<double> environment_intensity;
    std::optional<double> background_intensity;

    bool build(renderer::Scene& scene, const foundation::SearchPaths& paths,
	const std::array<double, 3>& legacy_color, double ambient,
	std::ostream *report) const;
};
ArtEnvironmentControls environment_controls;

enum class ArtSamplingMode { Uniform, Adaptive };

struct ArtRenderSettings {
    ArtSamplingMode sampling = ArtSamplingMode::Uniform;
    int min_samples = 0;
    int batch_size = 16;
    double noise_threshold = 0.1;
    foundation::LogMessage::Category log_level = foundation::LogMessage::Info;
    std::optional<int> max_bounces;
    std::optional<int> max_diffuse_bounces;
    std::optional<int> max_glossy_bounces;
    std::optional<int> max_specular_bounces;
    std::optional<int> rr_min_path_length;
    std::optional<int> next_event_estimation;

    bool valid_budget(int budget) const {
	return sampling != ArtSamplingMode::Adaptive || min_samples <= budget;
    }
    void apply(renderer::ParamArray& parameters, int budget) const;
    void write(std::ostream& output) const;
};
ArtRenderSettings render_settings;
ArtAOVOutput aov_output;
double display_exposure = 0.0;

struct ArtRenderIntegerSetting {
    const char *name;
    const char *renderer_path;
    int minimum;
    int maximum;
    std::optional<int> ArtRenderSettings::*value;
};
constexpr int MAX_RENDER_INTEGER = std::numeric_limits<int>::max();
constexpr ArtRenderIntegerSetting render_integer_settings[] = {
    {"max_bounces", "pt.max_bounces", -1, MAX_RENDER_INTEGER, &ArtRenderSettings::max_bounces},
    {"max_diffuse_bounces", "pt.max_diffuse_bounces", -1, MAX_RENDER_INTEGER, &ArtRenderSettings::max_diffuse_bounces},
    {"max_glossy_bounces", "pt.max_glossy_bounces", -1, MAX_RENDER_INTEGER, &ArtRenderSettings::max_glossy_bounces},
    {"max_specular_bounces", "pt.max_specular_bounces", -1, MAX_RENDER_INTEGER, &ArtRenderSettings::max_specular_bounces},
    {"rr_min_path_length", "pt.rr_min_path_length", 0, MAX_RENDER_INTEGER, &ArtRenderSettings::rr_min_path_length},
    {"next_event_estimation", "pt.next_event_estimation", 0, 1, &ArtRenderSettings::next_event_estimation}
};
constexpr std::pair<const char *, foundation::LogMessage::Category> render_log_levels[] = {
    {"debug", foundation::LogMessage::Debug}, {"info", foundation::LogMessage::Info},
    {"warning", foundation::LogMessage::Warning}, {"error", foundation::LogMessage::Error}
};
// Stay within the adaptive renderer's advertised threshold range.
constexpr double MIN_RENDER_NOISE_THRESHOLD = 0.0001;
constexpr double MAX_RENDER_NOISE_THRESHOLD = 10000.0;

static const ArtRenderIntegerSetting *
art_render_integer_setting(const std::string& name)
{
    for (const auto& setting : render_integer_settings)
	if (name == setting.name)
	    return &setting;
    return nullptr;
}

void
ArtRenderSettings::apply(renderer::ParamArray& parameters, int budget) const
{
    parameters.insert("tile_renderer", sampling == ArtSamplingMode::Adaptive ? "adaptive" : "generic");
    if (sampling == ArtSamplingMode::Adaptive) {
	parameters.insert_path("adaptive_tile_renderer.min_samples", min_samples)
	    .insert_path("adaptive_tile_renderer.max_samples", budget)
	    .insert_path("adaptive_tile_renderer.batch_size", batch_size)
	    .insert_path("adaptive_tile_renderer.noise_threshold", noise_threshold);
    }
    for (const auto& setting : render_integer_settings) {
	const auto& number = this->*(setting.value);
	if (number)
	    parameters.insert_path(setting.renderer_path, *number);
    }
}

void
ArtRenderSettings::write(std::ostream& output) const
{
    output << "sampling=" << (sampling == ArtSamplingMode::Adaptive ? "adaptive" : "uniform")
	<< "\nmin_samples=" << min_samples << "\nbatch_size=" << batch_size
	<< "\nnoise_threshold=" << std::setprecision(std::numeric_limits<double>::max_digits10)
	<< noise_threshold << '\n';
    for (const auto& setting : render_integer_settings) {
	const auto& number = this->*(setting.value);
	output << setting.name << '=' << (number ? std::to_string(*number) : "inherit") << '\n';
    }
    for (const auto& level : render_log_levels)
	if (log_level == level.second)
	    output << "renderer_log=" << level.first << '\n';
}


} // namespace

static bool
art_valid_power(double value)
{
    return std::isfinite(value) && value >= 0.0 &&
	value <= std::numeric_limits<float>::max() &&
	(value <= 0.0 || static_cast<float>(value) > 0.0);
}


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

const char* global_title_file;
asf::auto_release_ptr<asr::Project> project_ptr;


extern "C" {
    FILE* outfp = NULL;
    extern char* framebuffer;
    extern char* outputfile;
    extern char** objv;
    extern fastf_t azimuth, elevation;
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
    extern struct bu_structparse set_parse[];
    int cm_set(const int argc, const char **argv);
    extern void do_view_finalize(double azim, double elev);

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
    extern int curframe;

    size_t n_free;
    size_t n_malloc;		/* Totals at last check */
    size_t n_realloc;
    struct application APP;
    struct icv_image* bif = NULL;

    void option(const char *cat, const char *opt, const char *des, int verbose);
    void usage(const char* argv0, int verbose);
    int get_args(int argc, const char* argv[]);
}


// holds application specific parameters
extern "C" {
    struct bu_structparse view_parse[] = {
	{"%d", 1, "samples", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%d", 1, "s", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%f", 3, "background", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%f", 3, "bg", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%g", 1, "light_intensity", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%g", 1, "li", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%d", 1, "inspect_materials", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
	{"%d", 1, "progress", 0, BU_STRUCTPARSE_FUNC_NULL, NULL, NULL},
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

    light_intensity *= AmbientIntensity;

    view_parse[0].sp_offset = bu_byteoffset(samples);
    view_parse[1].sp_offset = bu_byteoffset(samples);
    view_parse[2].sp_offset = bu_byteoffset(background[0]);
    view_parse[3].sp_offset = bu_byteoffset(background[0]);
    view_parse[4].sp_offset = bu_byteoffset(light_intensity);
    view_parse[5].sp_offset = bu_byteoffset(light_intensity);
    view_parse[6].sp_offset = bu_byteoffset(inspect_materials);
    view_parse[7].sp_offset = bu_byteoffset(render_progress);

    // default output file name
    outputfile = (char*)"art.png";

    /* Neutral (white) sky/environment by default.  The environment is art's
     * dominant light (a ConstantEnvironmentEDF built from `background`), so a
     * blue sky tinted every surface cool and desaturated hues away from what rt
     * produces under its white default light.  A neutral dome keeps region
     * colors true; -C/-W still override this at runtime. */
    background[0] = 1.0;
    background[1] = 1.0;
    background[2] = 1.0;

    option("", "-o filename", "Render to specified image file (e.g., image.png or image.pix)", 0);
    option("", "-F framebuffer", "Render to a framebuffer (defaults to a window)", 0);
    // option("", "-s #", "Square image size (default: 512 - implies 512x512 image)", 100);
    // option("", "-w # -n #", "Image pixel dimensions as width and height", 100);
    option("", "-C #/#/#", "Set legacy background/environment color to R/G/B values (default: white)", 0);
    // option("", "-W", "Set background image color to white", 0);
    option("", "-R", "Disable reporting of overlaps", 100);
    // option("", "-? or -h", "Display help", 1);

    option("Raytrace", "-a # -e #", "Azimuth and elevation in degrees (default: -a 0 -e 0)", 0);
    option("Raytrace", "-p #", "Perspective angle in degrees (default: 0, orthographic)", 0);
    // option("Raytrace", "-E #", "Set perspective eye distance from model (default: 1.414)", 100);
    option("Raytrace", "-H #", "Specify number of hypersample rays per pixel (default: 0)", 100);
    option("Raytrace", "-J #", "Specify a \"jitter\" pattern (default: 0 - no jitter)", 100);
    option("Raytrace", "-P #", "Specify number of processors to use (default: all available)", 100);
    option("Raytrace", "-T # or -T #/#", "Tolerance as distance or distance/angular", 100);

    option("Advanced", "-c \"command\"", "Run rt commands; set inspect_materials=1 to inspect without rendering", 0);
    option("Advanced", "-M", "Read matrix + commands on stdin (RT 'saveview' scripts)", 0);
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


static void
art_region_color(const struct db_tree_state *state, float rgb[ELEMENTS_PER_VECT])
{
    struct region resolved = {};
    BU_LIST_INIT_MAGIC(&resolved.l, RT_REGION_MAGIC);
    resolved.reg_regionid = state->ts_regionid;
    resolved.reg_mater = state->ts_mater;
    // Match librt's table fallback after explicit and inherited colors.
    if (!resolved.reg_mater.ma_color_valid)
	db_mater_color_region(state->ts_dbip, &resolved);
    for (size_t channel = 0; channel < ELEMENTS_PER_VECT; ++channel)
	rgb[channel] = resolved.reg_mater.ma_color_valid ?
	    static_cast<float>(resolved.reg_mater.ma_color[channel]) : 1.0f;
}


struct ArtSceneBuilder {
    asr::Scene &scene;
    const asf::SearchPaths &search_paths;
    std::ostream *report;
    std::set<std::string> region_paths = {};
    bool valid = true;
};


/* Share the full-path material walk between inspection and rendering. */
static int
register_region_impl(struct db_tree_state* tsp,
		const struct db_full_path* pathp,
		const struct rt_comb_internal* combp,
		void* data)
{
    if (!pathp || !combp || !data)
	return 1;
    auto &builder = *static_cast<ArtSceneBuilder *>(data);
    struct bu_vls path = BU_VLS_INIT_ZERO;
    db_path_to_vls(&path, pathp);
    const std::string region_path = bu_vls_cstr(&path);
    bu_vls_free(&path);
    const char *name = region_path.c_str();

    // Distinct paths, including repeated-arc indices, are distinct instances.
    if (!builder.region_paths.insert(region_path).second)
	return 0;

    // Match librt's default air exclusion before resolving materials or objects.
    if (tsp->ts_aircode != 0) {
	if (builder.report)
	    *builder.report << "air_region " << std::quoted(region_path) << " skipped\n";
	return 0;
    }

    float region_rgb[ELEMENTS_PER_VECT];
    art_region_color(tsp, region_rgb);

    // Legacy light geometry becomes a point light.  Rendering the solid
    // would make it shadow its own source.
    if (bu_vls_strlen(&combp->shader) >= 5 &&
	bu_strncmp(bu_vls_cstr(&combp->shader), "light", 5) == 0) {
	if (builder.report) {
	    *builder.report << "light " << std::quoted(region_path)
		<< " point shader " << std::quoted(bu_vls_cstr(&combp->shader)) << '\n';
	    return 0;
	}
	point_t min, max;
	struct bu_vls bounds_error = BU_VLS_INIT_ZERO;
	const int status = rt_obj_bounds(&bounds_error, tsp->ts_dbip, 1, &name, 1, min, max);
	if (status != 0) {
	    bu_log("art: cannot bound light region '%s': %s\n", name, bu_vls_cstr(&bounds_error));
	    bu_vls_free(&bounds_error);
	    builder.valid = false;
	    return 0;
	}
	bu_vls_free(&bounds_error);
	ArtLight L;
	L.name = std::string(name) + "_light";
	L.pos[0] = (min[X] + max[X]) * 0.5;
	L.pos[1] = (min[Y] + max[Y]) * 0.5;
	L.pos[2] = (min[Z] + max[Z]) * 0.5;
	VMOVE(L.color, region_rgb);
	art_parse_light_intensity(&combp->shader, &L.bright, &L.fract);
	art_lights.push_back(L);
	bu_log("art: light source '%s' at %g %g %g (bright=%g)\n",
	       name, L.pos[0], L.pos[1], L.pos[2], L.bright);
	return 0;
    }

    // create assembly for current object
    std::string assembly_name = std::string(name) + "_object_assembly";
    asf::auto_release_ptr<asr::Assembly> assembly(
	asr::AssemblyFactory().create(
	    assembly_name.c_str(),
	    asr::ParamArray()));


    const std::string material_name = std::string(name) + "_mat";

    if (!art_add_material(assembly.ref(), material_name.c_str(), region_rgb,
	    tsp->ts_dbip, pathp, combp, builder.search_paths, builder.report)) {
	builder.valid = false;
	return 0;
    }

    if (builder.report)
	return 0;

    const asr::ParamArray geometry_parameters = asr::ParamArray()
	.insert("database_path", tsp->ts_dbip->dbi_filename)
	.insert("object_path", name);
    asf::auto_release_ptr<asr::Object> brlcad_object(
	new BrlcadObject(name, geometry_parameters, tsp->ts_dbip));

    // insert object into object array in assembly
    assembly->objects().insert(brlcad_object);

    // create an instance for our newly created object within the assembly
    const std::string instance_name = std::string(assembly_name) + "_brlcad_inst";
    // A region material applies at both entry and exit boundaries.
    const asf::StringDictionary material_mapping = asf::StringDictionary()
	.insert("default", material_name.c_str());
    assembly->object_instances().insert(
	asr::ObjectInstanceFactory::create(
	    instance_name.c_str(),
	    asr::ParamArray(),
	    name,
	    asf::Transformd::identity(),
	    material_mapping,
	    material_mapping));

    // add assembly to assemblies array in scene
    builder.scene.assemblies().insert(assembly);

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
    builder.scene.assembly_instances().insert(assembly_instance);

    return 0;
}


static int
register_region(struct db_tree_state *state, const struct db_full_path *path,
    const struct rt_comb_internal *combination, void *data)
{
    try {
	return register_region_impl(state, path, combination, data);
    } catch (const std::exception &error) {
	bu_log("art: cannot register region: %s\n", error.what());
	static_cast<ArtSceneBuilder *>(data)->valid = false;
	return 0;
    }
}


static bool
art_camera_transform(mat_t camera_to_world)
{
    constexpr double MAX_HORIZONTAL_FOV = 180.0;
    if (!std::isfinite(viewsize) || viewsize <= 0.0) {
	bu_log("art: camera viewsize must be finite and positive\n");
	return false;
    }
    if (!std::isfinite(rt_perspective) || rt_perspective < 0.0 ||
	rt_perspective >= MAX_HORIZONTAL_FOV) {
	bu_log("art: camera perspective must be finite and in [0, 180) degrees\n");
	return false;
    }
    for (double coordinate : eye_model) {
	if (!std::isfinite(coordinate)) {
	    bu_log("art: camera eye point must be finite\n");
	    return false;
	}
    }
    for (double coefficient : Viewrotscale) {
	if (!std::isfinite(coefficient)) {
	    bu_log("art: camera orientation must be finite\n");
	    return false;
	}
    }
    for (int axis : {X, Y, Z}) {
	if (!ZERO(Viewrotscale[axis * ELEMENTS_PER_HVECT + W]) ||
	    !ZERO(Viewrotscale[W * ELEMENTS_PER_HVECT + axis])) {
	    bu_log("art: camera viewrot must contain an affine orientation\n");
	    return false;
	}
    }

    // BRL-CAD's homogeneous view scale belongs in the film dimensions.
    // Appleseed needs an affine camera pose with a model-space eye point.
    mat_t orientation;
    MAT_COPY(orientation, Viewrotscale);
    orientation[MSA] = 1.0;
    if (!bn_mat_inverse(camera_to_world, orientation)) {
	bu_log("art: camera orientation must be nonsingular\n");
	return false;
    }
    MAT_DELTAS_VEC(camera_to_world, eye_model);
    return true;
}

static std::string
art_renderer_root()
{
    char root[MAXPATHLEN];
    char shaders[MAXPATHLEN];
    const char *runtime_root = bu_dir(root, MAXPATHLEN, BU_DIR_BIN, "..", NULL);
    if (runtime_root && bu_dir(shaders, MAXPATHLEN, runtime_root, "shaders/appleseed", NULL)
	&& bu_file_directory(shaders))
	return runtime_root;

    // Development builds may use an external SDK before dependency staging.
    return APPLESEED_ROOT;
}


static int
art_selected_objects(const char ***objects)
{
    if (cmd_objs) {
	*objects = (const char **)cmd_objs->buffer;
	return static_cast<int>(BU_PTBL_LEN(cmd_objs));
    }
    *objects = (const char **)objv;
    return objc;
}


namespace {

bool
ArtEnvironmentControls::build(asr::Scene& scene, const asf::SearchPaths& paths,
    const std::array<double, 3>& legacy_color, double ambient, std::ostream *report) const
{
    const auto dome_color = environment_color.value_or(legacy_color);
    const auto backdrop_color = background_color.value_or(legacy_color);
    const double dome_power = environment_intensity.value_or(ambient);
    ArtBackgroundSource source = background_source;
    if (source == ArtBackgroundSource::Inherit) {
	const bool independent_color = background_color || background_intensity ||
	    environment_color || environment_intensity;
	source = map.empty() ? (independent_color ? ArtBackgroundSource::Color :
	    ArtBackgroundSource::Environment) : (background_color ?
	    ArtBackgroundSource::Color : ArtBackgroundSource::Environment);
    }
    const double backdrop_power = background_intensity.value_or(
	source == ArtBackgroundSource::Environment ? dome_power :
	background_color ? 1.0 : ambient);

    std::string map_path;
    if (!map.empty()) {
	try {
	    auto path = std::filesystem::u8path(map);
	    if (path.is_relative())
		path = std::filesystem::u8path(paths.get_root_path().c_str()) / path;
	    std::error_code error;
	    if (!std::filesystem::is_regular_file(path, error)) {
		bu_log("art: environment map '%s' is not a readable image file\n", map.c_str());
		return false;
	    }
	    map_path = std::filesystem::canonical(path).generic_u8string();
	    auto texture = asr::DiskTexture2dFactory().create("environment_map",
		asr::ParamArray().insert("filename", map_path)
		    .insert("color_space", map_colorspace == "linear" ? "linear_rgb" : "srgb"),
		paths);
	    const auto& properties = texture->properties();
	    if (!properties.m_canvas_width || !properties.m_canvas_height ||
		(properties.m_channel_count != 3 && properties.m_channel_count != 4)) {
		bu_log("art: environment map '%s' must have nonzero dimensions and RGB or RGBA channels\n", map.c_str());
		return false;
	    }
	    scene.textures().insert(texture);
	    scene.texture_instances().insert(asr::TextureInstanceFactory::create(
		"environment_map_inst", asr::ParamArray().insert("addressing_mode", "wrap_u_clamp_v")
		    .insert("filtering_mode", "bilinear"), "environment_map"));
	} catch (const std::exception& error) {
	    bu_log("art: cannot load environment map '%s': %s\n", map.c_str(), error.what());
	    return false;
	}
    }

    const auto add_color = [&scene](const char *name,
	const std::array<double, 3>& color, double power) {
	const float components[] = {
	    static_cast<float>(color[0]), static_cast<float>(color[1]), static_cast<float>(color[2])};
	scene.colors().insert(asr::ColorEntityFactory::create(name,
	    asr::ParamArray().insert("color_space", "srgb").insert("multiplier", power),
	    asr::ColorValueArray(3, components)));
    };
    const auto add_dome = [&](const char *name, const char *color_name, double power) {
	asf::auto_release_ptr<asr::EnvironmentEDF> edf;
	if (map.empty()) {
	    add_color(color_name, dome_color, power);
	    edf = asr::ConstantEnvironmentEDFFactory().create(name,
		asr::ParamArray().insert("radiance", color_name));
	} else {
	    edf = asr::LatLongMapEnvironmentEDFFactory().create(name,
		asr::ParamArray().insert("radiance", "environment_map_inst")
		    .insert("radiance_multiplier", power));
	    // Appleseed panoramas are Y-up.  Place north at +Z and rotate
	    // the environment about BRL-CAD's +Z axis with positive azimuth.
	    edf->transform_sequence().set_transform(0.0f,
		asf::Transformd::from_local_to_parent(
		    asf::Matrix4d::make_rotation_z(asf::deg_to_rad(rotation)) *
		    asf::Matrix4d::make_rotation_x(asf::HalfPi<double>())));
	}
	scene.environment_edfs().insert(edf);
    };
    add_dome("sky_edf", "sky_radiance", dome_power);
    if (source == ArtBackgroundSource::Color) {
	add_color("background_radiance", backdrop_color, backdrop_power);
	scene.environment_shaders().insert(asr::BackgroundEnvironmentShaderFactory().create(
	    "sky_shader", asr::ParamArray().insert("color", "background_radiance")));
    } else {
	const char *visible_edf = "sky_edf";
	if (background_intensity) {
	    add_dome("background_edf", "background_environment_radiance", backdrop_power);
	    visible_edf = "background_edf";
	}
	scene.environment_shaders().insert(asr::EDFEnvironmentShaderFactory().create(
	    "sky_shader", asr::ParamArray().insert("environment_edf", visible_edf)));
    }
    scene.set_environment(asr::EnvironmentFactory::create("sky",
	asr::ParamArray().insert("environment_edf", "sky_edf")
	    .insert("environment_shader", "sky_shader")));

    if (report) {
	if (map.empty()) {
	    *report << "environment_color " << dome_color[0] << ' ' << dome_color[1]
		<< ' ' << dome_color[2] << " intensity " << dome_power << '\n';
	} else {
	    *report << "environment_map " << std::quoted(map_path) << " colorspace "
		<< map_colorspace << " rotation " << rotation << " intensity " << dome_power << '\n';
	}
	*report << "background_source " << art_background_source_name(source) << '\n';
	if (source == ArtBackgroundSource::Color) {
	    *report << "background_color " << backdrop_color[0] << ' ' << backdrop_color[1]
		<< ' ' << backdrop_color[2] << " intensity " << backdrop_power << '\n';
	} else {
	    *report << "background_environment intensity " << backdrop_power << '\n';
	}
    }
    return true;
}


} // namespace

static asf::auto_release_ptr<asr::Project>
build_project(std::ostream *report)
{
    if (!art_valid_power(AmbientIntensity)) {
	bu_log("art: ambient intensity must be finite, nonnegative, and representable by the renderer\n");
	return asf::auto_release_ptr<asr::Project>();
    }
    const std::array<double, 3> legacy_color = {background[0], background[1], background[2]};

    /* If user gave no sizing info at all, use 512 as default */
    struct bu_vls dimensions = BU_VLS_INIT_ZERO;
    if (width <= 0)
	width = 512;
    if (height <= 0)
	height = 512;

    // Create an empty project.
    asf::auto_release_ptr<asr::Project> project(asr::ProjectFactory::create("test project"));
    // Keep asset lookup stable when invoked from another directory.
    char database_path[MAXPATHLEN];
    struct bu_vls directory = BU_VLS_INIT_ZERO;
    if (!bu_file_realpath(APP.a_rt_i->rti_dbip->dbi_filename, database_path) ||
	!bu_path_component(&directory, database_path, BU_PATH_DIRNAME)) {
	bu_log("art: cannot resolve the database asset directory\n");
	bu_vls_free(&directory);
	return asf::auto_release_ptr<asr::Project>();
    }
    project->search_paths().set_root_path(bu_vls_cstr(&directory));
    project->search_paths().push_back_explicit_path("shaders");
    bu_vls_free(&directory);

    // Add the shader libraries from the selected Appleseed installation.
    const std::string renderer_root = art_renderer_root();
    char root[MAXPATHLEN];
    const char *libraries[] = {"shaders/appleseed", "shaders/max", "shaders/maya"};
    for (const char *library : libraries) {
	const char *path = bu_dir(root, MAXPATHLEN, renderer_root.c_str(), library, NULL);
	if (path) project->search_paths().push_back_explicit_path(path);
    }

    if (report) {
	*report << "project_root " << std::quoted(project->search_paths().get_root_path().c_str()) << '\n'
	    << "shader_search_path " << std::quoted(project->search_paths().to_string_reversed(
		asf::SearchPaths::osl_path_separator()).c_str()) << '\n';

    }

    // Add default configurations to the project.
    project->add_default_configurations();

    if (!render_settings.valid_budget(samples)) {
	bu_log("art: adaptive min_samples must not exceed the samples budget\n");
	return asf::auto_release_ptr<asr::Project>();
    }
    project->configurations()
	.get_by_name("final")->get_parameters()
	.insert_path("uniform_pixel_renderer.samples", samples)
	.insert_path("rendering_threads", (size_t)npsw)
	.insert("project_rooted_textures", true);
    render_settings.apply(project->configurations().get_by_name("final")->get_parameters(), samples);
    project->configurations()
	.get_by_name("interactive")->get_parameters()
	.insert_path("rendering_threads", "1") /* no multithreading - for debug rendering on appleseed */
	.insert("project_rooted_textures", true);

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

    ArtSceneBuilder builder{scene.ref(), project->search_paths(), report};

    // walk the db to register all regions
    struct db_tree_state state;
    RT_DBTS_INIT(&state);
    state.ts_dbip = APP.a_rt_i->rti_dbip;

    /* discovered lights are collected by register_region() during the walk */
    art_lights.clear();

    const char **objects = nullptr;
    const int count = art_selected_objects(&objects);
    if (count && db_walk_tree(APP.a_rt_i->rti_dbip, count, objects, 1,
	&state, register_region, NULL, NULL, &builder) < 0)
	builder.valid = false;

    if (builder.region_paths.empty()) {
	bu_log("art: selection contains no regions\n");
	builder.valid = false;
    }
    if (!builder.valid)
	return asf::auto_release_ptr<asr::Project>();
    if (report) {
	if (!environment_controls.build(scene.ref(), project->search_paths(),
		legacy_color, AmbientIntensity, report))
	    return asf::auto_release_ptr<asr::Project>();
	project->set_scene(scene);
	return project;
    }

    //------------------------------------------------------------------------
    // Light
    //------------------------------------------------------------------------

    if (art_lights.empty()) {
	// No light-source regions found in the .g database: fall back to a
	// single default point light so the scene is still lit (this preserves
	// the previous behavior for scenes without explicit lights).
	static const float LightRadiance[] = { 1.0f, 1.0f, 1.0f };
	// Building a frame must not change the intensity used by later frames.
	const double fallback_intensity = static_cast<double>(light_intensity) * AmbientIntensity;
	if (!art_valid_power(fallback_intensity)) {
	    bu_log("art: fallback light power is outside the renderer's range\n");
	    return asf::auto_release_ptr<asr::Project>();
	}
	assembly->colors().insert(
	    asr::ColorEntityFactory::create(
		"light_intensity",
		asr::ParamArray()
		.insert("color_space", "srgb")
		.insert("multiplier", fallback_intensity),
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
	    const double multiplier = light_intensity * fract;
	    if (!art_valid_power(multiplier)) {
		bu_log("art: light '%s' power is outside the renderer's range\n", L.name.c_str());
		return asf::auto_release_ptr<asr::Project>();
	    }

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

    if (!environment_controls.build(scene.ref(), project->search_paths(),
	    legacy_color, AmbientIntensity, nullptr))
	return asf::auto_release_ptr<asr::Project>();

    //------------------------------------------------------------------------
    // Camera
    //------------------------------------------------------------------------

    // declare camera outside of if-else scope
    asf::auto_release_ptr<asr::Camera> camera;

    mat_t camera_to_world;
    if (!art_camera_transform(camera_to_world))
	return asf::auto_release_ptr<asr::Project>();

    const double frame_aspect = static_cast<double>(width) / height;
    const bool perspective = rt_perspective > 0.0;
    const double film_width = perspective ? 1.0 : viewsize;
    // Preserve small model dimensions when passing them to the renderer.
    bu_vls_sprintf(&dimensions, "%.17g %.17g", film_width, film_width / frame_aspect);
    if (perspective) {
	struct bu_vls fov = BU_VLS_INIT_ZERO;
	bu_vls_sprintf(&fov, "%.17g", rt_perspective);
	camera = asr::PinholeCameraFactory().create(
	    "camera",
	    asr::ParamArray()
	    .insert("film_dimensions", bu_vls_cstr(&dimensions))
	    .insert("horizontal_fov", bu_vls_cstr(&fov)));
	bu_vls_free(&fov);
    } else {
	camera = asr::OrthographicCameraFactory().create(
	    "camera",
	    asr::ParamArray()
	    .insert("film_dimensions", bu_vls_cstr(&dimensions)));
    }
    camera->transform_sequence().set_transform(
	0.0f,
	asf::Transformd::from_local_to_parent(
	    asf::Matrix<double, 4, 4>::from_array(camera_to_world)));

    // Bind the camera to the scene.
    scene->cameras().insert(camera);

    //------------------------------------------------------------------------
    // Frame
    //------------------------------------------------------------------------

    // Create a frame and bind it to the project.
    asr::AOVContainer aovs;
    if (!aov_output.populate(aovs))
	return asf::auto_release_ptr<asr::Project>();
    bu_vls_sprintf(&dimensions, "%zd %zd", width, height);
    project->set_frame(
	asr::FrameFactory::create(
	    "beauty",
	    asr::ParamArray()
	    .insert("camera", "camera")
	    .insert("tile_size", "32 32")
	    .insert("resolution", bu_vls_cstr(&dimensions)), aovs));

    // Bind the scene to the project.
    project->set_scene(scene);
    bu_vls_free(&dimensions);

    return project;
}


static struct fb *
fb_setup()
{
    // Keep small MGED-invoked renders large enough to see with integer zoom.
    constexpr size_t MIN_FRAMEBUFFER_SIZE = 512;
    const size_t fb_width = std::max(width, MIN_FRAMEBUFFER_SIZE);
    const size_t fb_height = std::max(height, MIN_FRAMEBUFFER_SIZE);
    const size_t max_dimension = static_cast<size_t>(std::numeric_limits<int>::max());
    if (!width || !height || fb_width > max_dimension || fb_height > max_dimension) {
	bu_log("art: image dimensions exceed framebuffer limits\n");
	return FB_NULL;
    }
    struct fb *fbp = fb_open(framebuffer, static_cast<int>(fb_width), static_cast<int>(fb_height));
    if (!fbp) {
	bu_log("art: cannot open framebuffer\n");
	return FB_NULL;
    }
    const int actual_width = fb_getwidth(fbp);
    const int actual_height = fb_getheight(fbp);
    if (actual_width <= 0 || actual_height <= 0 ||
	static_cast<size_t>(actual_width) < width || static_cast<size_t>(actual_height) < height) {
	bu_log("art: framebuffer is smaller than the requested image\n");
	fb_close(fbp);
	return FB_NULL;
    }
    const int zoom = static_cast<int>(std::min(static_cast<size_t>(actual_width) / width,
	static_cast<size_t>(actual_height) / height));
    if (fb_view(fbp, static_cast<int>(width / 2), static_cast<int>(height / 2), zoom, zoom) < 0) {
	bu_log("art: could not configure framebuffer view\n");
	fb_close(fbp);
	return FB_NULL;
    }
    return fbp;
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
    // Projects and their procedural objects are released after each render.
}


extern "C" void
do_run(int UNUSED(a), int UNUSED(b))
{
    bu_bomb("in run, Don't call me!");
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


static bool
art_prepare_geometry(struct rt_i* rtip)
{
    RT_CK_RTI(rtip);

    const char **objects = nullptr;
    const int count = art_selected_objects(&objects);
    if (count <= 0 || !objects) {
	bu_log("art: no objects selected for rendering\n");
	return false;
    }
    if (BU_LIST_IS_EMPTY(&rtip->HeadRegion)) {
	struct bu_vls times = BU_VLS_INIT_ZERO;
	rt_prep_timer();
	const int result = rt_gettrees(rtip, count, objects, static_cast<size_t>(npsw));
	(void)rt_get_timer(&times, NULL);
	if (rt_verbosity & VERBOSE_STATS)
	    bu_log("GETTREE: %s\n", bu_vls_addr(&times));
	bu_vls_free(&times);
	memory_summary();
	if (result < 0) {
	    bu_log("art: loading geometry for '%s' failed\n", objects[0]);
	    return false;
	}
    }
    if (rtip->needprep)
	rt_prep_parallel(rtip, 1);
    return true;
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
 * The intermediate name is exclusively reserved and released on every path.
 */
static bool
art_save_image(asr::Project& project, const char* filename)
{
    const char* dot = strrchr(filename, '.');

    if (dot && BU_STR_EQUIV(dot, ".exr"))
	return project.get_frame()->write_main_image(filename);

    // Diagnostic sidecars have already been saved from the linear frame.
    if (!ArtDisplayExposure(display_exposure).apply(project.get_frame()->image()))
	return false;
    if (dot && BU_STR_EQUIV(dot, ".png"))
	return project.get_frame()->write_main_image(filename);

    struct TemporaryRender {
	char path[MAXPATHLEN] = {0};
	// Keep the reservation open while the renderer and codec access its path.
	FILE *stream = bu_temp_file_suffix(path, sizeof(path), ".png");
	~TemporaryRender() {
	    if (!stream)
		return;
	    if (fclose(stream) != 0)
		bu_log("art: could not close temporary render '%s'\n", path);
	    if (bu_file_exists(path, NULL) && !bu_file_delete(path))
		bu_log("art: could not remove temporary render '%s'\n", path);
	}
    } temporary;
    if (!temporary.stream) {
	bu_log("art: could not create temporary render for '%s'\n", filename);
	return false;
    }
    if (!project.get_frame()->write_main_image(temporary.path))
	return false;

    std::unique_ptr<icv_image_t, decltype(&icv_destroy)> image(
	icv_read(temporary.path, BU_MIME_IMAGE_PNG, 0, 0), icv_destroy);
    if (!image) {
	bu_log("art: could not read temporary render '%s'\n", temporary.path);
	return false;
    }
    const bool success = icv_write(image.get(), filename, BU_MIME_IMAGE_AUTO) == 0;
    if (!success)
	bu_log("art: could not write '%s'\n", filename);
    return success;
}


namespace {
struct ReleaseLogTarget {
    void operator()(asf::ILogTarget *target) const {
	asr::global_logger().remove_target(target);
	target->release();
    }
};

class RenderErrors final : public asf::ILogTarget {
    std::atomic<bool> m_has_errors{false};
public:
    void release() override { delete this; }
    void write(const asf::LogMessage::Category category, const char *UNUSED(file),
	const size_t UNUSED(line), const char *UNUSED(header), const char *UNUSED(message)) override {
	if (category >= asf::LogMessage::Error)
	    m_has_errors.store(true, std::memory_order_relaxed);
    }
    bool has_errors() const { return m_has_errors.load(std::memory_order_relaxed); }
};
}

static bool
art_render_image(asr::Project& project, const char *filename, ArtTileCallback& callback)
{
    ArtRenderControl controller(callback, curframe, render_progress != 0);
    if (!controller.valid())
	return false;
    asf::SearchPaths resource_search_paths;
    const std::string renderer_root = art_renderer_root();
    char headers[MAXPATHLEN];
    const char *path = bu_dir(headers, MAXPATHLEN, renderer_root.c_str(), "share/OSL/shaders", NULL);
    if (path)
	resource_search_paths.push_back_explicit_path(path);
    path = bu_dir(headers, MAXPATHLEN, renderer_root.c_str(), "shaders", NULL);
    if (path)
	resource_search_paths.push_back_explicit_path(path);
    std::unique_ptr<RenderErrors, ReleaseLogTarget> errors(new RenderErrors());
    asr::global_logger().add_target(errors.get());
    bool succeeded = false;
    {
	asr::MasterRenderer renderer(
	    project,
	    project.configurations().get_by_name("final")->get_inherited_parameters(),
	    resource_search_paths,
	    &callback);
	succeeded = renderer.render(controller).m_status == asr::MasterRenderer::RenderingResult::Succeeded;
    }
    const bool restored = controller.restore();
    if (controller.cancelled()) {
	bu_log("art: rendering cancelled; image was not saved\n");
	return false;
    }
    if (!restored)
	return false;
    if (!succeeded) {
	bu_log("art: rendering did not complete successfully\n");
	return false;
    }
    // Worker and OSL errors can be logged without failing MasterRenderer.
    // Include renderer cleanup in the observed scope before saving the image.
    if (errors->has_errors()) {
	bu_log("art: renderer reported errors; image was not saved\n");
	return false;
    }
    if (!callback.flush())
	return false;
    const bool saved = aov_output.write(*project.get_frame(), filename) && art_save_image(project, filename);
    if (saved)
	controller.complete();
    // Some framebuffer backends linger until their window is closed.
    const bool closed = callback.finish();
    return saved && closed;
}


static bool
art_process_scene(bool display_framebuffer, const char *filename)
{
    asr::global_logger().set_verbosity_level(render_settings.log_level);
    if (!inspect_materials) {
	if (!std::isfinite(azimuth) || !std::isfinite(elevation)) {
	    bu_log("art: camera azimuth and elevation must be finite\n");
	    return false;
	}
	if (APP.a_rt_i->stats.nsolids <= 0 || APP.a_rt_i->stats.nregions <= 0) {
	    bu_log("art: no active geometry to render\n");
	    return false;
	}
	do_view_finalize(azimuth, elevation);
    }
    std::ostringstream report;
    report.imbue(std::locale::classic());
    auto project = build_project(inspect_materials ? &report : nullptr);
    if (!project.get())
	return false;
    if (inspect_materials) {
	const std::string text = report.str();
	if (fwrite(text.data(), 1, text.size(), stdout) != text.size() || fflush(stdout) != 0) {
	    bu_log("art: cannot write material inspection report\n");
	    return false;
	}
	return true;
    }
    struct fb *fbp = display_framebuffer ? fb_setup() : FB_NULL;
    if (display_framebuffer && !fbp)
	return false;
    ArtTileCallback callback(fbp, display_exposure);
    return art_render_image(project.ref(), filename, callback);
}


struct ArtCommandFrames {
    bool pending = false;
    bool finished = false;
};
static ArtCommandFrames command_frames;


static bool
art_frame_range_valid()
{
    if (desiredframe < 0 || finalframe < -1 || (finalframe >= 0 && desiredframe > finalframe)) {
	bu_log("art: frame range requires 0 <= start <= end, or end=-1 for no limit\n");
	return false;
    }
    return true;
}


static std::string
art_frame_filename(const char *filename, int frame)
{
    std::string path(filename);
    if (frame == 0)
	return path;
    const size_t separator = path.find_last_of("/\\");
    const size_t basename = separator == std::string::npos ? 0 : separator + 1;
    const size_t dot = path.find_last_of('.');
    const size_t suffix = dot != std::string::npos && dot > basename ? dot : path.size();
    path.insert(suffix, "." + std::to_string(frame));
    return path;
}


static bool
art_frame_command_ready()
{
    if (APP.a_rt_i)
	return true;
    bu_log("art: frame commands require -M input after database loading\n");
    return false;
}


static int
art_cm_start(const int UNUSED(argc), const char **argv)
{
    if (!art_frame_command_ready())
	return -1;

    if (command_frames.pending) {
	bu_log("art: frame %d has no end command before start\n", curframe);
	return -1;
    }
    int frame = 0;
    const char *end = argv[1] + strlen(argv[1]);
    const auto parsed = std::from_chars(argv[1], end, frame);
    if (parsed.ec != std::errc() || parsed.ptr != end || frame < 0) {
	bu_log("art: start requires a nonnegative decimal frame number: %s\n", argv[1]);
	return -1;
    }
    if (!art_frame_range_valid())
	return -1;
    if (finalframe >= 0 && frame > finalframe) {
	command_frames.finished = true;
	return 0;
    }
    curframe = frame;
    command_frames.pending = true;
    return 0;
}


static int
art_cm_end(const int UNUSED(argc), const char** UNUSED(argv))
{
    if (!art_frame_command_ready())
	return -1;

    command_frames.pending = false;
    if (!art_frame_range_valid())
	return -1;
    // Retain setup commands from earlier frames; only rendering is skipped.
    if (curframe < desiredframe)
	return 0;
    if (finalframe >= 0 && curframe > finalframe) {
	command_frames.finished = true;
	return 0;
    }
    if (!inspect_materials && !art_prepare_geometry(APP.a_rt_i))
	return -1;
    const std::string filename = art_frame_filename(outputfile, curframe);
    return art_process_scene(framebuffer != nullptr, filename.c_str()) ? 0 : -1;
}


static bool
art_space(char value)
{
    return std::isspace(static_cast<unsigned char>(value)) != 0;
}

static void
art_setting_fields(const struct bu_structparse *table, std::vector<const struct bu_structparse *>& fields)
{
    for (const auto *field = table; field->sp_fmt[0]; ++field) {
	if (field->sp_fmt[1] == 'p')
	    art_setting_fields(reinterpret_cast<const struct bu_structparse *>(field->sp_offset), fields);
	else
	    fields.push_back(field);
    }
}

static int
art_setting_error(const std::string& name, const std::string& value, const char *reason)
{
    bu_log("art: invalid setting '%s=%s': %s\n", name.c_str(), value.c_str(), reason);
    return -1;
}

static bool
art_setting_integer(const std::string& value, int& number)
{
    const char *begin = value.data();
    const char *end = begin + value.size();
    while (begin != end && art_space(*begin))
	++begin;
    while (begin != end && art_space(end[-1]))
	--end;
    if (begin != end && *begin == '+') {
	++begin;
	if (begin == end || *begin == '-')
	    return false;
    }
    const auto parsed = std::from_chars(begin, end, number);
    return parsed.ec == std::errc() && parsed.ptr == end;
}

static bool
art_setting_real(const std::string& value, double& number)
{
    std::istringstream input(value);
    input.imbue(std::locale::classic());
    if (!(input >> number) || !std::isfinite(number))
	return false;
    input >> std::ws;
    return input.eof();
}

static bool
art_setting_color(const std::string& value, std::array<double, 3>& rgb)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    if (!bu_color_from_str(&color, value.c_str()))
	return false;
    rgb = {color.buc_rgb[0], color.buc_rgb[1], color.buc_rgb[2]};
    return true;
}

static void
art_print_color_setting(const char *name, const double *rgb)
{
    if (!rgb) {
	bu_log("%s=inherit\n", name);
	return;
    }
    const int digits = std::numeric_limits<double>::max_digits10;
    bu_log("%s=%#.*g/%#.*g/%#.*g\n", name,
	digits, rgb[0], digits, rgb[1], digits, rgb[2]);
}

static int
art_cm_set(const int argc, const char **argv)
{
    std::vector<const struct bu_structparse *> fields;
    art_setting_fields(set_parse, fields);
    if (argc == 1) {
	bu_log("Art Parameter Values\nwidth=%zu height=%zu\n", width, height);
	for (const auto *field : fields) {
	    if (BU_STR_EQUAL(field->sp_name, "width") || BU_STR_EQUAL(field->sp_name, "height"))
		continue;
	    if (BU_STR_EQUAL(field->sp_name, "background") || BU_STR_EQUAL(field->sp_name, "bg")) {
		art_print_color_setting(field->sp_name, background);
		continue;
	    }
	    // Print from the actual field address instead of a null structure base.
	    struct bu_structparse printable = *field;
	    printable.sp_offset = 0;
	    struct bu_vls value = BU_VLS_INIT_ZERO;
	    bu_vls_struct_item(&value, &printable, reinterpret_cast<const char *>(field->sp_offset), '/');
	    bu_log("%s=%s\n", field->sp_name, bu_vls_cstr(&value));
	    bu_vls_free(&value);
	}
	bu_log("environment_map=%s\nenvironment_map_colorspace=%s\nenvironment_rotation=%.17g\nbackground_source=%s\n",
	    environment_controls.map.empty() ? "inherit" : environment_controls.map.c_str(),
	    environment_controls.map_colorspace.c_str(), environment_controls.rotation,
	    art_background_source_name(environment_controls.background_source));
	art_print_color_setting("environment_color", environment_controls.environment_color ?
	    environment_controls.environment_color->data() : nullptr);
	art_print_color_setting("background_color", environment_controls.background_color ?
	    environment_controls.background_color->data() : nullptr);
	const std::pair<const char *, std::optional<double>> powers[] = {
	    {"environment_intensity", environment_controls.environment_intensity},
	    {"background_intensity", environment_controls.background_intensity}};
	for (const auto& power : powers) {
	    if (power.second)
		bu_log("%s=%.*g\n", power.first, std::numeric_limits<double>::max_digits10, *power.second);
	    else
		bu_log("%s=inherit\n", power.first);
	}
	std::ostringstream controls;
	controls.imbue(std::locale::classic());
	render_settings.write(controls);
	controls << "aovs=" << aov_output.value() << '\n'
	    << "display_exposure=" << display_exposure << '\n';
	bu_log("%s", controls.str().c_str());
	return 0;
    }

    std::string input;
    for (int index = 1; index < argc; ++index) {
	if (!input.empty())
	    input += ' ';
	const std::string argument = argv[index];
	const size_t equal = argument.find('=');
	// rt_do_cmd removes quotes but retains grouped whitespace in each argument.
	// A map filename remains one string even when it contains another equals sign.
	if (argument.find_first_of(" \t\r\n") != std::string::npos &&
	    argument.find('"') == std::string::npos &&
	    (equal == std::string::npos || argument.find('=', equal + 1) == std::string::npos ||
	        argument.substr(0, equal) == "environment_map")) {
	    if (equal != std::string::npos)
		input += argument.substr(0, equal + 1);
	    input += '"';
	    input += equal == std::string::npos ? argument : argument.substr(equal + 1);
	    input += '"';
	} else {
	    input += argument;
	}
    }

    if (input.empty())
	return art_setting_error("", "", "expected name=value");
    std::ostringstream canonical;
    canonical.imbue(std::locale::classic());
    canonical << std::showpoint << std::setprecision(std::numeric_limits<double>::max_digits10);
    std::optional<size_t> new_width, new_height;
    ArtEnvironmentControls new_environment = environment_controls;
    ArtRenderSettings new_render_settings = render_settings;
    ArtAOVOutput new_aov_output = aov_output;
    double new_display_exposure = display_exposure;
    int new_sample_budget = samples;
    std::set<std::string> compatibility_settings;
    size_t cursor = 0;
    while (cursor < input.size()) {
	while (cursor < input.size() && art_space(input[cursor]))
	    ++cursor;
	if (cursor == input.size())
	    break;
	const size_t name_begin = cursor;
	while (cursor < input.size() && input[cursor] != '=' && !art_space(input[cursor]))
	    ++cursor;
	const std::string name = input.substr(name_begin, cursor - name_begin);
	while (cursor < input.size() && art_space(input[cursor]))
	    ++cursor;
	if (name.empty() || cursor == input.size() || input[cursor++] != '=')
	    return art_setting_error(name, "", "expected name=value");
	while (cursor < input.size() && art_space(input[cursor]))
	    ++cursor;
	const bool quoted = cursor < input.size() && input[cursor] == '"';
	if (quoted)
	    ++cursor;
	const size_t value_begin = cursor;
	while (cursor < input.size() && (quoted ? input[cursor] != '"' : !art_space(input[cursor])))
	    ++cursor;
	const std::string value = input.substr(value_begin, cursor - value_begin);
	if (quoted && cursor == input.size())
	    return art_setting_error(name, value, "missing closing quote");
	if (quoted && ++cursor < input.size() && !art_space(input[cursor]))
	    return art_setting_error(name, value, "expected whitespace after closing quote");
	if (value.empty())
	    return art_setting_error(name, value, "a value is required");
	if (name == "display_exposure") {
	    if (!art_setting_real(value, new_display_exposure))
		return art_setting_error(name, value, "expected a finite exposure in stops");
	    continue;
	}
	if (name == "aovs") {
	    std::string reason;
	    if (!new_aov_output.assign(value, reason))
		return art_setting_error(name, value, reason.c_str());
	    continue;
	}
	if (name == "sampling") {
	    if (value == "uniform")
		new_render_settings.sampling = ArtSamplingMode::Uniform;
	    else if (value == "adaptive")
		new_render_settings.sampling = ArtSamplingMode::Adaptive;
	    else
		return art_setting_error(name, value, "expected uniform or adaptive");
	    continue;
	}
	if (name == "renderer_log") {
	    const auto level = std::find_if(std::begin(render_log_levels), std::end(render_log_levels),
		[&value](const auto& entry) { return value == entry.first; });
	    if (level == std::end(render_log_levels))
		return art_setting_error(name, value, "expected debug, info, warning, or error");
	    new_render_settings.log_level = level->second;
	    continue;
	}
	if (name == "min_samples" || name == "batch_size") {
	    int number;
	    if (!art_setting_integer(value, number) || number < (name == "batch_size" ? 1 : 0))
		return art_setting_error(name, value, "expected a complete integer in the supported range");
	    (name == "min_samples" ? new_render_settings.min_samples : new_render_settings.batch_size) = number;
	    continue;
	}
	if (name == "noise_threshold") {
	    double number;
	    if (!art_setting_real(value, number) || number < MIN_RENDER_NOISE_THRESHOLD ||
		number > MAX_RENDER_NOISE_THRESHOLD)
		return art_setting_error(name, value, "expected a finite threshold from 0.0001 through 10000");
	    new_render_settings.noise_threshold = number;
	    continue;
	}
	if (const auto *setting = art_render_integer_setting(name)) {
	    auto& number = new_render_settings.*(setting->value);
	    if (value == "inherit") {
		number.reset();
	    } else {
		int parsed;
		if (!art_setting_integer(value, parsed) || parsed < setting->minimum || parsed > setting->maximum)
		    return art_setting_error(name, value, "expected inherit or an integer in the supported range");
		number = parsed;
	    }
	    continue;
	}
	if (name == "environment_map") {
	    new_environment.map = value == "inherit" ? "" : value;
	    continue;
	}
	if (name == "environment_map_colorspace") {
	    if (value != "linear" && value != "srgb")
		return art_setting_error(name, value, "expected linear or srgb");
	    new_environment.map_colorspace = value;
	    continue;
	}
	if (name == "environment_rotation") {
	    double number;
	    if (!art_setting_real(value, number))
		return art_setting_error(name, value, "expected finite rotation in degrees");
	    const double full_circle = 360.0;
	    new_environment.rotation = std::fmod(number, full_circle);
	    if (new_environment.rotation < 0.0)
		new_environment.rotation += full_circle;
	    continue;
	}
	if (name == "background_source") {
	    if (value == "inherit")
		new_environment.background_source = ArtBackgroundSource::Inherit;
	    else if (value == "color")
		new_environment.background_source = ArtBackgroundSource::Color;
	    else if (value == "environment")
		new_environment.background_source = ArtBackgroundSource::Environment;
	    else
		return art_setting_error(name, value, "expected inherit, color, or environment");
	    continue;
	}
	if (name == "environment_color" || name == "background_color") {
	    auto& color = name == "environment_color" ? new_environment.environment_color :
		new_environment.background_color;
	    if (value == "inherit") {
		color.reset();
	    } else {
		std::array<double, 3> rgb;
		if (!art_setting_color(value, rgb))
		    return art_setting_error(name, value, "expected a valid color or inherit");
		color = rgb;
	    }
	    continue;
	}
	if (name == "environment_intensity" || name == "background_intensity") {
	    auto& power = name == "environment_intensity" ? new_environment.environment_intensity :
		new_environment.background_intensity;
	    if (value == "inherit") {
		power.reset();
	    } else {
		double number;
		if (!art_setting_real(value, number) || !art_valid_power(number))
		    return art_setting_error(name, value, "expected nonnegative power representable by the renderer, or inherit");
		power = number;
	    }
	    continue;
	}
	const auto found = std::find_if(fields.begin(), fields.end(), [&name](const auto *entry) {
	    return name == entry->sp_name;
	});
	if (found == fields.end())
	    return art_setting_error(name, value, "unknown parameter");
	const auto *field = *found;
	if (name == "save_overlaps" || name == "rt_cline_radius")
	    compatibility_settings.insert(name);

	if (name == "background" || name == "bg") {
	    std::array<double, 3> color;
	    if (!art_setting_color(value, color))
		return art_setting_error(name, value, "expected a valid color");
	    canonical << name << "=\"" << color[0] << '/' << color[1]
		<< '/' << color[2] << "\" ";
	} else if (field->sp_fmt[1] == 'd' && field->sp_count == 1) {
	    const bool boolean = name == "inspect_materials" || name == "save_overlaps" || name == "progress";
	    if (boolean && value == "!") {
		canonical << name << "=! ";
		continue;
	    }
	    int number;
	    if (!art_setting_integer(value, number))
		return art_setting_error(name, value, "expected a complete decimal integer in int range");
	    const bool positive = name == "samples" || name == "s" || name == "width" || name == "height";
	    if ((positive && number < 1) || (!positive && number < 0) || (boolean && number > 1))
		return art_setting_error(name, value, boolean ? "expected 0, 1, or !" :
		    positive ? "expected a positive integer" : "expected a nonnegative integer");
	    if (name == "samples" || name == "s")
		new_sample_budget = number;
	    if (name == "width")
		new_width = static_cast<size_t>(number);
	    else if (name == "height")
		new_height = static_cast<size_t>(number);
	    else
		canonical << name << '=' << number << ' ';
	} else if ((field->sp_fmt[1] == 'f' || field->sp_fmt[1] == 'g') && field->sp_count == 1) {
	    double number;
	    if (!art_setting_real(value, number))
		return art_setting_error(name, value, "expected a complete finite number");
	    if (name == "light_intensity" || name == "li") {
		if (!art_valid_power(number))
		    return art_setting_error(name, value, "expected nonnegative power representable by the renderer");
	    } else if ((name == "perspective" || name == "angle") && (number < 0.0 || number >= 180.0)) {
		return art_setting_error(name, value, "expected 0 <= perspective < 180 degrees");
	    }
	    canonical << name << '=' << number << ' ';
	} else {
	    return art_setting_error(name, value, "parameter type is not supported by art");
	}
    }

    if (!new_render_settings.valid_budget(new_sample_budget))
	return art_setting_error("min_samples", std::to_string(new_render_settings.min_samples),
	    "adaptive minimum must not exceed samples");
    // Validate the whole assignment list before changing shared renderer state.
    const std::string settings = canonical.str();
    if (!settings.empty()) {
	const char *arguments[] = {"set", settings.c_str()};
	const int result = cm_set(2, arguments);
	if (result != 0)
	    return result;
    }
    if (new_width)
	width = *new_width;
    if (new_height)
	height = *new_height;
    environment_controls = new_environment;
    render_settings = new_render_settings;
    aov_output = std::move(new_aov_output);
    display_exposure = new_display_exposure;
    for (const auto& name : compatibility_settings)
	bu_log("art: warning: unsupported rendering setting '%s'; value stored for command compatibility\n", name.c_str());
    return 0;
}


static int
art_cm_unsupported(const int UNUSED(argc), const char **argv)
{
    bu_log("art: command '%s' is not supported\n", argv[0]);
    return -1;
}


static bool
art_bind_commands()
{
    bool have_start = false;
    bool have_end = false;
    bool have_set = false;
    for (auto *command = rt_do_tab; command->ct_cmd; ++command) {
	if (BU_STR_EQUAL(command->ct_cmd, "start")) {
	    command->ct_func = art_cm_start;
	    have_start = true;
	} else if (BU_STR_EQUAL(command->ct_cmd, "end")) {
	    command->ct_func = art_cm_end;
	    have_end = true;
	} else if (BU_STR_EQUAL(command->ct_cmd, "set")) {
	    command->ct_func = art_cm_set;
	    have_set = true;
	} else if (BU_STR_EQUAL(command->ct_cmd, "multiview") ||
	    BU_STR_EQUAL(command->ct_cmd, "_closedb")) {
	    command->ct_func = art_cm_unsupported;
	}
    }
    if (!have_start || !have_end || !have_set) {
	bu_log("art: ray tracing command table is missing required commands\n");
	return false;
    }
    return true;
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

    std::unique_ptr<asf::ILogTarget, ReleaseLogTarget> log_target(
	asf::create_console_log_target(stderr));
    asr::global_logger().add_target(log_target.get());

    // Print appleseed's version string.
    RENDERER_LOG_INFO("%s\n", asf::Appleseed::get_synthetic_version_string());

    struct rt_i* rtip;
    const char *title_file = NULL;

    bu_setprogname(argv[0]);

    // Shared command handlers allocate the active selection lazily.
    struct CommandObjectsGuard {
	~CommandObjectsGuard() {
	    if (!cmd_objs)
		return;
	    for (size_t index = 0; index < BU_PTBL_LEN(cmd_objs); ++index)
		bu_free((char *)BU_PTBL_GET(cmd_objs, index), "command object");
	    bu_ptbl_free(cmd_objs);
	    BU_PUT(cmd_objs, struct bu_ptbl);
	    cmd_objs = nullptr;
	}
    } command_objects_guard;

    // initialize options and overload menu before parsing
    init_defaults();
    if (!art_bind_commands())
	return BRLCAD_ERROR;

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
    if (matflag && !art_frame_range_valid())
	return BRLCAD_ERROR;

    if (bu_optind >= argc) {
	RENDERER_LOG_INFO("%s: BRL-CAD geometry database not specified\n", argv[0]);
	usage(argv[0], 0);
	return 1;
    }

    title_file = argv[bu_optind];
    global_title_file = title_file;
    if (!objv) {
	objc = argc - bu_optind - 1;
	if (objc) {
	    objv = (char **)&(argv[bu_optind+1]);
	} else if (!matflag) {
	    bu_log("No Region specified\n");
	    return BRLCAD_ERROR;
	}
    }


    char title[1024] = { 0 };

    /* load the specified geometry database */
    rtip = rt_dirbuild_ex(title_file, title, sizeof(title), DB_OPEN_COMB_INSTANCES);
    if (rtip == RTI_NULL) {
	RENDERER_LOG_INFO("building the database directory for [%s] FAILED\n", title_file);
	return -1;
    }

    std::unique_ptr<struct rt_i, decltype(&rt_i_destroy)> database(rtip, rt_i_destroy);

    /* print optional title */
    if (title[0]) {
	RENDERER_LOG_INFO("database title: %s\n", title);
    }

    /* initialize values in application struct */
    RT_APPLICATION_INIT(&APP);

    /* configure raytrace application */
    APP.a_rt_i = rtip;
    APP.a_onehit = 1;

    if (objv && !inspect_materials && !art_prepare_geometry(rtip))
	return BRLCAD_ERROR;

    if (matflag) {
	char* buf;
	int nret = 0;
	while ((buf = rt_read_cmd(stdin)) != (char*)0) {
	    RENDERER_LOG_INFO("cmd: %s \n", buf);
	    nret = rt_do_cmd(APP.a_rt_i, buf, rt_do_tab);
	    bu_free(buf, "rt_read_cmd command buffer");
	    if (nret != 0 || command_frames.finished)
		break;
	}
	if (nret != 0)
	    return BRLCAD_ERROR;
	if (ferror(stdin)) {
	    bu_log("art: cannot read frame command stream\n");
	    return BRLCAD_ERROR;
	}
	if (command_frames.pending) {
	    bu_log("art: frame %d has no end command\n", curframe);
	    return BRLCAD_ERROR;
	}
	return BRLCAD_OK;
    }


    return art_process_scene(framebuffer != nullptr, outputfile) ? BRLCAD_OK : BRLCAD_ERROR;
}


// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
