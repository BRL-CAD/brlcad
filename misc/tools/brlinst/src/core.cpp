// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <miniz.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <mutex>
#include <fstream>
#include <set>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace installer {
namespace {
constexpr std::size_t max_parallel_workers = 8;
#ifdef _WIN32
constexpr unsigned journal_replace_attempts = 50;
constexpr auto journal_replace_delay = std::chrono::milliseconds(100);
#endif
}
std::string utf8(const fs::path &p) {
    return p.u8string();
}
fs::path path_from_utf8(const std::string &p) {
    return fs::u8path(p);
}
std::string windows_filename_key(const std::string &path) {
#ifdef _WIN32
    auto wide = path_from_utf8(path).wstring();
    CharUpperBuffW(wide.data(), static_cast<DWORD>(wide.size()));
    return utf8(fs::path(wide));
#else
    if (std::any_of(path.begin(), path.end(), [](unsigned char c) { return c >= 0x80; }))
        throw Error("Non-ASCII payload names require packaging or verification on Windows: " + path);
    std::string result = path;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
#endif
}

std::string read_text(const fs::path &path, std::size_t limit) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw Error("Cannot read " + utf8(path));
    auto length = fs::file_size(path);
    if (length > limit)
        throw Error("File exceeds size limit: " + utf8(path));
    std::string result(static_cast<std::size_t>(length), '\0');
    if (!input.read(result.data(), static_cast<std::streamsize>(result.size())))
        throw Error("Cannot read complete file: " + utf8(path));
    return result;
}

Json parse_json(const std::string &text) {
    // Reject duplicate fields: accepting different values in different consumers
    // makes configuration and recovery records ambiguous.
    std::vector<std::set<std::string>> keys;
    try {
        return Json::parse(text, [&](int depth, Json::parse_event_t event, Json &value) {
            if (depth > 64)
                throw Error("JSON nesting exceeds limit");
            if (event == Json::parse_event_t::object_start)
                keys.emplace_back();
            if (event == Json::parse_event_t::key && !keys.back().insert(value.get<std::string>()).second)
                throw Error("Duplicate JSON field: " + value.get<std::string>());
            if (event == Json::parse_event_t::object_end)
                keys.pop_back();
            return true;
        });
    } catch (const Json::exception &e) {
        throw Error(std::string("Invalid JSON: ") + e.what(), ExitCode::arguments);
    }
}

void check_no_links(const fs::path &path) {
    auto absolute = fs::absolute(path).lexically_normal();
    fs::path current;
    for (const auto &part : absolute) {
        current /= part;
#ifdef _WIN32
        DWORD attrs = GetFileAttributesW(current.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
            throw Error("Reparse point is not permitted: " + utf8(current));
        if (attrs == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND)
            throw Error("Cannot inspect destination: " + utf8(current));
#else
        std::error_code ec;
        auto status = fs::symlink_status(current, ec);
        if (ec && ec != std::errc::no_such_file_or_directory)
            throw Error("Cannot inspect " + utf8(current));
        if (fs::is_symlink(status))
            throw Error("Symbolic link is not permitted: " + utf8(current));
#endif
    }
}

void flush_file(const fs::path &path) {
#ifdef _WIN32
    HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw Error("Cannot open file for durable write: " + utf8(path));
    bool ok = FlushFileBuffers(h) != 0;
    CloseHandle(h);
#else
    int h = ::open(path.c_str(), O_RDONLY);
    if (h < 0)
        throw Error("Cannot open file for durable write: " + utf8(path));
    bool ok = ::fsync(h) == 0;
    ::close(h);
#endif
    if (!ok)
        throw Error("Cannot flush file to disk: " + utf8(path));
}
void publish_file(const fs::path &source, const fs::path &destination) {
    check_no_links(source);
    check_no_links(destination);
#ifdef _WIN32
    if (!MoveFileExW(source.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
        throw Error("Cannot publish file without replacing an existing file: " + utf8(destination));
#else
    fs::create_hard_link(source, destination);
    fs::remove(source);
#endif
}

void durable_json(const fs::path &path, const Json &data) {
    check_no_links(path);
    auto temp = path;
    temp += ".tmp";
    check_no_links(temp);
    const auto text = data.dump(2);
    if (text.size() > max_manifest_size)
        throw Error("Installation journal exceeds supported size");
#ifdef _WIN32
    HANDLE h = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
    if (h == INVALID_HANDLE_VALUE)
        throw Error("Cannot write journal: " + utf8(temp));
    DWORD written = 0;
    bool ok = WriteFile(h, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
              written == text.size() && FlushFileBuffers(h);
    CloseHandle(h);
    if (!ok)
        throw Error("Cannot commit journal: " + utf8(path));
    for (unsigned attempt = 0; attempt != journal_replace_attempts; ++attempt) {
        if (MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            return;
        auto error = GetLastError();
        if (error != ERROR_ACCESS_DENIED && error != ERROR_SHARING_VIOLATION && error != ERROR_LOCK_VIOLATION)
            break;
        std::this_thread::sleep_for(journal_replace_delay);
    }
    throw Error("Cannot commit journal: " + utf8(path));
#else
    FILE *f = std::fopen(temp.c_str(), "wb");
    if (!f)
        throw Error("Cannot write journal: " + utf8(temp));
    bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = std::fflush(f) == 0 && ok;
    ok = ::fsync(fileno(f)) == 0 && ok;
    ok = std::fclose(f) == 0 && ok;
    if (!ok)
        throw Error("Cannot flush journal: " + utf8(temp));
    fs::rename(temp, path);
    int d = ::open(path.parent_path().c_str(), O_RDONLY);
    if (d < 0)
        throw Error("Cannot open journal directory");
    int result = ::fsync(d);
    ::close(d);
    if (result != 0)
        throw Error("Cannot flush journal directory");
#endif
}

static void validate_portable_relative(const std::string &name) {
    constexpr char reverse_solidus = 0x5c;
    if (name.empty() || name.size() > 30000 || name.front() == '/' ||
        name.find(reverse_solidus) != std::string::npos)
        throw Error("Expected a relative forward-slash path: " + name, ExitCode::arguments);
    std::size_t start = 0;
    while (start <= name.size()) {
        auto end = name.find('/', start);
        auto part = name.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == "..")
            throw Error("Unsafe path: " + name, ExitCode::arguments);
        for (unsigned char c : part)
            if (c < 32)
                throw Error("Invalid path: " + name, ExitCode::arguments);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
}

void validate_relative(const std::string &name, bool internal) {
    if (name.empty() || name.size() > 30000 || name.front() == '/' || name.find('\\') != std::string::npos)
        throw Error("Expected a relative forward-slash path: " + name, ExitCode::arguments);
    std::size_t start = 0;
    while (start <= name.size()) {
        auto end = name.find('/', start);
        auto part = name.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == ".." || part.back() == ' ' || part.back() == '.')
            throw Error("Unsafe path: " + name, ExitCode::arguments);
        for (unsigned char c : part)
            if (c < 32 || std::string("<>:\"|?*").find(static_cast<char>(c)) != std::string::npos)
                throw Error("Invalid Windows path: " + name, ExitCode::arguments);
        std::string upper = part;
        std::transform(upper.begin(), upper.end(), upper.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        if (!internal && start == 0 && upper == ".BRLINST")
            throw Error("Reserved installer directory in payload", ExitCode::arguments);
        auto base = upper.substr(0, upper.find('.'));
        if ((base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") &&
            (base.substr(3) == "\xc2\xb9" || base.substr(3) == "\xc2\xb2" || base.substr(3) == "\xc2\xb3"))
            throw Error("Reserved Windows device name: " + name, ExitCode::arguments);
        if (base == "CON" || base == "PRN" || base == "AUX" || base == "NUL" || base == "CONIN$" ||
            base == "CONOUT$" ||
            ((base.substr(0, 3) == "COM" || base.substr(0, 3) == "LPT") && base.size() == 4 &&
             base[3] >= '0' && base[3] <= '9'))
            throw Error("Reserved Windows device name: " + name, ExitCode::arguments);
        if (end == std::string::npos)
            break;
        start = end + 1;
    }
}

static void fields(const Json &value, const std::set<std::string> &allowed) {
    if (!value.is_object())
        throw Error("Expected JSON object", ExitCode::arguments);
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!allowed.count(it.key()))
            throw Error("Unknown JSON field: " + it.key(), ExitCode::arguments);
}
static std::string text_field(const Json &j, const char *key) {
    if (!j.contains(key) || !j[key].is_string())
        throw Error(std::string("Missing string: ") + key, ExitCode::arguments);
    auto s = j[key].get<std::string>();
    if (s.empty() || s.size() > 65536 || s.find('\0') != std::string::npos)
        throw Error(std::string("Invalid string: ") + key, ExitCode::arguments);
    return s;
}
void validate_product(const Json &p, bool packaged) {
    fields(p, {"formatVersion", "id", "name", "version", "architecture", "publisher", "website",
               "welcome", "license", "licenseText", "icon", "branding", "shortcuts"});
    if (!p.contains("formatVersion") || p["formatVersion"] != 1)
        throw Error("Unsupported configuration formatVersion", ExitCode::arguments);
    for (auto key : {"id", "version"}) {
        auto s = text_field(p, key);
        if (s.size() > 80 || !std::all_of(s.begin(), s.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                       c == '.' || c == '-' || c == '_';
            }))
            throw Error(std::string("Use ASCII letters, digits, dot, hyphen or underscore for ") + key,
                        ExitCode::arguments);
    }
    product_architecture(p);
    auto name = text_field(p, "name");
    validate_portable_relative(name);
    if (name.find('/') != std::string::npos || name.size() > 80)
        throw Error("Product name must fit one directory component", ExitCode::arguments);
    text_field(p, "publisher");
    if (packaged)
        text_field(p, "licenseText");
    else
        text_field(p, "license");
    for (auto key : {"welcome", "icon", "website"})
        if (p.contains(key))
            text_field(p, key);
    if (p.contains("branding")) {
        const auto &branding = p["branding"];
        fields(branding, {"style", "exe", "msi"});
        if (!branding.contains("style") || branding["style"] != "classic")
            throw Error("branding.style must be classic", ExitCode::arguments);
        for (auto backend : {"exe", "msi"}) {
            if (!branding.contains(backend))
                continue;
            if (packaged)
                throw Error(std::string("Packaged product contains branding asset paths: ") + backend,
                            ExitCode::arguments);
            const auto &assets = branding[backend];
            if (backend == std::string("exe")) {
                fields(assets, {"header", "sidebar"});
                text_field(assets, "header");
                text_field(assets, "sidebar");
            } else {
                fields(assets, {"icon", "banner", "dialog", "license"});
                for (auto asset_name : {"icon", "banner", "dialog", "license"})
                    if (assets.contains(asset_name))
                        text_field(assets, asset_name);
            }
        }
    }
    if (p.contains("website")) {
        auto url = p["website"].get<std::string>();
        if (url.rfind("https://", 0) != 0)
            throw Error("Website must use https://", ExitCode::arguments);
    }
    if (!p.contains("shortcuts") || !p["shortcuts"].is_array() || p["shortcuts"].size() > 100)
        throw Error("shortcuts must be an array of at most 100 entries", ExitCode::arguments);
    std::set<std::string> names;
    for (const auto &shortcut : p["shortcuts"]) {
        fields(shortcut, {"name", "target", "icon", "arguments", "workingDirectory", "desktop"});
        auto label = text_field(shortcut, "name");
        validate_portable_relative(label);
        if (label.find('/') != std::string::npos || label.size() > 80)
            throw Error("Shortcut name must be a single component", ExitCode::arguments);
        if (!names.insert(label).second)
            throw Error("Duplicate shortcut name", ExitCode::arguments);
        validate_portable_relative(text_field(shortcut, "target"));
        if (shortcut.contains("icon"))
            validate_portable_relative(text_field(shortcut, "icon"));
        if (shortcut.contains("workingDirectory")) {
            auto dir = text_field(shortcut, "workingDirectory");
            if (dir != "@profile" && dir != ".")
                validate_portable_relative(dir);
        }
        if (shortcut.contains("arguments") &&
            (!shortcut["arguments"].is_string() ||
             shortcut["arguments"].get<std::string>().find('\0') != std::string::npos))
            throw Error("Invalid shortcut arguments", ExitCode::arguments);
        if (shortcut.contains("desktop") && !shortcut["desktop"].is_boolean())
            throw Error("desktop must be boolean", ExitCode::arguments);
    }
}
void validate_windows_product(const Json &product) {
    validate_relative(product.at("name").get<std::string>());
    std::set<std::string> names;
    for (const auto &shortcut : product.at("shortcuts")) {
        const auto label = shortcut.at("name").get<std::string>();
        validate_relative(label);
        const auto label_key = windows_filename_key(label);
        if (label_key == "UNINSTALL" || !names.insert(label_key).second)
            throw Error("Duplicate or reserved Windows shortcut name", ExitCode::arguments);
        validate_relative(shortcut.at("target").get<std::string>());
        if (shortcut.contains("icon"))
            validate_relative(shortcut.at("icon").get<std::string>());
        const auto directory = shortcut.value("workingDirectory", "@profile");
        if (directory != "@profile" && directory != ".")
            validate_relative(directory);
    }
}
Architecture product_architecture(const Json &product) {
    const auto name = text_field(product, "architecture");
    if (name == "x64")
        return Architecture::x64;
    if (name == "arm64")
        return Architecture::arm64;
    throw Error("architecture must be x64 or arm64", ExitCode::arguments);
}
std::string architecture_name(Architecture architecture) {
    return architecture == Architecture::arm64 ? "arm64" : "x64";
}
std::string instance_id(const Json &p) {
    return p.at("id").get<std::string>() + "!" + p.at("version").get<std::string>() + "!" +
           architecture_name(product_architecture(p));
}
std::string display_name(const Json &p, bool machine) {
    return p.at("name").get<std::string>() + " " + p.at("version").get<std::string>() + " (" +
           architecture_name(product_architecture(p)) + ", " +
           (machine ? "all users)" : "current user)");
}

Options parse_options(const std::vector<std::string> &args, bool removing) {
    Options options;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto &key = args[i];
        if (!seen.insert(key).second)
            throw Error("Duplicate option: " + key, ExitCode::arguments);
        if (key == "--quiet")
            options.quiet = true;
        else if (key == "--help")
            options.help = true;
        else if (key == "--recover" && !removing)
            options.recover = true;
        else if (key == "--apply" && !removing)
            options.apply = true;
        else {
            if (i + 1 == args.size())
                throw Error("Missing value for " + key, ExitCode::arguments);
            auto value = args[++i];
            if (key == "--log")
                options.log = path_from_utf8(value);
            else if (!removing && key == "--directory")
                options.directory = path_from_utf8(value);
            else if (!removing && key == "--scope") {
                if (value != "user" && value != "machine")
                    throw Error("scope must be user or machine", ExitCode::arguments);
                options.machine = value == "machine";
            } else if (!removing && (key == "--start-menu" || key == "--desktop" || key == "--path")) {
                if (value != "on" && value != "off")
                    throw Error(key + " must be on or off", ExitCode::arguments);
                if (key == "--start-menu")
                    options.start_menu = value == "on";
                if (key == "--desktop")
                    options.desktop = value == "on";
                if (key == "--path")
                    options.path = value == "on";
            } else
                throw Error("Unknown option: " + key, ExitCode::arguments);
        }
    }
    return options;
}
std::string option_help(bool removing) {
    return std::string(removing ? "uninstall.exe" : "setup.exe") + " [--quiet] [--log FILE] [--help]" +
           (removing ? ""
                     : "\n[--scope user|machine] [--directory PATH] [--recover]\n[--start-menu on|off] "
                       "[--desktop on|off] [--path on|off]") +
           "\nExit codes: 0 success, 2 arguments, 3 existing installation, 4 elevation required, 5 "
           "cancelled, 6 failure.\n";
}
Fingerprint fingerprint(const fs::path &path) {
    check_no_links(path);
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw Error("Cannot read " + utf8(path));
    Fingerprint result{0, 0};
    std::array<char, buffer_size> buf{};
    while (in) {
        in.read(buf.data(), buf.size());
        auto n = static_cast<std::size_t>(in.gcount());
        result.size += n;
        result.crc = static_cast<std::uint32_t>(
            mz_crc32(result.crc, reinterpret_cast<const unsigned char *>(buf.data()), n));
    }
    if (!in.eof())
        throw Error("Read failed: " + utf8(path));
    return result;
}
void check_cancel(const Cancel &cancel) {
    if (cancel && cancel())
        throw Error("Operation cancelled", ExitCode::cancelled);
}

std::size_t parallel_worker_count(std::size_t task_count) {
    if (!task_count)
        return 0;
    const auto available = std::max(1u, std::thread::hardware_concurrency());
    return std::min(task_count, std::min(max_parallel_workers, static_cast<std::size_t>(available)));
}

void parallel_for(std::size_t task_count, const std::string &stage, const Progress &progress,
                  const std::function<void(std::size_t)> &action) {
    if (progress)
        progress(stage, 0, task_count);
    const auto worker_count = parallel_worker_count(task_count);
    if (!worker_count)
        return;
    std::atomic_size_t next{0}, completed{0};
    std::atomic_bool stop{false};
    std::exception_ptr failure;
    std::mutex failure_mutex, progress_mutex;
    auto worker = [&] {
        while (!stop.load(std::memory_order_relaxed)) {
            const auto index = next.fetch_add(1, std::memory_order_relaxed);
            if (index >= task_count)
                return;
            try {
                action(index);
                const auto done = completed.fetch_add(1, std::memory_order_relaxed) + 1;
                if (progress) {
                    std::lock_guard<std::mutex> lock(progress_mutex);
                    progress(stage, done, task_count);
                }
            } catch (...) {
                stop.store(true, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(failure_mutex);
                if (!failure)
                    failure = std::current_exception();
            }
        }
    };
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i)
        workers.emplace_back(worker);
    for (auto &thread : workers)
        thread.join();
    if (failure)
        std::rethrow_exception(failure);
}
} // namespace installer
