// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace installer {
namespace {
fs::path journal_path(const fs::path &root) {
    return root / state_directory / state_filename;
}
void create_parent(const fs::path &root, const fs::path &relative) {
    fs::path current;
    for (const auto &component : relative.parent_path()) {
        current /= component;
        auto path = root / current;
        check_no_links(path);
        if (fs::exists(path)) {
            if (!fs::is_directory(path))
                throw Error("Expected directory: " + utf8(path));
            continue;
        }
        fs::create_directory(path);
    }
}
bool remove_empty(const fs::path &p) {
    check_no_links(p);
    if (!fs::exists(p))
        return true;
    if (fs::is_directory(p) && fs::is_empty(p))
        return fs::remove(p);
    return false;
}
} // namespace
Json load_state(const fs::path &root) {
    check_no_links(root);
    auto state = parse_json(read_text(journal_path(root)));
    const auto recorded_root = path_from_utf8(state.at("root")).lexically_normal();
    const auto actual_root = fs::absolute(root).lexically_normal();
#ifdef _WIN32
    const bool same_root =
        CompareStringOrdinal(recorded_root.c_str(), -1, actual_root.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    const bool same_root = recorded_root == actual_root;
#endif
    if (state.at("formatVersion") != 1 || !same_root)
        throw Error("Installation record does not match this directory");
    validate_product(state.at("product"), true);
    validate_windows_product(state.at("product"));
    auto phase = state.at("phase").get<std::string>();
    if (phase != "installing" && phase != "installed" && phase != "removing" && phase != "cleanup")
        throw Error("Unknown installation state");
    if (!state.at("machine").is_boolean() || !state.at("rootCreated").is_boolean())
        throw Error("Invalid installation scope");
    std::set<std::string> seen;
    for (const auto &entry : state.at("files")) {
        auto name = entry.at("path").get<std::string>();
        validate_relative(name, true);
        if (name.rfind(std::string(state_directory) + "/", 0) == 0 &&
            name != std::string(state_directory) + "/" + uninstall_filename &&
            name != std::string(state_directory) + "/" + notices_filename)
            throw Error("Unexpected internal file in installation record");
        if (name.rfind(std::string(state_directory) + "/", 0) != 0)
            validate_relative(name);
        if (!seen.insert(name).second || !entry.at("size").is_number_unsigned() ||
            !entry.at("crc").is_number_unsigned())
            throw Error("Invalid recorded file");
    }
    for (const auto &dir : state.at("directories"))
        validate_relative(dir.get<std::string>());
    return state;
}

void install(const Package &package, const Options &options, Integration &integration,
             const Progress &progress, const Cancel &cancel) {
    if (options.directory.empty() || !options.directory.is_absolute())
        throw Error("Choose an absolute installation directory", ExitCode::arguments);
    const auto root = options.directory.lexically_normal();
    check_no_links(root);
    if (root == root.root_path())
        throw Error("Cannot install into a filesystem root", ExitCode::arguments);
    if (!fs::is_directory(root.parent_path()))
        throw Error("The destination's parent directory must exist", ExitCode::arguments);
    if (fs::exists(root) && (!fs::is_directory(root) || !fs::is_empty(root))) {
        std::string message = "Destination must be new or empty: " + utf8(root);
        if (fs::exists(journal_path(root)))
            message +=
                ". Run its .brlinst/uninstall.exe before reinstalling (also recovers interrupted installs).";
        throw Error(message, ExitCode::existing);
    }
    std::uint64_t total = 0;
    Json files = Json::array();
    std::set<std::string> directories;
    // Declare every owned path before extraction. Recovery can then skip paths
    // not yet created without rewriting an increasingly large journal per file.
    for (const auto &entry : package.files) {
        total += entry.size;
        files.push_back({{"path", entry.path}, {"size", entry.size}, {"crc", entry.crc}});
        fs::path current;
        for (const auto &component : path_from_utf8(entry.path).parent_path()) {
            current /= component;
            if (current != state_directory)
                directories.insert(current.generic_u8string());
        }
    }
    // Extraction uses one temporary file, so reserve space for the payload and
    // the journal; there is no second complete uncompressed staging tree.
    if (fs::space(root.parent_path()).available < total + max_manifest_size)
        throw Error("Not enough free disk space");
    bool root_created = false;
    auto journal = journal_path(root);
    Json state{{"formatVersion", 1},
               {"root", utf8(root)},
               {"product", package.product},
               {"machine", options.machine},
               {"rootCreated", false},
               {"phase", "installing"},
               {"options",
                {{"startMenu", options.start_menu}, {"desktop", options.desktop}, {"path", options.path}}},
               {"files", std::move(files)},
               {"directories", directories},
               {"integration", Json::object()}};
    try {
        if (!fs::exists(root))
            root_created = fs::create_directory(root);
        state["rootCreated"] = root_created;
        check_no_links(root);
        fs::create_directory(root / state_directory);
        durable_json(journal, state);
        std::uint64_t done = 0;
        for (const auto &entry : package.files) {
            check_cancel(cancel);
            if (progress)
                progress("Installing " + entry.path, done, total);
            auto relative = path_from_utf8(entry.path);
            create_parent(root, relative);
            auto temp = root / state_directory / "partial";
            check_no_links(temp);
            extract_file(package, entry, temp, cancel);
            auto target = root / relative;
            check_no_links(target);
            if (fs::exists(target))
                throw Error("Destination file appeared during installation");
            publish_file(temp, target);
            done += entry.size;
        }
        check_cancel(cancel);
        if (progress)
            progress("Creating shortcuts and registration", total, total);
        integration.install(state, journal);
        state["phase"] = "installed";
        durable_json(journal, state);
    } catch (const std::exception &original) {
        const std::string reason = original.what();
        try {
            if (fs::exists(journal)) {
                uninstall(root, integration, progress);
            } else {
                auto temporary = journal;
                temporary += ".tmp";
                check_no_links(temporary);
                if (fs::exists(temporary))
                    fs::remove(temporary);
                remove_empty(root / state_directory);
                if (root_created)
                    remove_empty(root);
            }
        } catch (const std::exception &cleanup) {
            throw Error(reason + "\nCleanup incomplete: " + cleanup.what() +
                        "\nRun the installed uninstaller to retry recovery.");
        }
        throw;
    }
}

std::vector<std::string> uninstall(const fs::path &root, Integration &integration, const Progress &progress,
                                   bool defer_uninstaller) {
    auto state = load_state(root);
    auto journal = journal_path(root);
    state["phase"] = "removing";
    durable_json(journal, state);
    // External objects are removed before payload files, so the uninstaller and
    // journal remain available if a registry or shortcut operation fails.
    integration.remove(state, journal);
    std::vector<std::string> retained;
    std::uint64_t done = 0;
    auto files = state["files"].get<std::vector<Json>>();
    const std::string uninstaller = std::string(state_directory) + "/" + uninstall_filename;
    std::stable_partition(files.begin(), files.end(),
                          [&](const Json &e) { return e.at("path") != uninstaller; });
    for (const auto &entry : files) {
        auto name = entry.at("path").get<std::string>();
        if (progress)
            progress("Removing " + name, done++, state["files"].size());
        auto file = root / path_from_utf8(name);
        check_no_links(file);
        if (!fs::exists(file))
            continue;
        if (!fs::is_regular_file(file)) {
            retained.push_back(name);
            continue;
        }
        auto actual = fingerprint(file);
        if (defer_uninstaller && name == uninstaller) {
            if (actual.size != entry.at("size").get<std::uint64_t>() ||
                actual.crc != entry.at("crc").get<std::uint32_t>())
                retained.push_back(name);
            continue;
        }
        if (actual.size == entry.at("size").get<std::uint64_t>() &&
            actual.crc == entry.at("crc").get<std::uint32_t>()) {
            if (!fs::remove(file))
                throw Error("Cannot remove " + utf8(file));
        } else
            retained.push_back(name);
    }
    auto partial = root / state_directory / "partial";
    check_no_links(partial);
    if (fs::exists(partial))
        fs::remove(partial);
    auto dirs = state["directories"].get<std::vector<std::string>>();
    std::sort(dirs.begin(), dirs.end(),
              [](const std::string &a, const std::string &b) { return a.size() > b.size(); });
    for (const auto &dir : dirs)
        remove_empty(root / path_from_utf8(dir));
    if (defer_uninstaller) {
        state["phase"] = "cleanup";
        durable_json(journal, state);
    } else {
        fs::remove(journal);
        auto tmp = journal;
        tmp += ".tmp";
        check_no_links(tmp);
        if (fs::exists(tmp))
            fs::remove(tmp);
        remove_empty(root / state_directory);
        if (state["rootCreated"].get<bool>())
            remove_empty(root);
    }
    return retained;
}
void finalize_deferred_uninstall(const fs::path &root) {
    auto state = load_state(root);
    if (state.at("phase") != "cleanup")
        throw Error("Installation is not ready for deferred cleanup");
    auto journal = journal_path(root);
    if (!fs::remove(journal))
        throw Error("Cannot remove installation journal");
    auto temp = journal;
    temp += ".tmp";
    check_no_links(temp);
    if (fs::exists(temp) && !fs::remove(temp))
        throw Error("Cannot remove temporary installation journal");
}
} // namespace installer
