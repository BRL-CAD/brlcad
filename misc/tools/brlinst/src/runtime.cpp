// SPDX-License-Identifier: BSD-2-Clause
#include "installer/windows.hpp"
#include <shellapi.h>
#include <iostream>
#include <memory>

using namespace installer;
namespace {
void diagnostic(const std::string &message, bool quiet, HWND owner = nullptr) {
    if (quiet) {
        // A GUI-subsystem executable can still write inherited redirected handles.
        HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
        DWORD written = 0;
        if (h && h != INVALID_HANDLE_VALUE) {
            auto line = message + "\r\n";
            WriteFile(h, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
        }
    } else
        MessageBoxW(owner, wide(message).c_str(), L"brlinst", MB_OK | MB_ICONERROR);
}
void completion_log(Log &log, const std::string &message) noexcept {
    try {
        log.write(message);
    } catch (...) {
        // The filesystem operation is already committed; logging cannot change its result.
    }
}
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_SYSTEM32);
    SetDllDirectoryW(L"");
    int argc = 0;
    auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(narrow(argv[i]));
    LocalFree(argv);
    bool quiet = std::find(args.begin(), args.end(), "--quiet") != args.end() ||
                 std::find(args.begin(), args.end(), "--cleanup") != args.end();
    std::unique_ptr<Log> log;
    std::unique_ptr<Wizard> wizard;
#ifdef BRLINST_UNINSTALL
    constexpr bool removing = true;
#else
    constexpr bool removing = false;
#endif
    HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct EndCom {
        HRESULT result;
        ~EndCom() {
            if (SUCCEEDED(result))
                CoUninitialize();
        }
    } end_com{com};
    try {
        require_supported_windows();
#ifdef BRLINST_UNINSTALL
        if (args.size() == 10 && args[0] == "--cleanup" && args[2] == "--parent" && args[4] == "--log" &&
            args[6] == "--scope" && (args[7] == "user" || args[7] == "machine") &&
            args[8] == "--remove-root" && (args[9] == "on" || args[9] == "off")) {
            return cleanup_process(path_from_utf8(args[1]), static_cast<DWORD>(std::stoul(args[3])),
                                   path_from_utf8(args[5]), args[7] == "machine", args[9] == "on");
        }
#endif
        auto options = parse_options(args, removing);
        if (options.help) {
            if (quiet)
                diagnostic(option_help(removing), true);
            else
                MessageBoxW(nullptr, wide(option_help(removing)).c_str(), L"brlinst command line", MB_OK);
            return 0;
        }
        log = std::make_unique<Log>(options.log);
        log->write(removing ? "Uninstall started" : "Install started");
        auto executable = module_path();
        HANDLE image_handle = CreateFileW(executable.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        win_check(image_handle != INVALID_HANDLE_VALUE, "Cannot lock the running installer image");
        struct ImageLock {
            HANDLE handle;
            ~ImageLock() {
                CloseHandle(handle);
            }
        } image_lock{image_handle};
        // Keep the source executable at this path unchanged while validation
        // and extraction reopen its embedded payload.

        Json product;
        Package package;
        bool cleanup_remove_root = false;
        if (removing) {
            options.directory = executable.parent_path().parent_path();
            if (executable.parent_path().filename() != state_directory ||
                executable.filename() != uninstall_filename)
                throw Error("Run the uninstaller from its installed .brlinst directory");
            auto state = load_state(options.directory);
            product = state.at("product");
            options.machine = state.at("machine");
            cleanup_remove_root = state.at("rootCreated");
            validate_destination(options.directory, options.machine);
        } else {
            package = open_package(executable);
            product = package.product;
            if (options.directory.empty())
                options.directory = default_directory(product, options.machine);
        }
        const bool operation_removing = removing || options.recover;
        if (options.recover) {
            if (std::find(args.begin(), args.end(), "--directory") == args.end())
                throw Error("Recovery requires --directory", ExitCode::arguments);
            auto state = load_state(options.directory);
            if (instance_id(state.at("product")) != instance_id(product))
                throw Error("This installer does not match the interrupted installation");
            if (state.at("phase") == "installed")
                throw Error("Use the installed uninstaller to remove a completed installation");
            options.machine = state.at("machine");
            validate_destination(options.directory, options.machine);
        }
        if (operation_removing && options.machine && !elevated()) {
            if (quiet)
                throw Error("Run machine-wide quiet uninstall from an elevated process", ExitCode::elevation);
            return removing ? elevate_uninstall(options) : elevate_install(options);
        }
        if (!quiet) {
            wizard = std::make_unique<Wizard>(product, options, operation_removing);
            if (!options.apply && !wizard->configure()) {
                log->write("Cancelled before changes");
                return static_cast<int>(ExitCode::cancelled);
            }
        }
        if (!operation_removing) {
            validate_destination(options.directory, options.machine);
            if (options.machine && !elevated()) {
                if (quiet)
                    throw Error("Run machine-wide quiet installation from an elevated process",
                                ExitCode::elevation);
                wizard.reset();
                return elevate_install(options);
            }
        }
        OperationLock lock(options.machine);
        if (!operation_removing)
            check_registered(product, options.machine);
        WindowsIntegration integration;
        auto progress = [&](const std::string &message, std::uint64_t done, std::uint64_t total) {
            log->write(message);
            if (wizard)
                wizard->progress(message, done, total);
        };
        auto cancel = [&] { return wizard && wizard->cancelled(); };
        std::vector<std::string> retained;
        bool cleanup_scheduled = false;
        for (;;) {
            try {
                if (operation_removing) {
                    retained = uninstall(options.directory, integration, progress, removing);
                    if (removing) {
                        const auto uninstaller = std::string(state_directory) + "/" + uninstall_filename;
                        if (std::find(retained.begin(), retained.end(), uninstaller) == retained.end()) {
                            schedule_cleanup(options.directory, log->path, options.machine, cleanup_remove_root);
                            cleanup_scheduled = true;
                        }
                        finalize_deferred_uninstall(options.directory);
                    }
                } else
                    install(package, options, integration, progress, cancel);
                break;
            } catch (const Error &e) {
                log->write(std::string("Operation error: ") + e.what());
                if (quiet || e.code == ExitCode::cancelled)
                    throw;
                auto message = wide(std::string(e.what()) +
                                    "\n\nClose applications using this installation, then Retry. Cancel "
                                    "leaves recovery information if cleanup could not finish.");
                if (MessageBoxW(wizard->window(), message.c_str(), L"brlinst",
                                MB_RETRYCANCEL | MB_ICONERROR) != IDRETRY)
                    throw;
            } catch (const fs::filesystem_error &e) {
                log->write(std::string("Filesystem error: ") + e.what());
                if (quiet)
                    throw;
                auto message = wide(std::string(e.what()) +
                                    "\n\nClose applications using this installation, then Retry.");
                if (MessageBoxW(wizard->window(), message.c_str(), L"brlinst",
                                MB_RETRYCANCEL | MB_ICONERROR) != IDRETRY)
                    throw;
            }
        }
        std::string result =
            operation_removing ? "This version was removed." : "Installation completed successfully.";
        if (cleanup_scheduled)
            result += " Its uninstaller will be removed when this window/process closes.";
        if (!retained.empty()) {
            result += "\r\n\r\nModified files were preserved:\r\n";
            for (const auto &path : retained) {
                result += path + "\r\n";
                completion_log(*log, "Preserved modified file: " + path);
            }
        }
        result += "\r\n\r\nLog: " + utf8(log->path);
        completion_log(*log, "Operation completed");
        if (wizard)
            wizard->finish(result);
        return 0;
    } catch (const std::exception &e) {
        const auto error = dynamic_cast<const Error *>(&e);
        int code = error ? static_cast<int>(error->code) : static_cast<int>(ExitCode::failure);
        std::string message = e.what();
        try {
            if (!log)
                log = std::make_unique<Log>(fs::path{});
            log->write("ERROR: " + message);
            message += "\nLog: " + utf8(log->path);
        } catch (...) {
            message += "\nAn operation log could not be written.";
        }
        diagnostic(message, quiet, wizard ? wizard->window() : nullptr);
        return code;
    }
}
