// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <windows.h>
#include <array>
#include <cstring>
#include <sstream>

namespace installer {
namespace {
std::wstring wtext(const std::string &text) {
    return path_from_utf8(text).wstring();
}
struct VersionBlock {
    std::vector<BYTE> data;
    void word(WORD value) {
        data.push_back(static_cast<BYTE>(value));
        data.push_back(static_cast<BYTE>(value >> 8));
    }
    void align() {
        while (data.size() % 4)
            data.push_back(0);
    }
    void text(const std::wstring &value) {
        for (auto c : value)
            word(static_cast<WORD>(c));
        word(0);
    }
    void start(WORD size, WORD type, const std::wstring &key) {
        word(0);
        word(size);
        word(type);
        text(key);
        align();
    }
    void append(const VersionBlock &child) {
        align();
        data.insert(data.end(), child.data.begin(), child.data.end());
    }
    void finish() {
        if (data.size() > 65535)
            throw Error("Executable version resources exceed size limit");
        data[0] = static_cast<BYTE>(data.size());
        data[1] = static_cast<BYTE>(data.size() >> 8);
    }
};
WORD read16(const std::string &s, std::size_t offset) {
    return static_cast<unsigned char>(s.at(offset)) |
           (static_cast<WORD>(static_cast<unsigned char>(s.at(offset + 1))) << 8);
}
DWORD read32(const std::string &s, std::size_t offset) {
    return read16(s, offset) | (static_cast<DWORD>(read16(s, offset + 2)) << 16);
}
void resource(HANDLE h, LPCWSTR type, WORD id, const void *data, DWORD bytes) {
    if (!UpdateResourceW(h, type, MAKEINTRESOURCEW(id), MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                         const_cast<void *>(data), bytes))
        throw Error("Cannot update executable resources");
}
} // namespace
void update_resources(const fs::path &executable, const Json &product, const fs::path &icon,
                      const WindowsPackageInput::Branding &branding) {
    HANDLE update = BeginUpdateResourceW(executable.c_str(), FALSE);
    if (!update)
        throw Error("Cannot open executable resources");
    try {
        VersionBlock strings;
        strings.start(0, 1, L"040904B0");
        auto field = [&](const std::wstring &key, const std::string &value) {
            auto w = wtext(value);
            if (w.size() > 2048)
                throw Error("Product metadata too long for executable resources");
            VersionBlock block;
            block.start(static_cast<WORD>(w.size() + 1), 1, key);
            block.text(w);
            block.finish();
            strings.append(block);
        };
        field(L"ProductName", product.at("name"));
        field(L"CompanyName", product.at("publisher"));
        field(L"ProductVersion", product.at("version"));
        field(L"FileVersion", product.at("version"));
        field(L"FileDescription", product.at("name").get<std::string>() + " Installer");
        strings.finish();
        VersionBlock info;
        info.start(0, 1, L"StringFileInfo");
        info.append(strings);
        info.finish();
        VS_FIXEDFILEINFO fixed{};
        fixed.dwSignature = VS_FFI_SIGNATURE;
        fixed.dwStrucVersion = 0x10000;
        fixed.dwFileOS = VOS_NT_WINDOWS32;
        fixed.dwFileType = VFT_APP;
        fixed.dwFileFlagsMask = VS_FFI_FILEFLAGSMASK;
        std::istringstream version(product.at("version").get<std::string>());
        std::array<unsigned, 4> numbers{};
        for (auto &n : numbers) {
            version >> n;
            if (n > 65535)
                n = 0;
            if (version.peek() == '.')
                version.get();
            else
                break;
        }
        fixed.dwFileVersionMS = fixed.dwProductVersionMS = MAKELONG(numbers[1], numbers[0]);
        fixed.dwFileVersionLS = fixed.dwProductVersionLS = MAKELONG(numbers[3], numbers[2]);
        VersionBlock root;
        root.start(sizeof(fixed), 0, L"VS_VERSION_INFO");
        auto bytes = reinterpret_cast<BYTE *>(&fixed);
        root.data.insert(root.data.end(), bytes, bytes + sizeof(fixed));
        root.append(info);
        VersionBlock translation;
        translation.start(4, 0, L"Translation");
        translation.word(0x0409);
        translation.word(1200);
        translation.finish();
        VersionBlock var;
        var.start(0, 1, L"VarFileInfo");
        var.append(translation);
        var.finish();
        root.append(var);
        root.finish();
        resource(update, RT_VERSION, 1, root.data.data(), static_cast<DWORD>(root.data.size()));
        auto bitmap = [&](const fs::path &path, WORD id) {
            if (path.empty())
                return;
            auto data = read_text(path, 16 * 1024 * 1024);
            constexpr std::size_t bitmap_file_header_size = 14;
            resource(update, RT_BITMAP, id, data.data() + bitmap_file_header_size,
                     static_cast<DWORD>(data.size() - bitmap_file_header_size));
        };
        bitmap(branding.exe_header, 200);
        bitmap(branding.exe_sidebar, 201);
        if (!icon.empty()) {
            auto data = read_text(icon, 4 * 1024 * 1024);
            if (data.size() < 6 || read16(data, 0) != 0 || read16(data, 2) != 1 || read16(data, 4) == 0 ||
                read16(data, 4) > 128)
                throw Error("Invalid .ico resource");
            auto count = read16(data, 4);
            if (data.size() < 6 + static_cast<std::size_t>(count) * 16)
                throw Error("Truncated icon directory");
            VersionBlock group;
            group.word(0);
            group.word(1);
            group.word(count);
            for (WORD i = 0; i < count; ++i) {
                auto at = 6 + static_cast<std::size_t>(i) * 16;
                auto length = read32(data, at + 8), offset = read32(data, at + 12);
                if (offset > data.size() || length > data.size() - offset)
                    throw Error("Invalid icon bounds");
                resource(update, RT_ICON, static_cast<WORD>(100 + i), data.data() + offset, length);
                group.data.insert(group.data.end(), data.begin() + static_cast<std::ptrdiff_t>(at),
                                  data.begin() + static_cast<std::ptrdiff_t>(at + 12));
                group.word(static_cast<WORD>(100 + i));
            }
            resource(update, RT_GROUP_ICON, 1, group.data.data(), static_cast<DWORD>(group.data.size()));
        }
        if (!EndUpdateResourceW(update, FALSE)) {
            update = nullptr;
            throw Error("Cannot commit executable resources");
        }
        update = nullptr;
    } catch (...) {
        if (update)
            EndUpdateResourceW(update, TRUE);
        throw;
    }
}
} // namespace installer
