// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include "notices.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#ifdef _WIN32
#  include <windows.h>
#endif
namespace installer {
namespace {
constexpr std::uint32_t bitmap_info_header_size = 40;
constexpr std::uint16_t bitmap_bits_per_pixel = 24;
constexpr std::size_t bitmap_file_header_size = 14;
constexpr std::size_t bitmap_header_bytes = bitmap_file_header_size + bitmap_info_header_size;
constexpr std::uint32_t max_bitmap_dimension = 16384;
constexpr std::uint64_t max_bitmap_size = 16 * 1024 * 1024;
bool same_path_component(const fs::path &left, const fs::path &right) {
#ifdef _WIN32
    const auto left_text = left.wstring();
    const auto right_text = right.wstring();
    return CompareStringOrdinal(left_text.c_str(), static_cast<int>(left_text.size()),
                                right_text.c_str(), static_cast<int>(right_text.size()), TRUE) == CSTR_EQUAL;
#else
    return left == right;
#endif
}
bool path_is_within(const fs::path &path, const fs::path &directory) {
    const auto canonical_path = fs::weakly_canonical(path);
    const auto canonical_directory = fs::weakly_canonical(directory);
    auto path_part = canonical_path.begin();
    for (auto directory_part = canonical_directory.begin(); directory_part != canonical_directory.end();
         ++directory_part, ++path_part)
        if (path_part == canonical_path.end() || !same_path_component(*path_part, *directory_part))
            return false;
    return true;
}
std::uint16_t little16(const std::string &data, std::size_t offset) {
    return static_cast<unsigned char>(data[offset]) |
           (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1])) << 8);
}
std::uint32_t little32(const std::string &data, std::size_t offset) {
    return little16(data, offset) | (static_cast<std::uint32_t>(little16(data, offset + 2)) << 16);
}
struct Bitmap {
    std::string data;
    std::uint32_t width, height, pixel_offset;
    std::uint64_t row_size;
};
Bitmap read_bitmap(const fs::path &path) {
    auto data = read_text(path, max_bitmap_size);
    if (data.size() < bitmap_header_bytes || data[0] != 'B' || data[1] != 'M')
        throw Error("Invalid branding bitmap: " + utf8(path), ExitCode::arguments);
    const auto info_size = little32(data, 14);
    const auto pixel_offset = little32(data, 10);
    const auto width = little32(data, 18);
    const auto height = little32(data, 22);
    if (!width || !height || width > max_bitmap_dimension || height > max_bitmap_dimension)
        throw Error("Invalid branding bitmap dimensions: " + utf8(path), ExitCode::arguments);
    const std::uint64_t row_size = (static_cast<std::uint64_t>(width) * 3 + 3) & ~std::uint64_t{3};
    const std::uint64_t required_size = pixel_offset + row_size * height;
    if (info_size < bitmap_info_header_size ||
        pixel_offset < bitmap_file_header_size + info_size || required_size > data.size() ||
        little16(data, 26) != 1 || little16(data, 28) != bitmap_bits_per_pixel ||
        little32(data, 30) != 0)
        throw Error("Branding bitmap must be an uncompressed 24-bit BMP: " + utf8(path),
                    ExitCode::arguments);
    return {std::move(data), width, height, pixel_offset, row_size};
}
void validate_bitmap(const fs::path &path, std::uint32_t width, std::uint32_t height) {
    const auto bitmap = read_bitmap(path);
    if (bitmap.width != width || bitmap.height != height)
        throw Error("Branding bitmap must be an uncompressed 24-bit " + std::to_string(width) + "x" +
                    std::to_string(height) + " BMP: " + utf8(path), ExitCode::arguments);
}
void put16(std::string &data, std::size_t offset, std::uint16_t value) {
    data[offset] = static_cast<char>(value);
    data[offset + 1] = static_cast<char>(value >> 8);
}
void put32(std::string &data, std::size_t offset, std::uint32_t value) {
    put16(data, offset, static_cast<std::uint16_t>(value));
    put16(data, offset + 2, static_cast<std::uint16_t>(value >> 16));
}
}
void make_bitmap_canvas(const fs::path &source, const fs::path &destination,
                        std::uint32_t width, std::uint32_t height) {
    const auto bitmap = read_bitmap(source);
    const std::uint64_t row_size = (static_cast<std::uint64_t>(width) * 3 + 3) & ~std::uint64_t{3};
    const std::uint64_t file_size = bitmap_header_bytes + row_size * height;
    if (!width || !height || width > max_bitmap_dimension || height > max_bitmap_dimension ||
        file_size > max_bitmap_size)
        throw Error("Invalid derived branding bitmap dimensions", ExitCode::arguments);
    std::string output(static_cast<std::size_t>(file_size), '\0');
    output[0] = 'B';
    output[1] = 'M';
    put32(output, 2, static_cast<std::uint32_t>(file_size));
    put32(output, 10, static_cast<std::uint32_t>(bitmap_header_bytes));
    put32(output, 14, bitmap_info_header_size);
    put32(output, 18, width);
    put32(output, 22, height);
    put16(output, 26, 1);
    put16(output, 28, bitmap_bits_per_pixel);
    put32(output, 34, static_cast<std::uint32_t>(row_size * height));
    put32(output, 38, little32(bitmap.data, 38));
    put32(output, 42, little32(bitmap.data, 42));
    const auto copy_width = std::min(bitmap.width, width);
    const auto copy_height = std::min(bitmap.height, height);
    for (std::uint32_t y = 0; y < height; ++y) {
        auto *target = output.data() + bitmap_header_bytes + (height - y - 1) * row_size;
        std::memset(target, 0xff, static_cast<std::size_t>(width) * 3);
        if (y < copy_height) {
            const auto *pixels = bitmap.data.data() + bitmap.pixel_offset +
                                 (bitmap.height - y - 1) * bitmap.row_size;
            std::memcpy(target, pixels, static_cast<std::size_t>(copy_width) * 3);
        }
    }
    std::ofstream file(destination, std::ios::binary);
    if (!file.write(output.data(), static_cast<std::streamsize>(output.size())))
        throw Error("Cannot write derived branding bitmap: " + utf8(destination));
    file.close();
    flush_file(destination);
}
std::string license_notices() {
    return std::string(reinterpret_cast<const char *>(distribution_notices));
}
ProductInput prepare_product(const fs::path &config) {
    ProductInput result;
    result.product = parse_json(read_text(config));
    validate_product(result.product);
    auto asset = [&](const std::string &name) {
        auto path = path_from_utf8(name);
        return path.is_absolute() ? path : config.parent_path() / path;
    };
    result.product["licenseText"] =
        read_text(asset(result.product.at("license").get<std::string>()), 1024 * 1024);
    result.product.erase("license");
    if (result.product.contains("icon")) {
        result.icon = asset(result.product["icon"].get<std::string>());
        check_no_links(result.icon);
        if (!fs::is_regular_file(result.icon))
            throw Error("Missing product icon");
        result.product.erase("icon");
    }
    return result;
}
WindowsPackageInput prepare_windows_input(const fs::path &config, const fs::path &source,
                                          const fs::path &output) {
    WindowsPackageInput result;
    auto common = prepare_product(config);
    result.product = std::move(common.product);
    result.icon = std::move(common.icon);
    auto &product = result.product;
    auto asset = [&](const std::string &name) {
        auto p = path_from_utf8(name);
        return p.is_absolute() ? p : config.parent_path() / p;
    };
    if (product.contains("branding")) {
        auto &branding = product["branding"];
        auto branding_asset = [&](const char *backend, const char *name, fs::path &destination,
                                  std::uint32_t width, std::uint32_t height) {
            if (!branding.contains(backend) || !branding[backend].contains(name))
                return;
            destination = asset(branding[backend][name].get<std::string>());
            check_no_links(destination);
            if (!fs::is_regular_file(destination))
                throw Error("Missing branding bitmap: " + utf8(destination), ExitCode::arguments);
            validate_bitmap(destination, width, height);
        };
        branding_asset("exe", "header", result.branding.exe_header, 150, 57);
        branding_asset("exe", "sidebar", result.branding.exe_sidebar, 164, 314);
        if (branding.contains("msi") && branding["msi"].contains("icon")) {
            result.branding.msi_icon = asset(branding["msi"]["icon"].get<std::string>());
            check_no_links(result.branding.msi_icon);
            if (!fs::is_regular_file(result.branding.msi_icon))
                throw Error("Missing MSI product icon: " + utf8(result.branding.msi_icon), ExitCode::arguments);
        }
        branding_asset("msi", "banner", result.branding.msi_banner, 493, 58);
        branding_asset("msi", "dialog", result.branding.msi_dialog, 493, 312);
        if (branding.contains("msi") && branding["msi"].contains("license")) {
            result.branding.msi_license = asset(branding["msi"]["license"].get<std::string>());
            check_no_links(result.branding.msi_license);
            if (!fs::is_regular_file(result.branding.msi_license))
                throw Error("Missing MSI license: " + utf8(result.branding.msi_license), ExitCode::arguments);
            const auto rtf = read_text(result.branding.msi_license, 60000);
            if (rtf.rfind("{\\rtf", 0) != 0)
                throw Error("MSI license must be an RTF document", ExitCode::arguments);
        }
        branding.erase("exe");
        branding.erase("msi");
    }
    validate_product(product, true);
    validate_windows_product(product);
    check_no_links(source);
    if (!fs::is_directory(source))
        throw Error("Source must be a staged directory");
    for (const auto &s : product["shortcuts"]) {
        for (auto key : {"target", "icon"})
            if (s.contains(key)) {
                auto path = source / path_from_utf8(s[key]);
                check_no_links(path);
                if (!fs::is_regular_file(path))
                    throw Error("Missing shortcut file: " + utf8(path));
            }
        auto dir = s.value("workingDirectory", "@profile");
        if (dir != "@profile" && dir != ".") {
            check_no_links(source / path_from_utf8(dir));
            if (!fs::is_directory(source / path_from_utf8(dir)))
                throw Error("Missing shortcut working directory");
        }
    }
    check_no_links(output);
    if (fs::exists(output))
        throw Error("Output already exists: " + utf8(output));
    if (path_is_within(output, source))
        throw Error("Output must be outside source tree");
    // Include directories in the collision set, so differently spelled parent
    // directories cannot merge silently when a Unix tree is installed on Windows.
    std::set<std::string> names;
    for (const auto &entry : fs::recursive_directory_iterator(source)) {
        check_no_links(entry.path());
        auto name = entry.path().lexically_relative(source).generic_u8string();
        validate_relative(name);
        auto key = windows_filename_key(name);
        if (!names.insert(key).second)
            throw Error("Duplicate Windows filename: " + name);
        if (entry.is_directory())
            continue;
        if (!entry.is_regular_file())
            throw Error("Unsupported payload file type");
        result.files.emplace_back(name, entry.path());
        if (result.files.size() > 199998)
            throw Error("Too many payload files");
    }
    std::sort(result.files.begin(), result.files.end());
    return result;
}
} // namespace installer
