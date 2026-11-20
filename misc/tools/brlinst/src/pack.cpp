// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#ifdef BRLINST_WITH_MSI
#include "installer/msi.hpp"
#endif
#include <iostream>
#include <chrono>
#include <cctype>
#include <map>
#ifdef _WIN32
#include <windows.h>
#endif
using namespace installer;
namespace {
constexpr auto progress_interval = std::chrono::seconds(2);
class ConsoleProgress {
  public:
    void operator()(const std::string &stage, std::uint64_t done, std::uint64_t total) {
        const auto now = std::chrono::steady_clock::now();
        const bool changed = stage != stage_;
        const bool complete = total && done == total;
        if (!changed && !complete && now - last_ < progress_interval)
            return;
        std::cerr << '[' << std::chrono::duration_cast<std::chrono::seconds>(now - started_).count() << "s] "
                  << stage;
        if (total)
            std::cerr << ": " << done << '/' << total << " ("
                      << static_cast<unsigned>(static_cast<long double>(done) * 100 / total) << "%)";
        else if (done)
            std::cerr << ": " << done;
        std::cerr << '\n';
        stage_ = stage;
        last_ = now;
    }

  private:
    std::chrono::steady_clock::time_point started_ = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point last_ = started_ - progress_interval;
    std::string stage_;
};
} // namespace
int run(const std::vector<std::string> &args, fs::path directory) {
    try {
        if (args.empty() || args.front() == "--help") {
            std::cout << "brlinst --config FILE --source DIR --output FILE [--format exe|msi] [--stub FILE] "
                         "[--uninstaller FILE]\n"
                         "brlinst --verify FILE\n";
            return 0;
        }
        if (args.size() == 2 && args[0] == "--verify") {
            auto path = path_from_utf8(args[1]);
            auto extension = utf8(path.extension());
            for (auto &c : extension)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (extension == ".msi") {
#ifdef BRLINST_WITH_MSI
                ConsoleProgress telemetry;
                msi::verify(path, std::ref(telemetry));
                std::cout << "Verified MSI storage and table constraints (not an installation or ICE test)\n";
                return 0;
#else
                throw Error("MSI verification requires a Windows build with BRLINST_WITH_MSI",
                            ExitCode::arguments);
#endif
            }
            auto package = open_package(path);
            verify_package(package);
            std::cout << "Verified " << package.files.size() << " files for "
                      << display_name(package.product, false) << '\n';
            return 0;
        }
        std::map<std::string, fs::path> options;
        for (std::size_t i = 0; i < args.size(); i += 2) {
            if (i + 1 == args.size() ||
                (args[i] != "--config" && args[i] != "--source" && args[i] != "--output" &&
                 args[i] != "--stub" && args[i] != "--uninstaller" && args[i] != "--format"))
                throw Error("Invalid packager argument: " + args[i], ExitCode::arguments);
            if (!options.emplace(args[i], path_from_utf8(args[i + 1])).second)
                throw Error("Duplicate option", ExitCode::arguments);
        }
        for (auto key : {"--config", "--source", "--output"})
            if (!options.count(key))
                throw Error(std::string("Missing ") + key, ExitCode::arguments);
        const auto format = options.count("--format") ? utf8(options["--format"]) : "exe";
        if (format != "exe" && format != "msi")
            throw Error("format must be exe or msi", ExitCode::arguments);
        if (format == "msi") {
            if (options.count("--stub") || options.count("--uninstaller"))
                throw Error("MSI uses Windows Installer and does not take EXE runtime options",
                            ExitCode::arguments);
#ifdef BRLINST_WITH_MSI
            ConsoleProgress telemetry;
            msi::create(options["--config"], options["--source"], options["--output"],
                        directory / "brlinst-msi-actions.dll", std::ref(telemetry));
            std::cout << "Created " << utf8(options["--output"]) << '\n';
            return 0;
#else
            throw Error("MSI generation requires running a Windows build with BRLINST_WITH_MSI",
                        ExitCode::arguments);
#endif
        }
        if (!options.count("--stub"))
            options["--stub"] = directory / "brlinst-stub.exe";
        if (!options.count("--uninstaller"))
            options["--uninstaller"] = directory / "brlinst-uninstall.exe";
        ConsoleProgress telemetry;
        create_package(options["--config"], options["--source"], options["--stub"], options["--uninstaller"],
                       options["--output"], std::ref(telemetry));
        std::cout << "Created " << utf8(options["--output"]) << '\n';
        return 0;
    } catch (const Error &e) {
        std::cerr << e.what() << '\n';
        return static_cast<int>(e.code);
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return static_cast<int>(ExitCode::failure);
    }
}
#ifdef _WIN32
int wmain(int argc, wchar_t **argv) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i)
        args.push_back(utf8(fs::path(argv[i])));
    std::wstring module(32768, L'\0');
    auto n = GetModuleFileNameW(nullptr, module.data(), static_cast<DWORD>(module.size()));
    module.resize(n);
    return run(args, fs::path(module).parent_path());
}
#else
int main(int argc, char **argv) {
    return run(std::vector<std::string>(argv + 1, argv + argc), fs::absolute(argv[0]).parent_path());
}
#endif
