// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#ifdef _WIN32
#  include <windows.h>
#endif
using namespace installer;
namespace {
constexpr std::uint16_t pe_machine_x64 = 0x8664;
constexpr std::uint16_t pe_machine_arm64 = 0xaa64;
constexpr std::size_t bitmap_header_size = 54;
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
constexpr const char *native_test_architecture = "arm64";
#else
constexpr const char *native_test_architecture = "x64";
#endif
void require(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
template <class F> void fails(F action, const char *message) {
    bool failed = false;
    try {
        action();
    } catch (const std::exception &) {
        failed = true;
    }
    require(failed, message);
}
void write(const fs::path &p, const std::string &data) {
    std::ofstream f(p, std::ios::binary);
    f << data;
    if (!f)
        throw std::runtime_error("test write failed");
}
void put(std::string &s, std::size_t at, std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
        s.at(at + i) = static_cast<char>(value >> (i * 8));
}
std::string stub(std::uint16_t machine = pe_machine_x64) {
    std::string s(512, '\0');
    put(s, 0, 0x5a4d, 2);
    put(s, 0x3c, 64, 4);
    put(s, 64, 0x4550, 4);
    put(s, 68, machine, 2);
    put(s, 88, 0x20b, 2);
    put(s, 88 + 108, 16, 4);
    return s;
}
std::string bitmap(std::uint32_t width, std::uint32_t height) {
    const auto row_size = (static_cast<std::size_t>(width) * 3 + 3) & ~std::size_t{3};
    std::string data(bitmap_header_size + row_size * height, '\0');
    data[0] = 'B';
    data[1] = 'M';
    put(data, 2, data.size(), 4);
    put(data, 10, bitmap_header_size, 4);
    put(data, 14, 40, 4);
    put(data, 18, width, 4);
    put(data, 22, height, 4);
    put(data, 26, 1, 2);
    put(data, 28, 24, 2);
    put(data, 34, row_size * height, 4);
    return data;
}
struct Backend : Integration {
    bool fail = false, fail_remove = false;
    void install(Json &s, const fs::path &journal) override {
        s["integration"]["test"] = true;
        durable_json(journal, s);
        if (fail)
            throw Error("simulated registration failure");
    }
    void remove(Json &s, const fs::path &journal) override {
        if (fail_remove)
            throw Error("simulated removal failure");
        s["integration"] = Json::object();
        durable_json(journal, s);
    }
};
} // namespace
int main(int argc, char **argv) {
    if (argc == 3 && std::string(argv[1]) == "--fixture-stubs") {
        const auto destination = path_from_utf8(argv[2]);
        fs::create_directories(destination);
#ifdef _WIN32
        const auto binaries = fs::absolute(argv[0]).parent_path();
        fs::copy_file(binaries / "brlinst-stub.exe", destination / "brlinst-stub.exe");
        fs::copy_file(binaries / "brlinst-uninstall.exe", destination / "brlinst-uninstall.exe");
#else
        write(destination / "brlinst-stub.exe", stub());
        write(destination / "brlinst-uninstall.exe", stub());
#endif
        return 0;
    }
    auto root = fs::temp_directory_path() / ("installer-test-" + std::to_string(std::random_device{}()));
    fs::create_directories(root / "payload" / "bin");
    try {
        write(root / "small.bmp", bitmap(2, 3));
        make_bitmap_canvas(root / "small.bmp", root / "canvas.bmp", 4, 2);
        const auto canvas = read_text(root / "canvas.bmp");
        require(canvas.size() == 78, "Derived bitmap has the wrong size");
        require(static_cast<unsigned char>(canvas[18]) == 4 &&
                    static_cast<unsigned char>(canvas[22]) == 2,
                "Derived bitmap has the wrong dimensions");
        require(canvas[bitmap_header_size] == '\0' &&
                    static_cast<unsigned char>(canvas[bitmap_header_size + 6]) == 0xff,
                "Derived bitmap did not preserve artwork on a white canvas");
        for (auto path : {"../x", "/tmp/x", "a/../b", "a\\b", "C:/x", "a:stream", "a//b", "NUL.txt", "a/b.",
                          ".brlinst/state.json"})
            fails([&] { validate_relative(path); }, "unsafe filename accepted");
        validate_relative("bin/program.exe");
        validate_relative("share/模型.txt");
        require(windows_filename_key("share/ReadMe.txt") == "SHARE/README.TXT",
                "Windows filename key did not fold ASCII");
#ifndef _WIN32
        fails([] { windows_filename_key("share/模型.txt"); },
              "Non-Windows filename validation accepted Unicode");
#endif
        fails([] { parse_json("{\"a\":1,\"a\":2}"); }, "duplicate JSON accepted");
        const auto rtf = text_rtf("Text {value} [field] 模型\n", {"Test Font", 18, 4096});
        require(rtf.rfind("{\\rtf1", 0) == 0, "RTF header missing");
        require(rtf.find("\\{value\\}") != std::string::npos, "RTF braces not escaped");
        require(rtf.find("\\'5bfield\\'5d") != std::string::npos, "RTF brackets not escaped");
        require(rtf.find("\\u27169?") != std::string::npos, "RTF Unicode not encoded");
        require(rtf.find("\\par\n") != std::string::npos, "RTF newline not encoded");
        fails([] { text_rtf("Text", {"Test Font", 18, 4}); }, "oversized RTF accepted");
        fails([] { text_rtf("Text", {"Bad;Font", 18, 4096}); }, "unsafe RTF font accepted");
        auto path_enabled = parse_options({"--quiet", "--scope", "machine", "--path", "on"});
        require(path_enabled.machine, "scope parsing");
        require(path_enabled.path, "enabled PATH option ignored");
        require(!parse_options({"--path", "off"}).path, "disabled PATH option ignored");
        require(!parse_options({}).path, "PATH option is not opt-in");
        fails([] { parse_options({"--quiet", "--quiet"}); }, "duplicate switch accepted");
        fails([] { parse_options({"--path", "yes"}); }, "invalid boolean accepted");
        fails([] { parse_options({"--scope", "user"}, true); }, "uninstaller accepts scope override");
#ifdef _WIN32
        auto binaries = fs::absolute(argv[0]).parent_path();
        auto uninstaller_module = LoadLibraryExW((binaries / "brlinst-uninstall.exe").c_str(), nullptr,
                                                 LOAD_LIBRARY_AS_DATAFILE);
        require(uninstaller_module != nullptr, "uninstaller resources unavailable");
        bool has_uninstaller_icon = FindResourceW(uninstaller_module, MAKEINTRESOURCEW(1), RT_GROUP_ICON) != nullptr;
        FreeLibrary(uninstaller_module);
        require(has_uninstaller_icon, "uninstaller icon missing");
        fs::copy_file(binaries / "brlinst-stub.exe", root / "stub.exe");
        fs::copy_file(binaries / "brlinst-uninstall.exe", root / "uninstall.exe");
#else
        write(root / "stub.exe", stub());
        write(root / "uninstall.exe", stub());
#endif
        write(root / "license.txt", "Test license\n");
        const std::string contents(200000, 'x');
        write(root / "payload/bin/app.exe", contents);
        write(root / "payload/empty", "");
        fs::create_directories(root / "payload/share/doc");
        write(root / "payload/share/doc/manual.txt", "Nested payload");
        Json config{{"formatVersion", 1},
                    {"id", "test.product"},
                    {"name", "Test Product"},
                    {"version", "1.2.3"},
                    {"architecture", native_test_architecture},
                    {"publisher", "Tests"},
                    {"license", "license.txt"},
                    {"shortcuts", Json::array({{{"name", "Test"}, {"target", "bin/app.exe"}}})}};
        auto branded_config = config;
        branded_config["branding"] = {
            {"style", "classic"},
            {"exe", {{"header", "header.bmp"}, {"sidebar", "sidebar.bmp"}}},
            {"msi",
             {{"icon", "product.ico"},
              {"banner", "banner.bmp"},
              {"dialog", "dialog.bmp"},
              {"license", "license.rtf"}}}};
        validate_product(branded_config);
        auto shared_branding = branded_config;
        shared_branding["branding"]["msi"].erase("banner");
        shared_branding["branding"]["msi"].erase("dialog");
        validate_product(shared_branding);
        auto x64_config = config;
        x64_config["architecture"] = "x64";
        require(product_architecture(x64_config) == Architecture::x64, "x64 architecture not parsed");
        auto arm_config = config;
        arm_config["architecture"] = "arm64";
        validate_product(arm_config);
        require(product_architecture(arm_config) == Architecture::arm64, "arm64 architecture not parsed");
        require(instance_id(x64_config) != instance_id(arm_config), "Architectures share an instance identity");
        auto missing_architecture = config;
        missing_architecture.erase("architecture");
        fails([&] { validate_product(missing_architecture); }, "Missing architecture accepted");
        auto invalid_architecture = config;
        invalid_architecture["architecture"] = "amd64";
        fails([&] { validate_product(invalid_architecture); }, "Invalid architecture accepted");
        auto incomplete_branding = branded_config;
        incomplete_branding["branding"]["exe"].erase("sidebar");
        fails([&] { validate_product(incomplete_branding); }, "Incomplete EXE branding accepted");
        auto unknown_branding = branded_config;
        unknown_branding["branding"]["style"] = "custom";
        fails([&] { validate_product(unknown_branding); }, "Unknown branding style accepted");
        auto duplicate_shortcuts = config;
        duplicate_shortcuts["shortcuts"] =
            Json::array({{{"name", "\xc3\xa9"}, {"target", "bin/app.exe"}},
                         {{"name", "\xc3\x89"}, {"target", "bin/app.exe"}}});
        validate_product(duplicate_shortcuts);
        write(root / "duplicate-shortcuts.json", duplicate_shortcuts.dump());
        fails(
            [&] {
                prepare_windows_input(root / "duplicate-shortcuts.json", root / "payload",
                                      root / "duplicate-shortcuts.exe");
            },
            "Windows-equivalent Unicode shortcut names accepted");
        write(root / "product.json", config.dump());
        write(root / "product.icon", "test icon");
        auto common_config = config;
        common_config["icon"] = "product.icon";
        write(root / "common-product.json", common_config.dump());
        const auto product_input = prepare_product(root / "common-product.json");
        require(product_input.product.at("licenseText") == "Test license\n",
                "shared product preparation did not load the license");
        require(!product_input.product.contains("license"),
                "shared product preparation retained the license path");
        require(product_input.icon == root / "product.icon",
                "shared product preparation did not resolve the icon");
#ifdef _WIN32
        fails(
            [&] {
                prepare_windows_input(root / "product.json", root / "payload",
                                      root / "PAYLOAD" / "setup.exe");
            },
            "case-aliased output inside source tree accepted");
#endif
        create_package(root / "product.json", root / "payload", root / "stub.exe", root / "uninstall.exe",
                       root / "setup.exe");
        create_package(root / "product.json", root / "payload", root / "stub.exe", root / "uninstall.exe",
                       root / "repeat.exe");
        require(read_text(root / "setup.exe") == read_text(root / "repeat.exe"),
                "package generation is not deterministic");
#ifndef _WIN32
        write(root / "product-arm64.json", arm_config.dump());
        write(root / "stub-arm64.exe", stub(pe_machine_arm64));
        write(root / "uninstall-arm64.exe", stub(pe_machine_arm64));
        create_package(root / "product-arm64.json", root / "payload", root / "stub-arm64.exe",
                       root / "uninstall-arm64.exe", root / "setup-arm64.exe");
        auto arm_package = open_package(root / "setup-arm64.exe");
        verify_package(arm_package);
        require(product_architecture(arm_package.product) == Architecture::arm64,
                "ARM64 package architecture was not retained");
        fails(
            [&] {
                create_package(root / "product-arm64.json", root / "payload", root / "stub.exe",
                               root / "uninstall-arm64.exe", root / "mismatched-arm64.exe");
            },
            "Mismatched setup architecture accepted");
#endif
        auto package = open_package(root / "setup.exe");
        verify_package(package);
        require(package.files.size() == 5, "wrong packaged files");
        auto signed_data = read_text(root / "setup.exe");
        std::uint32_t pe = 0;
        for (unsigned i = 0; i < 4; ++i)
            pe |= static_cast<std::uint32_t>(static_cast<unsigned char>(signed_data[0x3c + i])) << (8 * i);
        put(signed_data, pe + 24 + 112 + 32, signed_data.size(), 4);
        put(signed_data, pe + 24 + 112 + 36, 16, 4);
        signed_data.append(16, 's');
        write(root / "signed.exe", signed_data);
        verify_package(open_package(root / "signed.exe"));
        auto corrupt = read_text(root / "setup.exe");
        corrupt.at(static_cast<std::size_t>(package.payload_offset) + 8) ^= 0x3f;
        write(root / "corrupt.exe", corrupt);
        fails([&] { verify_package(open_package(root / "corrupt.exe")); }, "corrupt payload accepted");
        auto truncated = read_text(root / "setup.exe");
        truncated.resize(truncated.size() - 2);
        write(root / "truncated.exe", truncated);
        fails([&] { open_package(root / "truncated.exe"); }, "truncated package accepted");
        Backend backend;
        Options options;
        options.directory = root / "corrupt-install";
        fails([&] { install(open_package(root / "corrupt.exe"), options, backend); },
              "corrupt payload installed");
        require(!fs::exists(options.directory), "corrupt payload rollback left destination artifacts");
        auto invalid_package = package;
        invalid_package.product["welcome"] = std::string(max_manifest_size, 'x');
        options.directory = root / "bootstrap";
        fails([&] { install(invalid_package, options, backend); }, "oversized initial journal accepted");
        require(!fs::exists(options.directory), "failed journal bootstrap left destination artifacts");
        install(package, options, backend);
        uninstall(options.directory, backend);
        options.directory = root / "install";
        install(package, options, backend);
        require(read_text(root / "install/bin/app.exe") == contents, "payload differs after install");
        require(load_state(options.directory)["phase"] == "installed", "install state not committed");
        fails([&] { install(package, options, backend); }, "nonempty destination accepted");
        write(root / "install/user-file", "Keep me");
        write(root / "install/bin/app.exe", "Modified");
        auto kept = uninstall(options.directory, backend);
        require(kept.size() == 1, "modified payload not reported");
        require(fs::exists(root / "install/user-file") && fs::exists(root / "install/bin/app.exe"),
                "user data removed");
        require(!fs::exists(root / "install/empty"), "owned file not removed");
        require(!fs::exists(root / "install/share"), "nested owned directories not removed");
        options.directory = root / "rollback";
        backend.fail = true;
        fails([&] { install(package, options, backend); }, "backend failure ignored");
        require(!fs::exists(options.directory), "rollback left owned payload");
        backend.fail = false;
        options.directory = root / "cancel";
        bool writing = false;
        fails(
            [&] {
                install(
                    package, options, backend,
                    [&](const std::string &m, std::uint64_t, std::uint64_t) {
                        if (m.rfind("Installing ", 0) == 0)
                            writing = true;
                    },
                    [&] { return writing; });
            },
            "cancel ignored");
        require(!fs::exists(options.directory), "cancel left destination");
        options.directory = root / "recovery";
        install(package, options, backend);
        backend.fail_remove = true;
        fails([&] { uninstall(options.directory, backend); }, "removal failure ignored");
        require(fs::exists(options.directory / state_directory / uninstall_filename),
                "failed removal lost uninstaller");
        require(fs::exists(options.directory / "bin/app.exe"), "failed integration removal deleted payload");
        backend.fail_remove = false;
        auto state = load_state(options.directory);
        state["phase"] = "installing";
        durable_json(options.directory / state_directory / state_filename, state);
        uninstall(options.directory, backend);
        require(!fs::exists(options.directory), "interrupted install recovery failed");
        options.directory = root / "deferred";
        install(package, options, backend);
        auto deferred = uninstall(options.directory, backend, {}, true);
        require(deferred.empty(), "unchanged uninstaller was retained");
        require(load_state(options.directory)["phase"] == "cleanup", "deferred cleanup state missing");
        finalize_deferred_uninstall(options.directory);
        require(!fs::exists(options.directory / state_directory / state_filename),
                "deferred cleanup journal remained after finalization");
        require(fs::exists(options.directory / state_directory / uninstall_filename),
                "deferred finalization removed the running image");
        // On ordinary Windows volumes this spelling aliases the existing file;
        // creating it would overwrite that file, not create a collision fixture.
        if (!fs::exists(root / "payload/bin/APP.EXE")) {
            write(root / "payload/bin/APP.EXE", "collision");
            fails(
                [&] {
                    create_package(root / "product.json", root / "payload", root / "stub.exe",
                                   root / "uninstall.exe", root / "collision.exe");
                },
                "case collision accepted");
            fs::remove(root / "payload/bin/APP.EXE");
        }
#ifndef _WIN32
        auto outside = root / "outside";
        write(outside, "untouched");
        fs::create_symlink(outside, root / "unsafe.exe.building");
        fails(
            [&] {
                create_package(root / "product.json", root / "payload", root / "stub.exe",
                               root / "uninstall.exe", root / "unsafe.exe");
            },
            "output symlink accepted");
        require(read_text(outside) == "untouched", "output symlink changed an unrelated file");
        fs::create_directory_symlink(root / "install", root / "link");
        fails([&] { check_no_links(root / "link/file"); }, "symlink destination accepted");
#endif
        fs::remove_all(root);
        std::cout << "All core scenarios passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\nTest directory: " << root << '\n';
        return 1;
    }
}
