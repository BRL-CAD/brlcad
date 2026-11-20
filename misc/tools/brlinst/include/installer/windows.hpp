// SPDX-License-Identifier: BSD-2-Clause
#pragma once
#include "installer/core.hpp"
#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <memory>
namespace installer {
std::wstring wide(const std::string &text);
std::string narrow(const std::wstring &text);
void win_check(bool ok, const std::string &operation);
fs::path module_path();
fs::path known_directory(REFKNOWNFOLDERID id);
bool elevated();
void require_supported_windows();
fs::path default_directory(const Json &product, bool machine);
void validate_destination(const fs::path &path, bool machine);
std::wstring quote_argument(const std::wstring &text);
int elevate_install(const Options &options);
int elevate_uninstall(const Options &options);
void check_registered(const Json &product, bool machine);
class Log {
    std::ofstream file;

  public:
    fs::path path;
    explicit Log(const fs::path &requested);
    void write(const std::string &message);
};
class WindowsIntegration : public Integration {
  public:
    void install(Json &state, const fs::path &journal) override;
    void remove(Json &state, const fs::path &journal) override;
};
class OperationLock {
    HANDLE handle = nullptr;

  public:
    explicit OperationLock(bool machine);
    ~OperationLock();
    OperationLock(const OperationLock &) = delete;
    OperationLock &operator=(const OperationLock &) = delete;
};
void schedule_cleanup(const fs::path &root, const fs::path &log, bool machine, bool remove_root);
int cleanup_process(const fs::path &root, DWORD parent_pid, const fs::path &log, bool machine,
                    bool remove_root);

class Wizard {
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    Wizard(const Json &product, Options &options, bool removing);
    ~Wizard();
    bool configure();
    void progress(const std::string &message, std::uint64_t done, std::uint64_t total);
    bool cancelled();
    void finish(const std::string &message);
    HWND window() const;
};
} // namespace installer
