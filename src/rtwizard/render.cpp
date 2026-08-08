/*                       R E N D E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "bio.h"
#include "vmath.h"
#include "bn/qmath.h"
#include "bu/app.h"
#include "bu/color.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/process.h"
#include "bu/str.h"
#include "bv.h"
#include "bv/vlist.h"
#include "dm.h"
#include "ged.h"
#include "icv.h"
#include "pkg.h"
#include "raytrace.h"

#include "animation.h"
#include "render.h"
#include "settings.h"

namespace {

namespace fs = std::filesystem;

class ScopedFramebufferToken {
public:
    ScopedFramebufferToken()
    {
        const char *current = std::getenv("FBSERV_TOKEN");
        if (current) {
            had_value = true;
            previous = current;
            if (previous.size() == 64) return;
        }
        std::random_device random;
        std::uniform_int_distribution<unsigned int> byte(0, 255);
        std::ostringstream token;
        token << std::hex << std::setfill('0');
        for (size_t index = 0; index < 32; ++index) token << std::setw(2) << byte(random);
        if (bu_setenv("FBSERV_TOKEN", token.str().c_str(), 1) != 0)
            throw std::runtime_error("unable to establish framebuffer session authentication");
        changed = true;
    }

    ~ScopedFramebufferToken()
    {
        if (!changed) return;
        if (had_value) {
            (void)bu_setenv("FBSERV_TOKEN", previous.c_str(), 1);
            return;
        }
#ifdef HAVE_WINDOWS_H
        (void)SetEnvironmentVariableA("FBSERV_TOKEN", nullptr);
#else
        (void)unsetenv("FBSERV_TOKEN");
#endif
    }

private:
    bool changed = false;
    bool had_value = false;
    std::string previous;
};

static bool
publish_file(const fs::path &temporary, const fs::path &destination)
{
#ifdef HAVE_WINDOWS_H
    return MoveFileEx(temporary.string().c_str(), destination.string().c_str(),
	MOVEFILE_WRITE_THROUGH | MOVEFILE_REPLACE_EXISTING) != 0;
#else
    std::error_code error;
    fs::rename(temporary, destination, error);
    return !error;
#endif
}

struct IcvDeleter {
    void operator()(icv_image_t *img) const { if (img) icv_destroy(img); }
};
using Image = std::unique_ptr<icv_image_t, IcvDeleter>;

struct DbDeleter {
    void operator()(db_i *dbip) const { if (dbip) db_close(dbip); }
};
using Database = std::unique_ptr<db_i, DbDeleter>;

struct GedDeleter {
    void operator()(ged *gedp) const { if (gedp) ged_close(gedp); }
};
using GedSession = std::unique_ptr<ged, GedDeleter>;

static void
set_error(char **errmsg, const std::string &message)
{
    if (!errmsg) return;
    *errmsg = bu_strdup(message.c_str());
}

class JobContext {
public:
    JobContext(const rtwizard_render_callbacks *cb, void *data, const char *log_path,
        const char *pid_path, bool console_log) : callbacks(cb), callback_data(data),
        console(console_log), pid_file(pid_path ? pid_path : "")
    {
        if (log_path && log_path[0]) {
            log_file.open(log_path, std::ios::trunc);
            if (!log_file) throw std::runtime_error("unable to open render log '" + std::string(log_path) + "'");
        }
        char unique[MAXPATHLEN] = {0};
        const char *name = bu_temp_file_name(unique, sizeof(unique));
        if (!name || !name[0]) throw std::runtime_error("unable to allocate a temporary render name");
        directory = fs::path(name + std::string("-rtwizard"));
        std::error_code ec;
        if (!fs::create_directory(directory, ec) || ec)
            throw std::runtime_error("unable to create temporary render directory '" + directory.string() + "'");
    }

    ~JobContext()
    {
        clear_pid();
        std::error_code ec;
        fs::remove_all(directory, ec);
    }

    bool cancelled() const
    {
        return callbacks && callbacks->cancelled && callbacks->cancelled(callback_data);
    }

    void log(const std::string &message) const
    {
        if (log_file) {
            log_file << message;
            if (message.empty() || message.back() != '\n') log_file << '\n';
            log_file.flush();
        }
        if (callbacks && callbacks->log) callbacks->log(callback_data, message.c_str());
        else if (console && !message.empty()) bu_log("%s%s", message.c_str(), message.back() == '\n' ? "" : "\n");
    }

    void progress(int completed, int total) const
    {
        if (callbacks && callbacks->progress) callbacks->progress(callback_data, completed, total);
    }

    fs::path file(const std::string &name) const { return directory / name; }

    void set_pid(int pid) const
    {
        if (pid_file.empty()) return;
        std::ofstream output(pid_file, std::ios::trunc);
        if (!output) throw std::runtime_error("unable to write pid file '" + pid_file + "'");
        output << pid << '\n';
        if (!output) throw std::runtime_error("unable to write pid file '" + pid_file + "'");
    }

    void clear_pid() const
    {
        if (!pid_file.empty() && bu_file_exists(pid_file.c_str(), nullptr))
            (void)bu_file_delete(pid_file.c_str());
    }

    const rtwizard_render_callbacks *callbacks = nullptr;
    void *callback_data = nullptr;
    bool console = false;
    mutable std::ofstream log_file;
    std::string pid_file;
    fs::path directory;
};

struct ViewState {
    double size = 1.0;
    quat_t orientation = {0.0, 0.0, 0.0, 1.0};
    point_t eye = VINIT_ZERO;
    double perspective = 0.0;
    point_t bounds_min = VINIT_ZERO;
    point_t bounds_max = VINIT_ZERO;
};

struct BoundsState {
    point_t minimum = VINIT_ZERO;
    point_t maximum = VINIT_ZERO;
    bool have_bounds = false;
    bool error = false;
};

static bool
finite_bounds(const point_t minimum, const point_t maximum)
{
    for (int axis = 0; axis < 3; ++axis) {
        if (!std::isfinite(minimum[axis]) || !std::isfinite(maximum[axis]) ||
            minimum[axis] > maximum[axis])
            return false;
    }
    return true;
}

static union tree *
bounds_leaf(db_tree_state *state, const db_full_path *, rt_db_internal *internal,
    void *client_data)
{
    BoundsState *bounds = static_cast<BoundsState *>(client_data);
    point_t minimum, maximum;
    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    int status = -1;
    if (internal->idb_meth->ft_bbox)
        status = internal->idb_meth->ft_bbox(internal, &minimum, &maximum, state->ts_tol);
    if (status < 0 && internal->idb_meth->ft_plot) {
        bu_list vhead;
        BU_LIST_INIT(&vhead);
        if (internal->idb_meth->ft_plot(&vhead, internal, state->ts_ttol, state->ts_tol, nullptr) >= 0 &&
            bv_vlist_bbox(&vhead, &minimum, &maximum, nullptr, nullptr) == 0)
            status = 0;
        BV_FREE_VLIST(&rt_vlfree, &vhead);
    }
    if (status < 0) {
        bounds->error = true;
        return TREE_NULL;
    }

    soltab *solid;
    BU_GET(solid, soltab);
    *solid = RT_SOLTAB_INIT_ZERO;
    solid->l.magic = RT_SOLTAB_MAGIC;
    solid->l2.magic = RT_SOLTAB2_MAGIC;
    solid->st_aradius = 1.0;
    VMOVE(solid->st_min, minimum);
    VMOVE(solid->st_max, maximum);

    union tree *leaf;
    BU_GET(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_op = OP_SOLID;
    leaf->tr_a.tu_stp = solid;
    return leaf;
}

static int
bounds_tree(const union tree *tree, point_t minimum, point_t maximum)
{
    point_t right_minimum, right_maximum;
    VSETALL(minimum, INFINITY);
    VSETALL(maximum, -INFINITY);
    VSETALL(right_minimum, INFINITY);
    VSETALL(right_maximum, -INFINITY);
    if (!tree) return -1;
    switch (tree->tr_op) {
        case OP_SOLID:
            VMOVE(minimum, tree->tr_a.tu_stp->st_min);
            VMOVE(maximum, tree->tr_a.tu_stp->st_max);
            return 0;
        case OP_UNION:
        case OP_XOR:
            if (bounds_tree(tree->tr_b.tb_left, minimum, maximum) < 0 ||
                bounds_tree(tree->tr_b.tb_right, right_minimum, right_maximum) < 0)
                return -1;
            VMIN(minimum, right_minimum);
            VMAX(maximum, right_maximum);
            return 0;
        case OP_INTERSECT:
            if (bounds_tree(tree->tr_b.tb_left, minimum, maximum) < 0 ||
                bounds_tree(tree->tr_b.tb_right, right_minimum, right_maximum) < 0)
                return -1;
            VMAX(minimum, right_minimum);
            VMIN(maximum, right_maximum);
            return 0;
        case OP_SUBTRACT:
        case OP_GUARD:
        case OP_XNOP:
            return bounds_tree(tree->tr_b.tb_left, minimum, maximum);
        case OP_NOT:
            VSETALL(minimum, -INFINITY);
            VSETALL(maximum, INFINITY);
            return 0;
        default:
            return -1;
    }
}

static void
free_bounds_tree(union tree *tree, bool keep_root)
{
    if (!tree) return;
    switch (tree->tr_op) {
        case OP_SOLID:
            if (tree->tr_a.tu_stp) BU_PUT(tree->tr_a.tu_stp, soltab);
            break;
        case OP_UNION:
        case OP_XOR:
        case OP_INTERSECT:
        case OP_SUBTRACT:
            free_bounds_tree(tree->tr_b.tb_left, false);
            free_bounds_tree(tree->tr_b.tb_right, false);
            break;
        case OP_GUARD:
        case OP_XNOP:
        case OP_NOT:
            free_bounds_tree(tree->tr_b.tb_left, false);
            break;
        default:
            break;
    }
    if (keep_root) {
        RT_TREE_INIT(tree);
        tree->tr_op = OP_NOP;
    } else {
        BU_PUT(tree, union tree);
    }
}

static union tree *
bounds_region_end(db_tree_state *, const db_full_path *, union tree *tree,
    void *client_data)
{
    BoundsState *bounds = static_cast<BoundsState *>(client_data);
    point_t minimum, maximum;
    if (bounds_tree(tree, minimum, maximum) == 0 && finite_bounds(minimum, maximum)) {
        if (!bounds->have_bounds) {
            VMOVE(bounds->minimum, minimum);
            VMOVE(bounds->maximum, maximum);
            bounds->have_bounds = true;
        } else {
            VMIN(bounds->minimum, minimum);
            VMAX(bounds->maximum, maximum);
        }
    }
    free_bounds_tree(tree, true);
    return tree;
}

/* Compute display-compatible bounds.  In particular, unbounded halfspaces do
 * not poison an otherwise finite scene: MGED's historical draw/autoview path
 * likewise frames the finite plotted objects. */
static bool
display_bounds(db_i *dbip, std::vector<const char *> &objects,
    point_t minimum, point_t maximum, std::string &error,
    const fastf_t *initial_matrix = nullptr)
{
    db_tree_state state = RT_DBTS_INIT_IDN;
    bg_tess_tol tessellation = BG_TESS_TOL_INIT_ZERO;
    bn_tol tolerance = BN_TOL_INIT_TOL;
    BoundsState bounds;
    VSETALL(bounds.minimum, INFINITY);
    VSETALL(bounds.maximum, -INFINITY);
    state.ts_dbip = dbip;
    state.ts_ttol = &tessellation;
    state.ts_tol = &tolerance;
    if (initial_matrix) MAT_COPY(state.ts_mat, initial_matrix);
    const int status = db_walk_tree(dbip, static_cast<int>(objects.size()), objects.data(), 1,
        &state, nullptr, bounds_region_end, bounds_leaf, &bounds);
    if (status < 0 || bounds.error || !bounds.have_bounds) {
        error = "unable to determine finite rendered-object bounds";
        return false;
    }
    VMOVE(minimum, bounds.minimum);
    VMOVE(maximum, bounds.maximum);
    return true;
}

static std::vector<std::string>
table_strings(const bu_ptbl *table)
{
    std::vector<std::string> result;
    if (!table) return result;
    for (size_t i = 0; i < BU_PTBL_LEN(table); ++i) {
        const char *value = reinterpret_cast<const char *>(BU_PTBL_GET(table, i));
        if (value && value[0]) result.emplace_back(value);
    }
    return result;
}

static std::vector<std::string>
rendered_objects(const rtwizard_settings *s, char picture_type)
{
    std::set<std::string> unique;
    const bool use_color = picture_type == '\0' || picture_type == 'A' || picture_type == 'C' ||
	picture_type == 'D' || picture_type == 'E' || picture_type == 'F';
    const bool use_line = picture_type == '\0' || picture_type == 'B' || picture_type == 'C' ||
	picture_type == 'D' || picture_type == 'F';
    const bool use_ghost = picture_type == '\0' || picture_type == 'E' || picture_type == 'F';
    std::vector<const bu_ptbl *> tables;
    if (use_color) tables.push_back(s->color);
    if (use_line) tables.push_back(s->line);
    if (use_ghost) tables.push_back(s->ghost);
    for (const auto *table : tables) {
        for (const std::string &object : table_strings(table)) unique.insert(object);
    }
    return std::vector<std::string>(unique.begin(), unique.end());
}

static ViewState
resolve_view(const rtwizard_settings *s, db_i *dbip, char picture_type)
{
    std::vector<std::string> objects = rendered_objects(s, picture_type);
    std::vector<const char *> names;
    for (const std::string &object : objects) names.push_back(object.c_str());
    if (names.empty()) throw std::runtime_error("no rendered objects were specified");

    ViewState result;
    std::string bounds_error;
    if (!display_bounds(dbip, names, result.bounds_min, result.bounds_max, bounds_error))
        throw std::runtime_error(bounds_error);

    GedSession session(ged_open("db", bu_vls_addr(s->input_file), 1));
    if (!session)
        throw std::runtime_error("unable to open geometry database for view setup");
    for (const std::string &object : objects) {
        const char *arguments[] = {"draw", object.c_str()};
        if (ged_exec_draw(session.get(), 2, arguments) != BRLCAD_OK) {
            const std::string detail = bu_vls_addr(session->ged_result_str);
            throw std::runtime_error("unable to draw '" + object + "' for view setup" +
                (detail.empty() ? std::string() : ": " + detail));
        }
    }
    const char *autoview_arguments[] = {"autoview"};
    if (ged_exec_autoview(session.get(), 1, autoview_arguments) != BRLCAD_OK)
        throw std::runtime_error("unable to calculate an automatic view: " +
            std::string(bu_vls_addr(session->ged_result_str)));
    bview *view = session->ged_gvp;

    vect_t aet = {
        s->az < DBL_MAX ? s->az : 35.0,
        s->el < DBL_MAX ? s->el : 25.0,
        s->tw < DBL_MAX ? s->tw : 0.0
    };
    bv_view_set_aet(view, aet);
    if (s->zoom < DBL_MAX) {
        if (!(s->zoom > 0.0)) {
            throw std::runtime_error("zoom must be positive");
        }
        bv_view_set_size(view, view->gv_size / s->zoom);
    }
    result.perspective = s->perspective < DBL_MAX ? s->perspective : 0.0;
    view->gv_perspective = result.perspective;
    if (s->center[0] < DBL_MAX) bv_view_set_center_vec(view, s->center);
    bv_update(view);

    const bool low_level = s->viewsize < DBL_MAX && s->orientation[0] < DBL_MAX && s->eye_pt[0] < DBL_MAX;
    if (low_level) {
        result.size = s->viewsize;
        HMOVE(result.orientation, s->orientation);
        VMOVE(result.eye, s->eye_pt);
        return result;
    }

    const char *eye_arguments[] = {"get_eyemodel"};
    if (ged_exec_get_eyemodel(session.get(), 1, eye_arguments) != BRLCAD_OK)
        throw std::runtime_error("unable to resolve the renderer eye point: " +
            std::string(bu_vls_addr(session->ged_result_str)));
    result.size = view->gv_size;
    quat_mat2quat(result.orientation, view->gv_rotation);
    double parsed_size = 0.0;
    quat_t parsed_orientation;
    if (std::sscanf(bu_vls_addr(session->ged_result_str),
            "viewsize %lf;\norientation %lf %lf %lf %lf;\neye_pt %lf %lf %lf;",
            &parsed_size, &parsed_orientation[0], &parsed_orientation[1],
            &parsed_orientation[2], &parsed_orientation[3], &result.eye[0],
            &result.eye[1], &result.eye[2]) != 8)
        throw std::runtime_error("libged returned an invalid renderer view description");
    return result;
}

static std::string
numbers(const fastf_t *values, size_t count, char separator = ' ')
{
    std::ostringstream stream;
    stream << std::setprecision(17);
    for (size_t i = 0; i < count; ++i) {
        if (i) stream << separator;
        stream << values[i];
    }
    return stream.str();
}

static std::string
quote_control_word(const std::string &word)
{
    if (word.find_first_of(";\r\n") != std::string::npos)
        throw std::runtime_error("animation object names may not contain command separators");
    std::string result = "\"";
    for (char c : word) {
        if (c == '\\' || c == '"') result.push_back('\\');
        result.push_back(c);
    }
    result.push_back('"');
    return result;
}

static int
run_process(JobContext &job, const std::vector<std::string> &arguments,
    const std::vector<std::string> &objects, const std::vector<std::string> &animation_commands)
{
    if (arguments.empty()) throw std::runtime_error("internal error: empty subprocess command");
    if (job.cancelled()) throw std::runtime_error("render cancelled");

    std::vector<const char *> argv;
    for (const std::string &argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    std::ostringstream command_log;
    command_log << "Running";
    for (const std::string &argument : arguments) command_log << ' ' << argument;
    job.log(command_log.str());

    bu_process *process = nullptr;
    bu_process_create(&process, argv.data(), BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW);
    if (!process) throw std::runtime_error("unable to start render subprocess");
    try {
        job.set_pid(bu_process_pid(process));
    } catch (...) {
        bu_pid_terminate(bu_process_pid(process));
        bu_process_wait_n(&process, 0);
        throw;
    }

    FILE *input = bu_process_file_open(process, BU_PROCESS_STDIN);
    if (!animation_commands.empty()) {
        if (!input) {
            bu_pid_terminate(bu_process_pid(process));
            bu_process_wait_n(&process, 0);
            throw std::runtime_error("unable to open renderer control stream");
        }
        for (const std::string &command : animation_commands) std::fprintf(input, "%s;\n", command.c_str());
        std::fputs("tree", input);
        for (const std::string &object : objects) {
            const std::string quoted = quote_control_word(object);
            std::fprintf(input, " %s", quoted.c_str());
        }
        std::fputs(";\nend;\n", input);
        std::fflush(input);
    }
    bu_process_file_close(process, BU_PROCESS_STDIN);

    std::string output;
    char buffer[4096];
    while (true) {
        if (job.cancelled()) {
            bu_pid_terminate(bu_process_pid(process));
        }
        int count = bu_process_read_n(process, BU_PROCESS_STDOUT, sizeof(buffer) - 1, buffer);
        if (count <= 0) break;
        buffer[count] = '\0';
        output.append(buffer, static_cast<size_t>(count));
        job.log(std::string(buffer, static_cast<size_t>(count)));
    }
    const int status = bu_process_wait_n(&process, 0);
    job.clear_pid();
    if (job.cancelled()) throw std::runtime_error("render cancelled");
    if (status != 0) {
        std::ostringstream message;
        message << fs::path(arguments.front()).filename().string() << " failed with status " << status;
        if (!output.empty()) message << ": " << output;
        throw std::runtime_error(message.str());
    }
    return status;
}

static std::string
executable(const char *name)
{
    const char *path = bu_dir(nullptr, 0, BU_DIR_BIN, name, BU_DIR_EXT, nullptr);
    if (!path || !path[0]) throw std::runtime_error(std::string("unable to locate ") + name);
    return path;
}

static std::vector<std::string>
base_command(const rtwizard_settings *s, const ViewState &view, const char *program,
    const fs::path &output, const std::string &cut_plane)
{
    const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
    const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
    std::vector<std::string> command = {executable(program), "-w", std::to_string(width),
        "-n", std::to_string(height)};
    if (s->benchmark) command.push_back("-B");
    if (s->cpus > 0) {
        command.push_back("-P");
        command.push_back(std::to_string(s->cpus));
    }
    if (!cut_plane.empty()) {
        command.push_back("-k");
        command.push_back(cut_plane);
    }
    command.insert(command.end(), {"-o", output.string(), "-V",
        std::to_string(static_cast<double>(width) / static_cast<double>(height)),
        "-R", "-A", "0.9", "-p", std::to_string(view.perspective),
        "-c", "viewsize " + std::to_string(view.size),
        "-c", "orientation " + numbers(view.orientation, 4),
        "-c", "eye_pt " + numbers(view.eye, 3)});
    return command;
}

static std::array<unsigned char, 3>
color_chars(const bu_color *color)
{
    std::array<unsigned char, 3> value = {0, 0, 0};
    bu_color_to_rgb_chars(color, value.data());
    return value;
}

static std::string
color_string(const std::array<unsigned char, 3> &color, char separator)
{
    return std::to_string(color[0]) + separator + std::to_string(color[1]) + separator + std::to_string(color[2]);
}

static Image
blank_image(size_t width, size_t height, const std::array<unsigned char, 3> &color)
{
    Image image(icv_create(width, height, ICV_COLOR_SPACE_RGB));
    if (!image) throw std::runtime_error("unable to allocate render image");
    for (size_t pixel = 0; pixel < width * height; ++pixel) {
        for (size_t channel = 0; channel < 3; ++channel)
            image->data[pixel * 3 + channel] = static_cast<double>(color[channel]) / 255.0;
    }
    return image;
}

static Image
copy_image(const icv_image_t *source)
{
    Image image(icv_create(source->width, source->height, source->color_space));
    if (!image || image->channels != source->channels)
        throw std::runtime_error("unable to copy render image");
    std::copy(source->data, source->data + source->width * source->height * source->channels,
        image->data);
    return image;
}

static Image
read_pix(const fs::path &path, size_t width, size_t height)
{
    Image image(icv_read(path.string().c_str(), BU_MIME_IMAGE_PIX, width, height));
    if (!image) throw std::runtime_error("unable to read renderer output '" + path.string() + "'");
    return image;
}

static bool
pixel_matches(const icv_image_t *image, size_t pixel, const std::array<unsigned char, 3> &color)
{
    for (size_t channel = 0; channel < 3; ++channel) {
        int value = static_cast<int>(std::lround(std::clamp(image->data[pixel * 3 + channel], 0.0, 1.0) * 255.0));
        if (value != static_cast<int>(color[channel])) return false;
    }
    return true;
}

static void
overlay_non_background(icv_image_t *base, const icv_image_t *overlay,
    const std::array<unsigned char, 3> &background,
    const std::array<unsigned char, 3> &foreground, bool mask_only)
{
    const size_t pixels = base->width * base->height;
    for (size_t pixel = 0; pixel < pixels; ++pixel) {
        if (!pixel_matches(overlay, pixel, background)) {
            for (size_t channel = 0; channel < 3; ++channel) {
                base->data[pixel * 3 + channel] = mask_only ?
                    static_cast<double>(foreground[channel]) / 255.0 :
                    overlay->data[pixel * 3 + channel];
            }
        }
    }
}

static Image
render_rt(JobContext &job, const rtwizard_settings *s, const ViewState &view,
    const std::vector<std::string> &objects, const std::string &name,
    const std::string &cut_plane, const std::vector<std::string> &animation_commands)
{
    const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
    const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
    const auto background = color_chars(s->bkg_color);
    fs::path output = job.file(name + ".pix");
    std::vector<std::string> command = base_command(s, view, "rt", output, cut_plane);
    command.insert(command.end(), {"-C", color_string(background, '/')});
    if (s->ao_samples > 0) {
        std::string ambient = "set ambSamples=" + std::to_string(s->ao_samples);
        if (s->ao_radius > 0.0) ambient += " ambRadius=" + std::to_string(s->ao_radius);
        command.insert(command.end(), {"-c", ambient});
    }
    command.push_back(bu_vls_addr(s->input_file));
    if (animation_commands.empty()) command.insert(command.end(), objects.begin(), objects.end());
    run_process(job, command, objects, animation_commands);
    return read_pix(output, width, height);
}

static Image
render_edge(JobContext &job, const rtwizard_settings *s, const ViewState &view,
    const std::vector<std::string> &objects, const std::vector<std::string> &occluders,
    const std::string &name, const std::string &cut_plane,
    const std::vector<std::string> &animation_commands,
    std::array<unsigned char, 3> &edge_background,
    std::array<unsigned char, 3> &edge_foreground, bool &mask_only)
{
    const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
    const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
    edge_foreground = color_chars(s->line_color);
    edge_background = occluders.empty() ? color_chars(s->bkg_color) : color_chars(s->non_line_color);
    std::array<unsigned char, 3> render_foreground = edge_foreground;
    mask_only = render_foreground == edge_background;
    if (mask_only) {
        render_foreground = {{static_cast<unsigned char>(255 - edge_background[0]),
            static_cast<unsigned char>(255 - edge_background[1]),
            static_cast<unsigned char>(255 - edge_background[2])}};
    }
    fs::path output = job.file(name + ".pix");
    std::vector<std::string> command = base_command(s, view, "rtedge", output, cut_plane);
    command.insert(command.end(), {"-c", "set fg=" + color_string(render_foreground, ','),
        "-c", "set bg=" + color_string(edge_background, ',')});
    if (!occluders.empty()) {
        std::ostringstream list;
        for (size_t i = 0; i < occluders.size(); ++i) {
            if (i) list << ' ';
            list << occluders[i];
        }
        command.insert(command.end(), {"-c", "set om=" + std::to_string(s->occlusion),
            "-c", "set oo=\"" + list.str() + "\""});
    }
    command.push_back(bu_vls_addr(s->input_file));
    if (animation_commands.empty()) command.insert(command.end(), objects.begin(), objects.end());
    run_process(job, command, objects, animation_commands);
    return read_pix(output, width, height);
}

static Image
compose_frame(JobContext &job, const rtwizard_settings *s, const ViewState &view,
    const std::string &cut_plane, const std::vector<std::string> &animation_commands,
    char picture_type)
{
    const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
    const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
    const auto background = color_chars(s->bkg_color);
    const bool use_color = picture_type == '\0' || picture_type == 'A' || picture_type == 'C' ||
	picture_type == 'D' || picture_type == 'E' || picture_type == 'F';
    const bool use_line = picture_type == '\0' || picture_type == 'B' || picture_type == 'C' ||
	picture_type == 'D' || picture_type == 'F';
    const bool use_ghost = picture_type == '\0' || picture_type == 'E' || picture_type == 'F';
    const auto colors = use_color ? table_strings(s->color) : std::vector<std::string>();
    const auto ghosts = use_ghost ? table_strings(s->ghost) : std::vector<std::string>();
    const auto edges = use_line ? table_strings(s->line) : std::vector<std::string>();
    std::vector<std::string> occluders = colors;
    occluders.insert(occluders.end(), ghosts.begin(), ghosts.end());
    std::sort(occluders.begin(), occluders.end());
    occluders.erase(std::unique(occluders.begin(), occluders.end()), occluders.end());

    Image result = colors.empty() ? blank_image(width, height, background) :
        render_rt(job, s, view, colors, "color", cut_plane, animation_commands);

    if (!ghosts.empty()) {
        Image ghost = render_rt(job, s, view, ghosts, "ghost", cut_plane, animation_commands);
        Image occupied = render_rt(job, s, view, occluders, "occupied", cut_plane, animation_commands);
        Image color_only = copy_image(result.get());
        const double gamma = s->ghost_intensity;
        if (!(gamma > 0.0)) throw std::runtime_error("ghost intensity must be positive");
        const size_t pixels = width * height;
        for (size_t pixel = 0; pixel < pixels; ++pixel) {
            const double gray = 0.26 * ghost->data[pixel * 3] +
                0.66 * ghost->data[pixel * 3 + 1] + 0.08 * ghost->data[pixel * 3 + 2];
            /* Match libicv's byte encoding: lrint obeys round-to-even for
             * exact half values, which matters for the historical ghost
             * recipe's weighted CRT conversion. */
            const int gray_byte = static_cast<int>(std::lrint(std::clamp(gray, 0.0, 1.0) * 255.0));
            const int faded_byte = static_cast<int>(std::floor(
                std::clamp(std::pow((gray_byte + 4.0) / 259.0, 1.0 / gamma) * 255.0,
                    0.0, 255.0) + 0.5));
            const double faded = static_cast<double>(faded_byte) / 255.0;
            const bool color_bg = pixel_matches(color_only.get(), pixel, background);
            const bool occupied_bg = pixel_matches(occupied.get(), pixel, background);
            for (size_t channel = 0; channel < 3; ++channel) {
                const double merged = color_bg ? faded : color_only->data[pixel * 3 + channel];
                result->data[pixel * 3 + channel] = occupied_bg ?
                    color_only->data[pixel * 3 + channel] : merged;
            }
        }
    }

    if (!edges.empty()) {
        std::array<unsigned char, 3> edge_background;
        std::array<unsigned char, 3> edge_foreground;
        bool mask_only = false;
        Image edge = render_edge(job, s, view, edges, occluders, "edges", cut_plane,
            animation_commands, edge_background, edge_foreground, mask_only);
        if (colors.empty() && ghosts.empty()) {
            result = std::move(edge);
        } else {
            overlay_non_background(result.get(), edge.get(), edge_background,
                edge_foreground, mask_only);
        }
    }
    return result;
}

static int
write_framebuffer(JobContext &job, const rtwizard_settings *s, const icv_image_t *image)
{
    const int width = static_cast<int>(image->width);
    const int height = static_cast<int>(image->height);
    if (s->port < 0) {
        if (!bu_vls_strlen(s->fb_dev)) return 0;
        const std::string device = bu_vls_addr(s->fb_dev);
        struct fb *framebuffer = fb_open(device.c_str(), width, height);
        if (!framebuffer) throw std::runtime_error("unable to open framebuffer '" + device + "'");
        std::vector<unsigned char> pixels(image->width * image->height * 3);
        for (size_t i = 0; i < pixels.size(); ++i)
            pixels[i] = static_cast<unsigned char>(std::lround(std::clamp(image->data[i], 0.0, 1.0) * 255.0));
        const int written = fb_writerect(framebuffer, 0, 0, width, height, pixels.data());
        fb_close(framebuffer);
        if (written < 0) throw std::runtime_error("unable to write rendered image to framebuffer");
        return 0;
    }

    struct fb *framebuffer = nullptr;
    bu_process *server = nullptr;
    std::unique_ptr<ScopedFramebufferToken> server_token;
    std::string target;
    std::string managed_ipc_addr;

    auto stop_server = [&](bool graceful = false) {
        if (!server) return;
        if (graceful) {
            for (int attempt = 0; attempt < 50 && bu_process_alive(server); ++attempt)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (bu_process_alive(server)) bu_pid_terminate(bu_process_pid(server));
        const int fd = bu_process_fileno(server, BU_PROCESS_STDOUT);
        char buffer[4096];
        while (bu_process_pending(fd)) {
            const int count = bu_process_read_n(server, BU_PROCESS_STDOUT, sizeof(buffer), buffer);
            if (count <= 0) break;
            job.log(std::string(buffer, static_cast<size_t>(count)));
        }
        bu_process_wait_n(&server, 0);
        job.clear_pid();
        if (!managed_ipc_addr.empty()) {
            if (pkg_ipc_addr_cleanup(managed_ipc_addr.c_str()) != 0)
                job.log("Warning: unable to remove local framebuffer IPC endpoint.\n");
            managed_ipc_addr.clear();
        }
    };

    auto launch_server = [&](const std::vector<std::string> &command, const std::string &connection) {
        std::vector<const char *> argv;
        for (const std::string &argument : command) argv.push_back(argument.c_str());
        argv.push_back(nullptr);
        bu_process_create(&server, argv.data(), BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW);
        if (!server) {
            if (!managed_ipc_addr.empty()) {
                (void)pkg_ipc_addr_cleanup(managed_ipc_addr.c_str());
                managed_ipc_addr.clear();
            }
            throw std::runtime_error("unable to start framebuffer server");
        }
        bu_process_file_close(server, BU_PROCESS_STDIN);
        try {
            job.set_pid(bu_process_pid(server));
        } catch (...) {
            bu_pid_terminate(bu_process_pid(server));
            bu_process_wait_n(&server, 0);
            if (!managed_ipc_addr.empty()) {
                (void)pkg_ipc_addr_cleanup(managed_ipc_addr.c_str());
                managed_ipc_addr.clear();
            }
            throw;
        }
        for (int attempt = 0; attempt < 30 && !framebuffer && bu_process_alive(server); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            framebuffer = fb_open(connection.c_str(), width, height);
        }
        if (!framebuffer) stop_server();
        return framebuffer != nullptr;
    };

    auto ensure_server_token = [&]() {
        if (!server_token) server_token = std::make_unique<ScopedFramebufferToken>();
    };

    if (s->fb_transport != RTWIZARD_FB_TCP) {
        char ipc_addr[512] = {0};
        if (pkg_ipc_addr(ipc_addr, sizeof(ipc_addr), "rtwizard-fbserv") == 0) {
            ensure_server_token();
            managed_ipc_addr = ipc_addr;
            target = std::string("ipc:") + ipc_addr;
            std::vector<std::string> command = {executable("fbserv"), "-A", "-w", std::to_string(width),
                "-n", std::to_string(height), "-I", ipc_addr};
            if (bu_vls_strlen(s->fb_dev)) command.insert(command.end(), {"-F", bu_vls_addr(s->fb_dev)});
            job.log("Starting managed framebuffer server over local IPC: " + std::string(ipc_addr) + "\n");
            (void)launch_server(command, target);
        } else {
            job.log("Local framebuffer IPC is unavailable on this platform.\n");
        }
        if (!framebuffer && s->fb_transport == RTWIZARD_FB_IPC)
            throw std::runtime_error("unable to start a local IPC framebuffer server");
    }

    if (!framebuffer) {
        target = std::to_string(s->port);
        framebuffer = fb_open(target.c_str(), width, height);
        if (framebuffer) {
            job.log("Using existing TCP framebuffer server on port " + target + ".\n");
        } else {
            ensure_server_token();
            std::vector<std::string> command = {executable("fbserv"), "-A", "-w", std::to_string(width),
                "-n", std::to_string(height), "-p", target};
            if (bu_vls_strlen(s->fb_dev)) command.insert(command.end(), {"-F", bu_vls_addr(s->fb_dev)});
            job.log("Starting managed TCP framebuffer server on port " + target + ".\n");
            (void)launch_server(command, target);
        }
    }
    if (!framebuffer) {
        stop_server();
        throw std::runtime_error("unable to open framebuffer '" + target + "'");
    }
    std::vector<unsigned char> pixels(image->width * image->height * 3);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<unsigned char>(std::lround(std::clamp(image->data[i], 0.0, 1.0) * 255.0));
    int written = fb_writerect(framebuffer, 0, 0, static_cast<int>(image->width),
        static_cast<int>(image->height), pixels.data());
    if (server) {
        /* MSG_FBFREE asks fbserv to leave its service loop and close the
         * opaque IPC listener, including its filesystem endpoint. */
        (void)fb_free(framebuffer);
        stop_server(true);
    } else {
        fb_close(framebuffer);
    }
    if (written < 0) throw std::runtime_error("unable to write rendered image to framebuffer");
    return 0;
}

static void
write_image_atomic(const rtwizard_settings *s, icv_image_t *image)
{
    if (!bu_vls_strlen(s->output_file)) return;
    const fs::path output(bu_vls_addr(s->output_file));
    const fs::path temp = output.parent_path() /
        (output.stem().string() + ".tmp-" + std::to_string(bu_pid()) + output.extension().string());
    if (icv_write(image, temp.string().c_str(), BU_MIME_IMAGE_AUTO) != 0) {
        std::error_code ec;
        fs::remove(temp, ec);
        throw std::runtime_error("unable to write output image '" + output.string() + "'");
    }

    if (!publish_file(temp, output)) {
	std::error_code ec;
        fs::remove(temp, ec);
        throw std::runtime_error("unable to replace output image '" + output.string() + "'");
    }
}

static rtwizard::AnimationPlan
preset_plan(const rtwizard_settings *s, const ViewState &base, db_i *dbip,
    char picture_type)
{
    rtwizard::AnimationPlan plan;
    plan.duration = s->animation_duration > 0.0 ? s->animation_duration : 5.0;
    plan.fps = s->animation_fps > 0 ? s->animation_fps : 10;
    const std::string preset = bu_vls_addr(s->animation_preset);
    bool cyclic = (preset == "orbit" && std::fabs(s->orbit_angle) >= 360.0) ||
        (preset == "turntable" && std::fabs(s->turntable_angle) >= 360.0);
    if (s->animation_cyclic >= 0) cyclic = s->animation_cyclic != 0;
    plan.cyclic = cyclic;
    plan.plays = s->animation_plays >= 0 ? s->animation_plays : (cyclic ? 0 : 1);
    int frame_count = s->animation_frames;
    if (preset == "cut" && s->cut_steps >= 2) frame_count = s->cut_steps;
    if (frame_count < 2) frame_count = static_cast<int>(std::llround(plan.duration * plan.fps)) + (cyclic ? 0 : 1);
    if (frame_count < 2) throw std::runtime_error("animation requires at least two frames");
    plan.frames.reserve(static_cast<size_t>(frame_count));

    if (preset == "cut") {
        vect_t direction;
        if (s->cut_direction_set) {
            VMOVE(direction, s->cut_direction);
            VUNITIZE(direction);
        } else {
            mat_t rotation;
            quat_quat2mat(rotation, base.orientation);
            VSET(direction, -rotation[8], -rotation[9], -rotation[10]);
            VUNITIZE(direction);
        }
        vect_t reference, xaxis, yaxis;
        if (std::fabs(direction[2]) < 0.9) VSET(reference, 0.0, 0.0, 1.0);
        else VSET(reference, 0.0, 1.0, 0.0);
        VCROSS(xaxis, reference, direction);
        VUNITIZE(xaxis);
        VCROSS(yaxis, direction, xaxis);
        VUNITIZE(yaxis);
        mat_t projection_matrix;
        MAT_IDN(projection_matrix);
        VMOVE(&projection_matrix[0], xaxis);
        VMOVE(&projection_matrix[4], yaxis);
        VMOVE(&projection_matrix[8], direction);
        std::vector<std::string> object_storage = rendered_objects(s, picture_type);
        std::vector<const char *> object_names;
        for (const std::string &object : object_storage) object_names.push_back(object.c_str());
        point_t projected_minimum, projected_maximum;
        std::string bounds_error;
        if (!display_bounds(dbip, object_names, projected_minimum, projected_maximum,
                bounds_error, projection_matrix))
            throw std::runtime_error("unable to determine cutting-plane bounds: " + bounds_error);
        const double minimum = projected_minimum[2];
        const double maximum = projected_maximum[2];
        const double span = maximum - minimum;
        if (!(span > 0.0)) throw std::runtime_error("rendered objects have degenerate cutting-plane bounds");
        const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
        const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
        const double epsilon = std::max({std::fabs(span) / static_cast<double>(std::max(width, height)),
            std::fabs(span) * 0.05, 1.0e-9});
        for (int index = 0; index < frame_count; ++index) {
            const double fraction = static_cast<double>(index) / static_cast<double>(frame_count - 1);
            const double distance = minimum - epsilon + fraction * span;
            point_t point;
            VSCALE(point, direction, distance);
            rtwizard::AnimationFrame frame;
            frame.time = plan.duration * fraction;
            frame.view_size = base.size;
            std::copy(base.orientation, base.orientation + 4, frame.orientation.begin());
            std::copy(base.eye, base.eye + 3, frame.eye.begin());
            frame.perspective = base.perspective;
            std::ostringstream cut;
            cut << std::setprecision(17) << point[0] << ',' << point[1] << ',' << point[2] << ','
                << direction[0] << ',' << direction[1] << ',' << direction[2];
            frame.cut_plane = cut.str();
            plan.frames.push_back(std::move(frame));
        }
        return plan;
    }

    if (preset == "orbit") {
        vect_t axis;
        VMOVE(axis, s->orbit_axis);
        VUNITIZE(axis);
        point_t center;
        if (s->orbit_center[0] < DBL_MAX) VMOVE(center, s->orbit_center);
        else VADD2SCALE(center, base.bounds_min, base.bounds_max, 0.5);
        vect_t radial;
        VSUB2(radial, base.eye, center);
        double radius = MAGNITUDE(radial);
        if (!(radius > 1.0e-12)) {
            vect_t reference;
            if (std::fabs(axis[2]) < 0.9) VSET(reference, 0, 0, 1); else VSET(reference, 1, 0, 0);
            VCROSS(radial, axis, reference);
            VUNITIZE(radial);
            radius = base.size;
            VSCALE(radial, radial, radius);
        }
        if (s->orbit_radius < DBL_MAX) {
            radius = s->orbit_radius;
            VUNITIZE(radial);
            VSCALE(radial, radial, radius);
        }
        if (s->orbit_elevation < DBL_MAX) {
            const double elevation = s->orbit_elevation * DEG2RAD;
            const double along = VDOT(radial, axis);
            vect_t in_plane;
            VJOIN1(in_plane, radial, -along, axis);
            if (MAGNITUDE(in_plane) < 1.0e-12) {
                vect_t reference;
                if (std::fabs(axis[2]) < 0.9) VSET(reference, 0, 0, 1); else VSET(reference, 1, 0, 0);
                VCROSS(in_plane, axis, reference);
            }
            VUNITIZE(in_plane);
            VCOMB2(radial, radius * std::cos(elevation), in_plane, radius * std::sin(elevation), axis);
        }
        for (int index = 0; index < frame_count; ++index) {
            const double fraction = static_cast<double>(index) /
                static_cast<double>(cyclic ? frame_count : frame_count - 1);
            const double angle = fraction * s->orbit_angle * DEG2RAD;
            const double cosine = std::cos(angle), sine = std::sin(angle), dot = VDOT(axis, radial);
            vect_t cross, rotated;
            VCROSS(cross, axis, radial);
            for (int component = 0; component < 3; ++component)
                rotated[component] = radial[component] * cosine + cross[component] * sine +
                    axis[component] * dot * (1.0 - cosine);
            point_t eye;
            VADD2(eye, center, rotated);
            vect_t direction;
            VSUB2(direction, center, eye);
            VUNITIZE(direction);
            mat_t matrix;
            quat_t orientation;
            bn_mat_lookat(matrix, direction, 0);
            quat_mat2quat(orientation, matrix);
            rtwizard::AnimationFrame frame;
            frame.time = plan.duration * fraction;
            frame.view_size = base.size;
            std::copy(orientation, orientation + 4, frame.orientation.begin());
            std::copy(eye, eye + 3, frame.eye.begin());
            frame.perspective = base.perspective;
            plan.frames.push_back(std::move(frame));
        }
        return plan;
    }

    if (preset == "turntable") {
        vect_t axis;
        VMOVE(axis, s->turntable_axis);
        VUNITIZE(axis);
        point_t center;
        if (s->turntable_center[0] < DBL_MAX) {
            VMOVE(center, s->turntable_center);
        } else {
            const char *object = bu_vls_addr(s->turntable_object);
            point_t minimum, maximum;
            bu_vls messages = BU_VLS_INIT_ZERO;
            if (rt_obj_bounds(&messages, dbip, 1, &object, 0, minimum, maximum) == BRLCAD_ERROR) {
                std::string detail = bu_vls_addr(&messages);
                bu_vls_free(&messages);
                throw std::runtime_error("unable to determine turntable-object bounds: " + detail);
            }
            bu_vls_free(&messages);
            VADD2SCALE(center, minimum, maximum, 0.5);
        }
        std::string path = bu_vls_addr(s->turntable_object);
        if (path.find_first_of(";\r\n\t ") != std::string::npos)
            throw std::runtime_error("turntable object paths may not contain whitespace or semicolons");
        if (path.empty() || path.front() != '/') path.insert(path.begin(), '/');
        for (int index = 0; index < frame_count; ++index) {
            const double fraction = static_cast<double>(index) /
                static_cast<double>(cyclic ? frame_count : frame_count - 1);
            const double angle = fraction * s->turntable_angle * DEG2RAD;
            const double x = axis[0], y = axis[1], z = axis[2];
            const double c = std::cos(angle), sine = std::sin(angle), m = 1.0 - c;
            mat_t matrix;
            MAT_IDN(matrix);
            matrix[0] = c+x*x*m; matrix[1] = x*y*m-z*sine; matrix[2] = x*z*m+y*sine;
            matrix[4] = y*x*m+z*sine; matrix[5] = c+y*y*m; matrix[6] = y*z*m-x*sine;
            matrix[8] = z*x*m-y*sine; matrix[9] = z*y*m+x*sine; matrix[10] = c+z*z*m;
            matrix[3] = center[0] - (matrix[0]*center[0] + matrix[1]*center[1] + matrix[2]*center[2]);
            matrix[7] = center[1] - (matrix[4]*center[0] + matrix[5]*center[1] + matrix[6]*center[2]);
            matrix[11] = center[2] - (matrix[8]*center[0] + matrix[9]*center[1] + matrix[10]*center[2]);
            std::ostringstream command;
            command << std::setprecision(17) << "anim " << path << " matrix lmul";
            for (int element = 0; element < 16; ++element) command << ' ' << matrix[element];
            rtwizard::AnimationFrame frame;
            frame.time = plan.duration * fraction;
            frame.view_size = base.size;
            std::copy(base.orientation, base.orientation + 4, frame.orientation.begin());
            std::copy(base.eye, base.eye + 3, frame.eye.begin());
            frame.perspective = base.perspective;
            frame.commands.push_back(command.str());
            plan.frames.push_back(std::move(frame));
        }
        return plan;
    }
    throw std::runtime_error("unknown animation preset '" + preset + "'");
}

static rtwizard::AnimationPlan
animation_plan(const rtwizard_settings *s, const ViewState &base, db_i *dbip,
    char picture_type)
{
    if (bu_vls_strlen(s->animation_preset)) return preset_plan(s, base, dbip, picture_type);
    rtwizard::AnimationPlan plan;
    std::string error;
    const std::array<double, 4> orientation{{base.orientation[0], base.orientation[1], base.orientation[2], base.orientation[3]}};
    const std::array<double, 3> eye{{base.eye[0], base.eye[1], base.eye[2]}};
    if (!rtwizard::evaluate_animation(bu_vls_addr(s->animation_file), base.size, orientation, eye,
            base.perspective, s->animation_duration, s->animation_fps, s->animation_frames,
            s->animation_plays, s->animation_cyclic, dbip->dbi_local2base, plan, error))
        throw std::runtime_error(error);
    return plan;
}

static bool
valid_frame_file(const fs::path &path, size_t width, size_t height)
{
    Image image(icv_read(path.string().c_str(), BU_MIME_IMAGE_AUTO, width, height));
    return image && image->width == width && image->height == height;
}

static void
write_png_atomic(icv_image_t *image, const fs::path &output)
{
    const fs::path temp = output.parent_path() /
        (output.stem().string() + ".tmp-" + std::to_string(bu_pid()) + output.extension().string());
    if (icv_write(image, temp.string().c_str(), BU_MIME_IMAGE_PNG) != 0)
        throw std::runtime_error("unable to write animation frame '" + output.string() + "'");

    if (!publish_file(temp, output)) {
	std::error_code ec;
        fs::remove(temp, ec);
        throw std::runtime_error("unable to publish animation frame '" + output.string() + "'");
    }
}

static void
write_animation_atomic(const rtwizard_settings *s, const rtwizard::AnimationPlan &plan,
    const std::vector<fs::path> &frames, size_t width, size_t height)
{
    if (!bu_vls_strlen(s->output_file)) return;
    const fs::path output(bu_vls_addr(s->output_file));
    std::string extension = output.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const icv_anim_format_t format = (extension == ".avi" || extension == ".mjpg") ? ICV_ANIM_MJPG : ICV_ANIM_APNG;
    std::unique_ptr<icv_anim_t, decltype(&icv_anim_destroy)> animation(
        icv_anim_create(format, static_cast<uint32_t>(width), static_cast<uint32_t>(height), plan.fps),
        &icv_anim_destroy);
    if (!animation) throw std::runtime_error("unable to create output animation");
    icv_anim_set_plays(animation.get(), static_cast<uint32_t>(plan.plays));
    for (const fs::path &frame_path : frames) {
        Image frame(icv_read(frame_path.string().c_str(), BU_MIME_IMAGE_AUTO, width, height));
        if (!frame || icv_anim_add_frame(animation.get(), frame.get()) != 0)
            throw std::runtime_error("unable to add animation frame '" + frame_path.string() + "'");
    }
    const fs::path temp = output.parent_path() /
        (output.stem().string() + ".tmp-" + std::to_string(bu_pid()) + output.extension().string());
    if (icv_anim_write(animation.get(), temp.string().c_str()) != 0)
        throw std::runtime_error("unable to write output animation '" + output.string() + "'");

    if (!publish_file(temp, output)) {
	std::error_code ec;
        fs::remove(temp, ec);
        throw std::runtime_error("unable to publish output animation '" + output.string() + "'");
    }
}

static void
render_animation(JobContext &job, const rtwizard_settings *s, const ViewState &base,
    db_i *dbip, char picture_type)
{
    const size_t width = s->width_set ? s->width : (s->size_set ? s->size : s->width);
    const size_t height = s->height_set ? s->height : (s->size_set ? s->size : s->height);
    const rtwizard::AnimationPlan plan = animation_plan(s, base, dbip, picture_type);
    fs::path frame_directory = bu_vls_strlen(s->frame_dir) ? fs::path(bu_vls_addr(s->frame_dir)) : job.directory;
    std::error_code ec;
    fs::create_directories(frame_directory, ec);
    if (ec) throw std::runtime_error("unable to create animation frame directory '" + frame_directory.string() + "'");
    std::vector<fs::path> frame_paths;
    frame_paths.reserve(plan.frames.size());
    job.progress(0, static_cast<int>(plan.frames.size()));
    for (size_t index = 0; index < plan.frames.size(); ++index) {
        std::ostringstream name;
        name << "frame-" << std::setw(6) << std::setfill('0') << index << ".png";
        fs::path path = frame_directory / name.str();
        if (fs::exists(path)) {
            if (s->resume && valid_frame_file(path, width, height)) {
                job.log("Using existing frame " + std::to_string(index + 1) + "/" + std::to_string(plan.frames.size()));
                frame_paths.push_back(path);
                job.progress(static_cast<int>(index + 1), static_cast<int>(plan.frames.size()));
                continue;
            }
            if (!s->resume && bu_vls_strlen(s->frame_dir))
                throw std::runtime_error("animation frame already exists: " + path.string() + " (use --resume)");
        }
        const rtwizard::AnimationFrame &state = plan.frames[index];
        ViewState view = base;
        view.size = state.view_size;
        std::copy(state.orientation.begin(), state.orientation.end(), view.orientation);
        std::copy(state.eye.begin(), state.eye.end(), view.eye);
        view.perspective = state.perspective;
        job.log("Rendering frame " + std::to_string(index + 1) + "/" + std::to_string(plan.frames.size()));
        Image image = compose_frame(job, s, view, state.cut_plane, state.commands, picture_type);
        write_png_atomic(image.get(), path);
        frame_paths.push_back(path);
        if (job.callbacks && job.callbacks->frame)
            job.callbacks->frame(job.callback_data, path.string().c_str(), static_cast<int>(index), static_cast<int>(plan.frames.size()));
        job.progress(static_cast<int>(index + 1), static_cast<int>(plan.frames.size()));
    }
    write_animation_atomic(s, plan, frame_paths, width, height);
}

} // namespace

extern "C" int
rtwizard_render(const rtwizard_settings *settings, char picture_type,
    const rtwizard_render_callbacks *callbacks, void *callback_data, char **errmsg)
{
    if (errmsg) *errmsg = nullptr;
    try {
        if (!settings || settings->magic != RTWIZARD_MAGIC)
            throw std::runtime_error("invalid rtwizard settings");
        JobContext job(callbacks, callback_data, bu_vls_addr(settings->log_file),
            bu_vls_addr(settings->pid_file), settings->verbose > 0);
        Database database(db_open(bu_vls_addr(settings->input_file), DB_OPEN_READONLY));
        if (!database || db_dirbuild(database.get()) < 0)
            throw std::runtime_error("unable to open geometry database '" +
                std::string(bu_vls_addr(settings->input_file)) + "'");
        ViewState view = resolve_view(settings, database.get(), picture_type);
        if (bu_vls_strlen(settings->save_view_keyframe)) {
            const std::array<double, 3> eye{{view.eye[0], view.eye[1], view.eye[2]}};
            const std::array<double, 4> orientation{{view.orientation[0], view.orientation[1],
                view.orientation[2], view.orientation[3]}};
            std::string error;
            if (!rtwizard::save_view_keyframe(bu_vls_addr(settings->save_view_keyframe),
                    settings->keyframe_time, settings->replace_keyframe != 0, eye, orientation,
                    view.size, view.perspective, database->dbi_local2base, error))
                throw std::runtime_error(error);
            return BRLCAD_OK;
        }
        if (bu_vls_strlen(settings->animation_preset) || bu_vls_strlen(settings->animation_file)) {
            render_animation(job, settings, view, database.get(), picture_type);
        } else {
            job.progress(0, 1);
            Image frame = compose_frame(job, settings, view, std::string(), {}, picture_type);
            write_image_atomic(settings, frame.get());
            write_framebuffer(job, settings, frame.get());
            job.progress(1, 1);
            if (callbacks && callbacks->frame && bu_vls_strlen(settings->output_file))
                callbacks->frame(callback_data, bu_vls_addr(settings->output_file), 0, 1);
        }
        return BRLCAD_OK;
    } catch (const std::exception &error) {
        set_error(errmsg, error.what());
        return BRLCAD_ERROR;
    }
}
