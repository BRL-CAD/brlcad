// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include <json.hpp>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
namespace installer {
namespace fs = std::filesystem;
using Json = nlohmann::json;
constexpr std::size_t buffer_size = 64 * 1024;
constexpr std::size_t max_manifest_size = 16 * 1024 * 1024;
constexpr std::uint64_t max_package_size = 0xff000000ULL;
constexpr const char *state_directory = ".brlinst";
constexpr const char *state_filename = "state.json";
constexpr const char *notices_filename = "NOTICE.txt";
constexpr const char *uninstall_filename = "uninstall.exe";
enum class ExitCode : int {
    success = 0,
    arguments = 2,
    existing = 3,
    elevation = 4,
    cancelled = 5,
    failure = 6
};
enum class Architecture { x64, arm64 };
struct Error : std::runtime_error {
    ExitCode code;
    explicit Error(const std::string &message, ExitCode c = ExitCode::failure)
        : std::runtime_error(message), code(c) {}
};
struct Options {
    bool quiet = false, machine = false, start_menu = true, desktop = false, path = false;
    bool apply = false, help = false, recover = false;
    fs::path directory, log;
};
struct FileEntry {
    std::string path;
    std::uint64_t size = 0, compressed_size = 0, offset = 0;
    std::uint32_t crc = 0;
};
struct Package {
    fs::path executable;
    Json product;
    std::vector<FileEntry> files;
    std::uint64_t payload_offset = 0;
};
struct ProductInput {
    Json product;
    fs::path icon;
};
// Validated input shared by the Windows output formats. Internal runtime files
// are supplied by each backend, never by the staged application.
struct WindowsPackageInput : ProductInput {
    struct Branding {
        fs::path exe_header, exe_sidebar, msi_icon, msi_banner, msi_dialog, msi_license;
    } branding;
    std::vector<std::pair<std::string, fs::path>> files;
};
ProductInput prepare_product(const fs::path &config);
WindowsPackageInput prepare_windows_input(const fs::path &config, const fs::path &source,
                                          const fs::path &output);
void make_bitmap_canvas(const fs::path &source, const fs::path &destination,
                        std::uint32_t width, std::uint32_t height);
struct RtfOptions {
    std::string font_family;
    unsigned font_size_half_points;
    std::size_t size_limit;
};
std::string text_rtf(const std::string &text, const RtfOptions &options);
std::string license_notices();
struct Fingerprint {
    std::uint64_t size;
    std::uint32_t crc;
};
using Progress = std::function<void(const std::string &, std::uint64_t, std::uint64_t)>;
using Cancel = std::function<bool()>;
std::string utf8(const fs::path &path);
fs::path path_from_utf8(const std::string &path);
std::string windows_filename_key(const std::string &path);
std::string read_text(const fs::path &path, std::size_t limit = max_manifest_size);
Json parse_json(const std::string &text);
void flush_file(const fs::path &path);
void publish_file(const fs::path &source, const fs::path &destination);
void durable_json(const fs::path &path, const Json &data);
void validate_relative(const std::string &path, bool internal = false);
void validate_product(const Json &product, bool packaged = false);
void validate_windows_product(const Json &product);
Architecture product_architecture(const Json &product);
std::string architecture_name(Architecture architecture);
void check_no_links(const fs::path &path);
std::string instance_id(const Json &product);
std::string display_name(const Json &product, bool machine);
Options parse_options(const std::vector<std::string> &args, bool uninstall = false);
std::string option_help(bool uninstall);
Fingerprint fingerprint(const fs::path &path);
void check_cancel(const Cancel &cancel);
std::size_t parallel_worker_count(std::size_t task_count);
void parallel_for(std::size_t task_count, const std::string &stage, const Progress &progress,
                  const std::function<void(std::size_t)> &action);
Package open_package(const fs::path &executable);
void verify_package(const Package &package, const Progress &progress = {}, const Cancel &cancel = {});
void extract_file(const Package &package, const FileEntry &entry, const fs::path &output,
                  const Cancel &cancel = {});
void create_package(const fs::path &config, const fs::path &source, const fs::path &stub,
                    const fs::path &uninstaller, const fs::path &output, const Progress &progress = {});
struct Integration {
    virtual ~Integration() = default;
    virtual void install(Json &state, const fs::path &journal) = 0;
    virtual void remove(Json &state, const fs::path &journal) = 0;
};
Json load_state(const fs::path &root);
void install(const Package &package, const Options &options, Integration &integration,
             const Progress &progress = {}, const Cancel &cancel = {});
std::vector<std::string> uninstall(const fs::path &root, Integration &integration,
                                   const Progress &progress = {}, bool defer_uninstaller = false);
void finalize_deferred_uninstall(const fs::path &root);
#ifdef _WIN32
void update_resources(const fs::path &executable, const Json &product, const fs::path &icon,
                      const WindowsPackageInput::Branding &branding);
#endif
} // namespace installer
