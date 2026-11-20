/*                       R E N D E R . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 */

#include "common.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
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
#include "dm.h"
#include "dm/fbserv.h"
#include "ged.h"
#include "icv.h"
#include "icv/anim.h"
#include "pkg.h"
#include "raytrace.h"

#include "render.h"
#include "settings.h"

namespace {

namespace fs = std::filesystem;

constexpr int RTWIZARD_DEFAULT_FB_PORT = 0;
constexpr const char *RTWIZARD_DEFAULT_FB_DEVICE = "/dev/mem";
constexpr auto PROCESS_POLL_INTERVAL = std::chrono::milliseconds(10);
constexpr auto PROCESS_TERMINATE_TIMEOUT = std::chrono::milliseconds(1000);
constexpr auto PROCESS_OUTPUT_IDLE_TIMEOUT = std::chrono::milliseconds(50);
constexpr auto FBSERV_GRACEFUL_TIMEOUT = std::chrono::milliseconds(500);
constexpr size_t PROCESS_DRAIN_CHUNK_LIMIT = 64;
constexpr size_t FBSERV_TOKEN_SIZE = 65;
constexpr size_t FBSERV_TLS_PIN_SIZE = 65;
constexpr auto FBSERV_TLS_START_TIMEOUT = std::chrono::milliseconds(5000);

static std::string
drain_process_output(bu_process *process,
    size_t chunk_limit = PROCESS_DRAIN_CHUNK_LIMIT)
{
    std::string output;
    if (!process) return output;

    const int fd = bu_process_fileno(process, BU_PROCESS_STDOUT);
    char buffer[4096];
    size_t chunks = 0;
    while (chunks < chunk_limit && bu_fd_read_ready(fd, 0) > 0) {
        const int count = bu_process_read_n(process, BU_PROCESS_STDOUT,
            sizeof(buffer), buffer);
        if (count <= 0) break;
        output.append(buffer, static_cast<size_t>(count));
        ++chunks;
    }
    return output;
}

static std::string
drain_terminated_process_output(bu_process *process)
{
    std::string output;
    auto idle_deadline = std::chrono::steady_clock::now() +
        PROCESS_OUTPUT_IDLE_TIMEOUT;
    const auto final_deadline = std::chrono::steady_clock::now() +
        PROCESS_TERMINATE_TIMEOUT;

    while (std::chrono::steady_clock::now() < final_deadline) {
        std::string available = drain_process_output(process);
        if (!available.empty()) {
            output.append(available);
            idle_deadline = std::chrono::steady_clock::now() +
                PROCESS_OUTPUT_IDLE_TIMEOUT;
            continue;
        }
        if (std::chrono::steady_clock::now() >= idle_deadline)
            break;
        std::this_thread::sleep_for(PROCESS_POLL_INTERVAL);
    }
    return output;
}

static int
wait_for_process(bu_process *process, std::chrono::milliseconds timeout,
    std::string *output)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (true) {
        std::string available = drain_process_output(process,
            PROCESS_DRAIN_CHUNK_LIMIT);
        if (output) output->append(available);
        const int state = bu_process_poll(process, nullptr);
        if (state != 0) {
            available = drain_process_output(process);
            if (output) output->append(available);
            return state;
        }
        if (timeout.count() == 0 || std::chrono::steady_clock::now() >= deadline)
            return 0;
        std::this_thread::sleep_for(PROCESS_POLL_INTERVAL);
    }
}

static int
stop_process(bu_process **process, std::chrono::milliseconds graceful_timeout,
    std::string *output = nullptr)
{
    if (!process || !*process) return -1;

    int state = wait_for_process(*process, graceful_timeout, output);
    if (state == 1) {
	/* The direct child may exit while descendants still own its output pipe. */
	(void)bu_process_terminate(*process);
    } else {
        (void)bu_process_terminate(*process);
        state = wait_for_process(*process, PROCESS_TERMINATE_TIMEOUT, output);
    }
    if (output) output->append(drain_terminated_process_output(*process));
    return bu_process_wait_n(process,
        state == 1 ? 0 : static_cast<int>(PROCESS_TERMINATE_TIMEOUT.count()));
}

static std::string
framebuffer_token()
{
    std::array<char, FBSERV_TOKEN_SIZE> token = {};
    if (fbs_create_token(token.data(), token.size()) != BRLCAD_OK)
        throw std::runtime_error("unable to establish framebuffer session authentication");
    return token.data();
}

static bool
valid_tls_pin(const std::string &pin)
{
    return pin.size() == FBSERV_TLS_PIN_SIZE - 1 &&
        std::all_of(pin.begin(), pin.end(), [](unsigned char character) {
            return std::isxdigit(character) != 0;
        });
}

static fs::path
framebuffer_pin_file()
{
    char path[MAXPATHLEN] = {0};
    FILE *stream = bu_temp_file(path, sizeof(path));
    if (!stream) throw std::runtime_error("unable to create framebuffer TLS pin file");
    if (fclose(stream) != 0)
        throw std::runtime_error("unable to close framebuffer TLS pin file");
    return path;
}

static std::string
wait_for_tls_pin(const fs::path &path, bu_process *process)
{
    const auto deadline = std::chrono::steady_clock::now() +
        FBSERV_TLS_START_TIMEOUT;
    while (std::chrono::steady_clock::now() < deadline) {
        std::ifstream input(path);
        std::string pin;
        if (input && std::getline(input, pin) && valid_tls_pin(pin)) return pin;
        if (process && bu_process_poll(process, nullptr) != 0) break;
        std::this_thread::sleep_for(PROCESS_POLL_INTERVAL);
    }
    return {};
}

static void
delete_tls_pin_file(const fs::path &path)
{
    if (!path.empty())
        (void)bu_file_delete(path.string().c_str());
}

struct GuiFramebufferServer {
    bu_process *process = nullptr;
    std::string token;
    std::string tls_pin;
    std::string connection;
};

GuiFramebufferServer gui_framebuffer_server;
std::mutex gui_framebuffer_server_mutex;

static void
stop_gui_framebuffer_server()
{
    if (gui_framebuffer_server.process) {
        (void)stop_process(&gui_framebuffer_server.process,
            std::chrono::milliseconds(0));
    }
    gui_framebuffer_server.token.clear();
    gui_framebuffer_server.tls_pin.clear();
    gui_framebuffer_server.connection.clear();
}

static std::string
gui_framebuffer_server_output()
{
    std::string output;
    if (!gui_framebuffer_server.process) return output;

    output = drain_process_output(gui_framebuffer_server.process);
    return output;
}

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

class StagedOutput {
public:
    StagedOutput(const fs::path &destination, const fs::path &job_directory) :
        output(destination), directory(destination.parent_path() /
            ("." + destination.filename().string() + ".tmp-" +
            job_directory.filename().string())), temporary(directory / destination.filename())
    {
        std::error_code error;
        if (!fs::create_directory(directory, error) || error)
            throw std::runtime_error("unable to create output staging directory '" +
                directory.string() + "'");
    }

    ~StagedOutput()
    {
        std::error_code error;
        fs::remove_all(directory, error);
    }

    const fs::path &path() const { return temporary; }
    bool publish() const { return publish_file(temporary, output); }

    StagedOutput(const StagedOutput &) = delete;
    StagedOutput &operator=(const StagedOutput &) = delete;

private:
    fs::path output;
    fs::path directory;
    fs::path temporary;
};

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

    const fs::path &temporary_directory() const { return directory; }

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
};

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
resolve_view(const rtwizard_settings *s, char picture_type)
{
    std::vector<std::string> objects = rendered_objects(s, picture_type);
    if (objects.empty()) throw std::runtime_error("no rendered objects were specified");

    ViewState result;
    result.perspective = s->perspective < DBL_MAX ? s->perspective : 0.0;
    const bool low_level = s->viewsize < DBL_MAX &&
	s->orientation[0] < DBL_MAX && s->eye_pt[0] < DBL_MAX;
    if (low_level) {
	result.size = s->viewsize;
	HMOVE(result.orientation, s->orientation);
	VMOVE(result.eye, s->eye_pt);
	return result;
    }

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
    view->gv_perspective = result.perspective;
    if (s->center[0] < DBL_MAX) bv_view_set_center_vec(view, s->center);
    bv_update(view);

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

static int
run_process(JobContext &job, const std::vector<std::string> &arguments,
    int timeout_seconds)
{
    if (arguments.empty()) throw std::runtime_error("internal error: empty subprocess command");
    if (timeout_seconds < 0) throw std::runtime_error("internal error: negative process timeout");
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
        (void)stop_process(&process, std::chrono::milliseconds(0));
        throw;
    }

    bu_process_file_close(process, BU_PROCESS_STDIN);

    std::string output;
    const auto start = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::seconds(timeout_seconds);
    int state = 0;
    bool cancelled = false;
    bool timed_out = false;
    while (state == 0) {
        std::string available = drain_process_output(process,
            PROCESS_DRAIN_CHUNK_LIMIT);
        output.append(available);
        if (!available.empty()) job.log(available);

        state = bu_process_poll(process, nullptr);
        if (state == 0) {
            cancelled = job.cancelled();
            timed_out = timeout_seconds > 0 &&
                std::chrono::steady_clock::now() - start >= timeout;
        }
        if (state != 0 || cancelled || timed_out) break;
        if (available.empty()) std::this_thread::sleep_for(PROCESS_POLL_INTERVAL);
    }

    if (state < 0 || cancelled || timed_out) {
        std::string trailing;
        (void)stop_process(&process, std::chrono::milliseconds(0), &trailing);
        output.append(trailing);
        if (!trailing.empty()) job.log(trailing);
        job.clear_pid();
        if (cancelled) throw std::runtime_error("render cancelled");
        if (timed_out) {
            throw std::runtime_error(fs::path(arguments.front()).filename().string() +
                " exceeded the " + std::to_string(timeout_seconds) + " second process timeout");
        }
        throw std::runtime_error("unable to monitor render subprocess");
    }

    /* Stop any descendants that retained the output pipe after the direct
     * renderer exited, then drain all bytes they published before teardown. */
    (void)bu_process_terminate(process);
    std::string trailing = drain_terminated_process_output(process);
    output.append(trailing);
    if (!trailing.empty()) job.log(trailing);
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
    const std::string &cut_plane)
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
    command.insert(command.end(), objects.begin(), objects.end());
    run_process(job, command, s->process_timeout);
    return read_pix(output, width, height);
}

static Image
render_edge(JobContext &job, const rtwizard_settings *s, const ViewState &view,
    const std::vector<std::string> &objects, const std::vector<std::string> &occluders,
    const std::string &name, const std::string &cut_plane,
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
            "-c", "set oo=\\\"" + list.str() + "\\\""});
    }
    command.push_back(bu_vls_addr(s->input_file));
    command.insert(command.end(), objects.begin(), objects.end());
    run_process(job, command, s->process_timeout);
    return read_pix(output, width, height);
}

static Image
compose_frame(JobContext &job, const rtwizard_settings *s, const ViewState &view,
    const std::string &cut_plane, char picture_type)
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
        render_rt(job, s, view, colors, "color", cut_plane);

    if (!ghosts.empty()) {
        Image ghost = render_rt(job, s, view, ghosts, "ghost", cut_plane);
        Image occupied = render_rt(job, s, view, occluders, "occupied", cut_plane);
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
            edge_background, edge_foreground, mask_only);
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
    const bool tcp_requested = s->port >= 0;
    const bool existing_target = bu_vls_strlen(s->fb_target) > 0;
    if (!existing_target && !tcp_requested && !bu_vls_strlen(s->fb_dev)) return 0;
    const std::string device = bu_vls_strlen(s->fb_dev) ?
        bu_vls_addr(s->fb_dev) : RTWIZARD_DEFAULT_FB_DEVICE;

    struct fb *framebuffer = nullptr;
    bu_process *server = nullptr;
    std::string server_token;
    std::string target;
    std::string existing_server_error;
    constexpr int open_attempts = 30;
    constexpr auto open_retry_delay = std::chrono::milliseconds(100);
    std::unique_lock<std::mutex> gui_server_lock(gui_framebuffer_server_mutex,
        std::defer_lock);

    auto open_framebuffer = [&](const std::string &connection, bu_process *process = nullptr,
        const std::string &auth_token = std::string(),
        const std::string &tls_pin = std::string()) {
        for (int attempt = 0; attempt < open_attempts && !framebuffer; ++attempt) {
            if (!tls_pin.empty()) {
                framebuffer = fb_open_with_auth_and_tls(connection.c_str(), width,
                    height, auth_token.c_str(), tls_pin.c_str());
            } else {
                framebuffer = auth_token.empty() ? fb_open(connection.c_str(), width, height) :
                    fb_open_with_auth(connection.c_str(), width, height, auth_token.c_str());
            }
            if (framebuffer || (process && !bu_process_alive(process))) break;
            if (attempt + 1 < open_attempts) std::this_thread::sleep_for(open_retry_delay);
        }
        return framebuffer != nullptr;
    };

    auto stop_server = [&](bool graceful = false) {
        if (!server) return;
        std::string output;
        (void)stop_process(&server,
            graceful ? FBSERV_GRACEFUL_TIMEOUT : std::chrono::milliseconds(0),
            &output);
        if (!output.empty()) job.log(output);
        job.clear_pid();
    };

    auto launch_server = [&](const std::vector<std::string> &command, const std::string &connection,
        const std::string &auth_token) {
        const bool use_tls = connection.rfind("ipc:", 0) != 0 && getenv("FBSERV_TLS");
        fs::path pin_file;
        std::string tls_pin;
        std::vector<const char *> argv;
        for (const std::string &argument : command) argv.push_back(argument.c_str());
        argv.push_back(nullptr);
        std::vector<std::string> env_storage = {"FBSERV_TOKEN=" + auth_token};
        if (use_tls) {
            pin_file = framebuffer_pin_file();
            env_storage.push_back("FBSERV_TLS_PIN_FILE=" + pin_file.string());
        }
        std::vector<const char *> env_overrides;
        for (const std::string &entry : env_storage) env_overrides.push_back(entry.c_str());
        env_overrides.push_back(nullptr);
        struct bu_process_create_info create_info = {0};
        create_info.opts = BU_PROCESS_OUT_EQ_ERR | BU_PROCESS_HIDE_WINDOW;
        create_info.env_overrides = env_overrides.data();
        bu_process_create_ex(&server, argv.data(), &create_info);
        if (!server) {
            delete_tls_pin_file(pin_file);
            throw std::runtime_error("unable to start framebuffer server");
        }
        bu_process_file_close(server, BU_PROCESS_STDIN);
        try {
            job.set_pid(bu_process_pid(server));
        } catch (...) {
            (void)stop_process(&server, std::chrono::milliseconds(0));
            throw;
        }
        if (use_tls) {
            tls_pin = wait_for_tls_pin(pin_file, server);
            delete_tls_pin_file(pin_file);
            if (tls_pin.empty()) {
                stop_server();
                return false;
            }
        }
        (void)open_framebuffer(connection, server, auth_token, tls_pin);
        if (!framebuffer) stop_server();
        return framebuffer != nullptr;
    };

    auto ensure_server_token = [&]() {
        if (server_token.empty()) server_token = framebuffer_token();
    };

    if (existing_target) {
        target = bu_vls_addr(s->fb_target);
        gui_server_lock.lock();
        bu_process *process = gui_framebuffer_server.connection == target ?
            gui_framebuffer_server.process : nullptr;
        if (!process) gui_server_lock.unlock();
        const std::string &auth_token = process ? gui_framebuffer_server.token : server_token;
        const std::string tls_pin = process ? gui_framebuffer_server.tls_pin : std::string();
        (void)open_framebuffer(target, process, auth_token, tls_pin);
        if (!framebuffer && process && !bu_process_alive(process))
            existing_server_error = gui_framebuffer_server_output();
        if (framebuffer) job.log("Using existing framebuffer " + target + ".\n");
    } else if (!tcp_requested) {
        char ipc_addr[512] = {0};
        if (pkg_ipc_addr(ipc_addr, sizeof(ipc_addr), "rtwizard-fbserv") == 0) {
            ensure_server_token();
            target = std::string("ipc:") + ipc_addr;
            std::vector<std::string> command = {executable("fbserv"), "-A", "-w", std::to_string(width),
                "-n", std::to_string(height), "-I", ipc_addr, "-F", device};
            job.log("Starting managed framebuffer server over local IPC: " + std::string(ipc_addr) + "\n");
            (void)launch_server(command, target, server_token);
        } else {
            job.log("Local framebuffer IPC is unavailable on this platform.\n");
        }
    }

    if (!framebuffer && !existing_target) {
        const int port = tcp_requested ? s->port : RTWIZARD_DEFAULT_FB_PORT;
        target = std::to_string(port);
        framebuffer = fb_open(target.c_str(), width, height);
        if (framebuffer) {
            job.log("Using existing TCP framebuffer server on port " + target + ".\n");
        } else {
            ensure_server_token();
            std::vector<std::string> command = {executable("fbserv"), "-A", "-w", std::to_string(width),
                "-n", std::to_string(height), "-p", target, "-F", device};
            job.log("Starting managed TCP framebuffer server on port " + target + ".\n");
            (void)launch_server(command, target, server_token);
        }
    }
    if (!framebuffer) {
        stop_server();
        std::string message = "unable to open framebuffer '" + target + "'";
        if (!existing_server_error.empty()) message += ": " + existing_server_error;
        throw std::runtime_error(message);
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
write_image_atomic(JobContext &job, const rtwizard_settings *s, icv_image_t *image)
{
    if (!bu_vls_strlen(s->output_file)) return;
    const fs::path output(bu_vls_addr(s->output_file));
    const StagedOutput staged(output, job.temporary_directory());
    if (icv_write(image, staged.path().string().c_str(), BU_MIME_IMAGE_AUTO) != 0) {
        throw std::runtime_error("unable to write output image '" + output.string() + "'");
    }

    if (!staged.publish()) {
        throw std::runtime_error("unable to replace output image '" + output.string() + "'");
    }
}
static icv_anim_format_t
animation_format(const char *path)
{
    const char *extension = strrchr(path, '.');
    if (!extension) return ICV_ANIM_UNKNOWN;
    if (BU_STR_EQUIV(extension, ".png") || BU_STR_EQUIV(extension, ".apng")) return ICV_ANIM_APNG;
    if (BU_STR_EQUIV(extension, ".avi") || BU_STR_EQUIV(extension, ".mjpg")) return ICV_ANIM_MJPG;
    return ICV_ANIM_UNKNOWN;
}

static std::vector<std::string>
cut_planes(const rtwizard_settings *settings, const ViewState &view, db_i *dbip, char picture_type)
{
    vect_t direction;
    if (settings->cut_direction_set) {
        VMOVE(direction, settings->cut_direction);
        VUNITIZE(direction);
    } else {
        mat_t rotation;
        quat_quat2mat(rotation, view.orientation);
        VSET(direction, -rotation[8], -rotation[9], -rotation[10]);
        VUNITIZE(direction);
    }
    vect_t reference, xaxis, yaxis;
    if (std::fabs(direction[Z]) < 0.9) VSET(reference, 0.0, 0.0, 1.0);
    else VSET(reference, 0.0, 1.0, 0.0);
    VCROSS(xaxis, reference, direction);
    VUNITIZE(xaxis);
    VCROSS(yaxis, direction, xaxis);
    VUNITIZE(yaxis);
    mat_t projection;
    MAT_IDN(projection);
    VMOVE(&projection[0], xaxis);
    VMOVE(&projection[4], yaxis);
    VMOVE(&projection[8], direction);

    std::vector<std::string> object_storage = rendered_objects(settings, picture_type);
    std::vector<const char *> objects;
    for (const std::string &object : object_storage) objects.push_back(object.c_str());
    point_t minimum, maximum;
    struct bu_vls bounds_error = BU_VLS_INIT_ZERO;
    const int bounds_status = rt_obj_bounds_mat(&bounds_error, dbip,
	static_cast<int>(objects.size()), objects.data(), 0, projection,
	RT_OBJ_BOUNDS_IGNORE_INFINITE, minimum, maximum);
    if (bounds_status != BRLCAD_OK) {
	const std::string detail = bu_vls_addr(&bounds_error);
	bu_vls_free(&bounds_error);
	throw std::runtime_error("unable to determine cutting-plane bounds" +
	    (detail.empty() ? std::string() : ": " + detail));
    }
    bu_vls_free(&bounds_error);
    const double span = maximum[Z] - minimum[Z];
    if (!(span > 0.0)) throw std::runtime_error("rendered objects have degenerate cutting-plane bounds");

    const size_t width = settings->width_set ? settings->width : (settings->size_set ? settings->size : settings->width);
    const size_t height = settings->height_set ? settings->height : (settings->size_set ? settings->size : settings->height);
    const double margin = std::max({std::fabs(span) / static_cast<double>(std::max(width, height)), std::fabs(span) * 0.05, 1.0e-9});
    std::vector<std::string> planes;
    planes.reserve(static_cast<size_t>(settings->cut_steps));
    for (int frame = 0; frame < settings->cut_steps; ++frame) {
        const double fraction = static_cast<double>(frame) / static_cast<double>(settings->cut_steps - 1);
        const double distance = minimum[Z] - margin + fraction * span;
        point_t point;
        VSCALE(point, direction, distance);
        std::ostringstream plane;
        plane << std::setprecision(17) << point[X] << ',' << point[Y] << ',' << point[Z] << ',' << direction[X] << ',' << direction[Y] << ',' << direction[Z];
        planes.push_back(plane.str());
    }
    return planes;
}

static void
render_cut_animation(JobContext &job, const rtwizard_settings *settings, const ViewState &view, db_i *dbip, char picture_type)
{
    if (!bu_vls_strlen(settings->output_file)) throw std::runtime_error("cutting-plane animation requires a file output");
    const icv_anim_format_t format = animation_format(bu_vls_addr(settings->output_file));
    if (format == ICV_ANIM_UNKNOWN) throw std::runtime_error("animation output must use .png, .apng, .avi, or .mjpg");
    const size_t width = settings->width_set ? settings->width : (settings->size_set ? settings->size : settings->width);
    const size_t height = settings->height_set ? settings->height : (settings->size_set ? settings->size : settings->height);
    std::unique_ptr<icv_anim_t, decltype(&icv_anim_destroy)> animation(icv_anim_create(format, static_cast<uint32_t>(width), static_cast<uint32_t>(height), settings->animation_fps), &icv_anim_destroy);
    if (!animation) throw std::runtime_error("unable to create output animation");

    const std::vector<std::string> planes = cut_planes(settings, view, dbip, picture_type);
    for (size_t frame = 0; frame < planes.size(); ++frame) {
        if (job.cancelled()) throw std::runtime_error("render cancelled");
        job.log("Rendering frame " + std::to_string(frame + 1) + "/" + std::to_string(planes.size()) + "\n");
        Image image = compose_frame(job, settings, view, planes[frame], picture_type);
        if (icv_anim_add_frame(animation.get(), image.get()) != 0) throw std::runtime_error("unable to add output animation frame");
        job.progress(static_cast<int>(frame + 1), static_cast<int>(planes.size()));
    }
    const fs::path output(bu_vls_addr(settings->output_file));
    const StagedOutput staged(output, job.temporary_directory());
    if (icv_anim_write(animation.get(), staged.path().string().c_str()) != 0) {
        throw std::runtime_error("unable to write output animation '" + output.string() + "'");
    }
    if (!staged.publish()) {
        throw std::runtime_error("unable to replace output animation '" + output.string() + "'");
    }
}

} // namespace

extern "C" int
rtwizard_fbserv_start(char *target, size_t target_size, int width, int height,
    const char *device, int port, char **errmsg)
{
    if (errmsg) *errmsg = nullptr;
    if (!target || !target_size || width <= 0 || height <= 0 || !device || !device[0]) {
        set_error(errmsg, "invalid framebuffer server settings");
        return BRLCAD_ERROR;
    }

    std::lock_guard<std::mutex> server_lock(gui_framebuffer_server_mutex);
    fs::path pin_file;
    try {
        std::string connection;
        std::vector<std::string> command = {executable("fbserv"), "-A", "-w",
            std::to_string(width), "-n", std::to_string(height)};

        stop_gui_framebuffer_server();
        gui_framebuffer_server.token = framebuffer_token();
        if (port >= 0) {
            connection = std::to_string(port);
            command.insert(command.end(), {"-p", connection});
        } else {
            char ipc_addr[512] = {0};
            if (pkg_ipc_addr(ipc_addr, sizeof(ipc_addr), "rtwizard-gui-fbserv") == 0) {
                connection = std::string("ipc:") + ipc_addr;
                command.insert(command.end(), {"-I", ipc_addr});
            } else {
                connection = std::to_string(RTWIZARD_DEFAULT_FB_PORT);
                command.insert(command.end(), {"-p", connection});
            }
        }
        command.insert(command.end(), {"-F", device});

        std::vector<const char *> argv;
        for (const std::string &argument : command) argv.push_back(argument.c_str());
        argv.push_back(nullptr);
        const bool use_tls = connection.rfind("ipc:", 0) != 0 && getenv("FBSERV_TLS");
        std::vector<std::string> env_storage = {
            "FBSERV_TOKEN=" + gui_framebuffer_server.token
        };
        if (use_tls) {
            pin_file = framebuffer_pin_file();
            env_storage.push_back("FBSERV_TLS_PIN_FILE=" + pin_file.string());
        }
        std::vector<const char *> env_overrides;
        for (const std::string &entry : env_storage) env_overrides.push_back(entry.c_str());
        env_overrides.push_back(nullptr);
        struct bu_process_create_info create_info = {0};
        create_info.opts = BU_PROCESS_OUT_EQ_ERR;
        create_info.env_overrides = env_overrides.data();
        bu_process_create_ex(&gui_framebuffer_server.process, argv.data(), &create_info);
        if (!gui_framebuffer_server.process) {
            gui_framebuffer_server.token.clear();
            throw std::runtime_error("unable to start framebuffer server");
        }
        bu_process_file_close(gui_framebuffer_server.process, BU_PROCESS_STDIN);
        if (use_tls) {
            gui_framebuffer_server.tls_pin = wait_for_tls_pin(pin_file,
                gui_framebuffer_server.process);
            delete_tls_pin_file(pin_file);
            pin_file.clear();
            if (gui_framebuffer_server.tls_pin.empty())
                throw std::runtime_error("unable to establish framebuffer TLS identity");
        }
        gui_framebuffer_server.connection = connection;
        if (bu_strlcpy(target, connection.c_str(), target_size) >= target_size)
            throw std::runtime_error("framebuffer target exceeds the available buffer");
        return BRLCAD_OK;
    } catch (const std::exception &error) {
        delete_tls_pin_file(pin_file);
        stop_gui_framebuffer_server();
        set_error(errmsg, error.what());
        return BRLCAD_ERROR;
    }
}

extern "C" void
rtwizard_fbserv_stop(void)
{
    std::lock_guard<std::mutex> server_lock(gui_framebuffer_server_mutex);
    stop_gui_framebuffer_server();
}

extern "C" int
rtwizard_render(const rtwizard_settings *settings, char picture_type, const rtwizard_render_callbacks *callbacks, void *callback_data, char **errmsg)
{
    if (errmsg) *errmsg = nullptr;
    if (!settings || settings->magic != RTWIZARD_MAGIC) {
        set_error(errmsg, "invalid rtwizard settings");
        return BRLCAD_ERROR;
    }
    try {
        JobContext job(callbacks, callback_data, bu_vls_addr(settings->log_file), bu_vls_addr(settings->pid_file), settings->verbose != 0);
        Database database(db_open(bu_vls_addr(settings->input_file), DB_OPEN_READONLY));
        if (!database || db_dirbuild(database.get()) < 0) throw std::runtime_error("unable to open geometry database '" + std::string(bu_vls_addr(settings->input_file)) + "'");
        ViewState view = resolve_view(settings, picture_type);
        if (settings->cut_steps >= 2) {
            render_cut_animation(job, settings, view, database.get(), picture_type);
        } else {
            Image image = compose_frame(job, settings, view, std::string(), picture_type);
            write_image_atomic(job, settings, image.get());
            (void)write_framebuffer(job, settings, image.get());
            job.progress(1, 1);
            if (callbacks && callbacks->frame && bu_vls_strlen(settings->output_file)) callbacks->frame(callback_data, bu_vls_addr(settings->output_file), 1, 1);
        }
        return BRLCAD_OK;
    } catch (const std::exception &error) {
        set_error(errmsg, error.what());
        return BRLCAD_ERROR;
    }
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
