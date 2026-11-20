// SPDX-License-Identifier: BSD-2-Clause
#include "installer/core.hpp"
#include <miniz.h>
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <map>
#include <set>

namespace installer {
namespace {
constexpr std::array<char, 16> magic{
    {'C', 'A', 'D', 'I', 'N', 'S', 'T', 'A', 'L', 'L', 'E', 'R', '0', '0', '1', '\0'}};
constexpr std::uint64_t footer_size = 48;
constexpr std::uint64_t max_file_size = 64ULL * 1024 * 1024 * 1024;
constexpr std::uint64_t max_total_size = 1024ULL * 1024 * 1024 * 1024;
constexpr std::uint16_t pe_machine_x64 = 0x8664;
constexpr std::uint16_t pe_machine_arm64 = 0xaa64;
std::uint64_t number(std::istream &in, unsigned bytes) {
    std::uint64_t n = 0;
    for (unsigned i = 0; i < bytes; ++i) {
        int c = in.get();
        if (c == EOF)
            throw Error("Truncated package header");
        n |= static_cast<std::uint64_t>(c) << (i * 8);
    }
    return n;
}
void number(std::ostream &out, std::uint64_t value, unsigned bytes) {
    for (unsigned i = 0; i < bytes; ++i)
        out.put(static_cast<char>(value >> (i * 8)));
}
struct PeInfo {
    std::uint64_t end;
    std::uint16_t machine;
};
std::uint16_t pe_machine(Architecture architecture) {
    return architecture == Architecture::arm64 ? pe_machine_arm64 : pe_machine_x64;
}
PeInfo pe_info(std::istream &in, std::uint64_t length) {
    if (length < 256)
        throw Error("Runtime is not a supported 64-bit Windows executable");
    in.seekg(0);
    if (number(in, 2) != 0x5a4d)
        throw Error("Runtime has no MZ header");
    in.seekg(0x3c);
    auto pe = number(in, 4);
    if (pe > length - 200)
        throw Error("Invalid PE header offset");
    in.seekg(static_cast<std::streamoff>(pe));
    if (number(in, 4) != 0x4550)
        throw Error("Runtime has no PE header");
    const auto machine = static_cast<std::uint16_t>(number(in, 2));
    if (machine != pe_machine_x64 && machine != pe_machine_arm64)
        throw Error("Expected Windows x64 or arm64 PE runtime");
    in.seekg(static_cast<std::streamoff>(pe + 24));
    if (number(in, 2) != 0x20b)
        throw Error("Expected PE32+ runtime");
    in.seekg(static_cast<std::streamoff>(pe + 24 + 108));
    if (number(in, 4) < 5)
        return {length, machine};
    in.seekg(static_cast<std::streamoff>(pe + 24 + 112 + 4 * 8));
    auto certificates = number(in, 4);
    auto size = number(in, 4);
    if (certificates == 0 && size == 0)
        return {length, machine};
    if (certificates < pe + 200 || certificates > length || size != length - certificates || certificates % 8)
        throw Error("Invalid PE certificate table");
    return {certificates, machine};
}
std::string folded(const std::string &path) {
    return windows_filename_key(path);
}
void check_entries(const std::vector<FileEntry> &entries, std::uint64_t available) {
    std::set<std::string> files, dirs;
    std::uint64_t offset = 0, total = 0;
    bool has_uninstaller = false;
    if (entries.empty() || entries.size() > 200000)
        throw Error("Invalid package file count");
    for (const auto &entry : entries) {
        validate_relative(entry.path, true);
        const bool internal = entry.path.rfind(std::string(state_directory) + "/", 0) == 0;
        if (internal && entry.path != std::string(state_directory) + "/" + uninstall_filename &&
            entry.path != std::string(state_directory) + "/" + notices_filename)
            throw Error("Unexpected internal payload file");
        if (!internal)
            validate_relative(entry.path);
        if (entry.path == std::string(state_directory) + "/" + uninstall_filename)
            has_uninstaller = true;
        auto key = folded(entry.path);
        if (!files.insert(key).second)
            throw Error("Duplicate Windows filename: " + entry.path);
        auto parent = path_from_utf8(key).parent_path();
        while (!parent.empty()) {
            dirs.insert(parent.generic_u8string());
            parent = parent.parent_path();
        }
        if (entry.offset != offset || offset > available || entry.compressed_size > available - offset ||
            entry.size > max_file_size)
            throw Error("Invalid payload file bounds");
        offset += entry.compressed_size;
        if (entry.size > max_total_size - total)
            throw Error("Expanded payload exceeds limit");
        total += entry.size;
    }
    for (const auto &dir : dirs)
        if (files.count(dir))
            throw Error("File/directory collision: " + dir);
    if (!has_uninstaller)
        throw Error("Package has no uninstaller");
    if (available < offset || available - offset > 7)
        throw Error("Unexpected bytes between payload and manifest");
}
void inflate_entry(const Package &package, const FileEntry &entry, std::ostream *out, const Cancel &cancel) {
    std::ifstream in(package.executable, std::ios::binary);
    if (!in)
        throw Error("Cannot reopen package");
    in.seekg(static_cast<std::streamoff>(package.payload_offset + entry.offset));
    mz_stream stream{};
    if (mz_inflateInit(&stream) != MZ_OK)
        throw Error("Cannot initialize decompressor");
    struct End {
        mz_stream *s;
        ~End() {
            mz_inflateEnd(s);
        }
    } end{&stream};
    std::array<unsigned char, buffer_size> input{}, output{};
    std::uint64_t left = entry.compressed_size, produced = 0;
    mz_ulong checksum = 0;
    int result = MZ_OK;
    while (result != MZ_STREAM_END) {
        check_cancel(cancel);
        if (stream.avail_in == 0 && left) {
            auto n = static_cast<std::size_t>(std::min<std::uint64_t>(left, input.size()));
            if (!in.read(reinterpret_cast<char *>(input.data()), static_cast<std::streamsize>(n)))
                throw Error("Truncated compressed file");
            left -= n;
            stream.next_in = input.data();
            stream.avail_in = static_cast<unsigned int>(n);
        }
        stream.next_out = output.data();
        stream.avail_out = static_cast<unsigned int>(output.size());
        result = mz_inflate(&stream, MZ_NO_FLUSH);
        auto n = output.size() - stream.avail_out;
        if (result != MZ_OK && result != MZ_STREAM_END)
            throw Error("Corrupt compressed file: " + entry.path);
        if (n > entry.size - produced)
            throw Error("Expanded file exceeds declared size");
        produced += n;
        checksum = mz_crc32(checksum, output.data(), n);
        if (out && !out->write(reinterpret_cast<char *>(output.data()), static_cast<std::streamsize>(n)))
            throw Error("Cannot write " + entry.path);
        if (result != MZ_STREAM_END && !left && !stream.avail_in && !n)
            throw Error("Truncated deflate stream");
    }
    if (left || stream.avail_in || produced != entry.size || checksum != entry.crc)
        throw Error("Payload integrity check failed: " + entry.path);
}
FileEntry deflate_file(const fs::path &source, const std::string &name, std::ostream &out,
                       std::uint64_t origin) {
    check_no_links(source);
    if (!fs::is_regular_file(source))
        throw Error("Not a regular file: " + utf8(source));
    FileEntry entry;
    entry.path = name;
    entry.offset = static_cast<std::uint64_t>(out.tellp()) - origin;
    std::ifstream in(source, std::ios::binary);
    if (!in)
        throw Error("Cannot read " + utf8(source));
    mz_stream stream{};
    if (mz_deflateInit(&stream, MZ_BEST_COMPRESSION) != MZ_OK)
        throw Error("Cannot initialize compressor");
    struct End {
        mz_stream *s;
        ~End() {
            mz_deflateEnd(s);
        }
    } end{&stream};
    std::array<unsigned char, buffer_size> input{}, output{};
    int result = MZ_OK;
    do {
        in.read(reinterpret_cast<char *>(input.data()), input.size());
        auto n = static_cast<std::size_t>(in.gcount());
        if (!in && !in.eof())
            throw Error("Payload read error");
        entry.size += n;
        if (entry.size > max_file_size)
            throw Error("File exceeds package size limit");
        entry.crc = static_cast<std::uint32_t>(mz_crc32(entry.crc, input.data(), n));
        stream.next_in = input.data();
        stream.avail_in = static_cast<unsigned int>(n);
        const int flush = in.eof() ? MZ_FINISH : MZ_NO_FLUSH;
        do {
            stream.next_out = output.data();
            stream.avail_out = static_cast<unsigned int>(output.size());
            result = mz_deflate(&stream, flush);
            if (result != MZ_OK && result != MZ_STREAM_END)
                throw Error("Compression failed");
            auto used = output.size() - stream.avail_out;
            if (!out.write(reinterpret_cast<char *>(output.data()), static_cast<std::streamsize>(used)))
                throw Error("Package write failed");
        } while (stream.avail_out == 0);
    } while (result != MZ_STREAM_END);
    entry.compressed_size = static_cast<std::uint64_t>(out.tellp()) - origin - entry.offset;
    return entry;
}
FileEntry deflate_text(const std::string &text, const std::string &name, std::ostream &out,
                       std::uint64_t origin) {
    mz_ulong length = mz_compressBound(static_cast<mz_ulong>(text.size()));
    std::vector<unsigned char> compressed(length);
    if (mz_compress2(compressed.data(), &length, reinterpret_cast<const unsigned char *>(text.data()),
                     static_cast<mz_ulong>(text.size()), MZ_BEST_COMPRESSION) != MZ_OK)
        throw Error("Cannot compress dependency notices");
    FileEntry entry{name, text.size(), length, static_cast<std::uint64_t>(out.tellp()) - origin,
                    static_cast<std::uint32_t>(
                        mz_crc32(0, reinterpret_cast<const unsigned char *>(text.data()), text.size()))};
    if (!out.write(reinterpret_cast<const char *>(compressed.data()), static_cast<std::streamsize>(length)))
        throw Error("Cannot write dependency notices");
    return entry;
}
void append_bytes(std::istream &input, std::ostream &output, std::uint64_t offset, std::uint64_t length) {
    input.clear();
    input.seekg(static_cast<std::streamoff>(offset));
    std::array<char, buffer_size> buffer{};
    while (length) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(length, buffer.size()));
        if (!input.read(buffer.data(), static_cast<std::streamsize>(count)))
            throw Error("Cannot read compressed payload shard");
        if (!output.write(buffer.data(), static_cast<std::streamsize>(count)))
            throw Error("Cannot assemble installer payload");
        length -= count;
    }
}
} // namespace

Package open_package(const fs::path &executable) {
    Package package;
    package.executable = executable;
    std::ifstream in(executable, std::ios::binary);
    if (!in)
        throw Error("Cannot read package");
    auto length = fs::file_size(executable);
    if (length > max_package_size)
        throw Error("Package exceeds supported executable size");
    const auto image = pe_info(in, length);
    auto end = image.end;
    if (end < footer_size)
        throw Error("Missing package footer");
    in.seekg(static_cast<std::streamoff>(end - footer_size));
    std::array<char, 16> actual{};
    in.read(actual.data(), actual.size());
    if (actual != magic)
        throw Error("No installer payload found");
    auto payload = number(in, 8), manifest = number(in, 8), size = number(in, 8);
    auto crc = number(in, 4), version = number(in, 4);
    if (version != 1 || payload < 256 || manifest < payload || manifest > end - footer_size ||
        size != end - footer_size - manifest || size > max_manifest_size)
        throw Error("Invalid package manifest bounds");
    in.seekg(static_cast<std::streamoff>(manifest));
    std::string text(static_cast<std::size_t>(size), '\0');
    if (!in.read(text.data(), static_cast<std::streamsize>(size)))
        throw Error("Cannot read package manifest");
    if (mz_crc32(0, reinterpret_cast<const unsigned char *>(text.data()), text.size()) != crc)
        throw Error("Corrupt package manifest");
    auto data = parse_json(text);
    if (data.at("formatVersion") != 1)
        throw Error("Unsupported package version");
    package.product = data.at("product");
    validate_product(package.product, true);
    validate_windows_product(package.product);
    if (image.machine != pe_machine(product_architecture(package.product)))
        throw Error("Executable architecture does not match the package manifest");
    package.payload_offset = payload;
    if (!data.at("files").is_array() || data.at("files").size() > 200000)
        throw Error("Invalid package file list");
    for (const auto &item : data.at("files")) {
        for (auto key : {"size", "compressedSize", "offset", "crc"})
            if (!item.at(key).is_number_unsigned())
                throw Error("Invalid file metadata");
        if (item.at("crc").get<std::uint64_t>() > UINT32_MAX)
            throw Error("Invalid file checksum");
        package.files.push_back({item.at("path").get<std::string>(), item.at("size"),
                                 item.at("compressedSize"), item.at("offset"), item.at("crc")});
    }
    check_entries(package.files, manifest - payload);
    return package;
}
void verify_package(const Package &p, const Progress &progress, const Cancel &cancel) {
    std::uint64_t total = 0, done = 0;
    for (const auto &e : p.files)
        total += e.size;
    for (const auto &e : p.files) {
        if (progress)
            progress("Verifying " + e.path, done, total);
        inflate_entry(p, e, nullptr, cancel);
        done += e.size;
    }
}
void extract_file(const Package &p, const FileEntry &e, const fs::path &output, const Cancel &cancel) {
    check_no_links(output);
    if (fs::exists(output))
        throw Error("Refusing to replace existing file: " + utf8(output));
    std::ofstream out(output, std::ios::binary);
    if (!out)
        throw Error("Cannot create " + utf8(output));
    inflate_entry(p, e, &out, cancel);
    out.flush();
    if (!out)
        throw Error("Cannot flush " + utf8(output));
    out.close();
}
void create_package(const fs::path &config, const fs::path &source, const fs::path &stub,
                    const fs::path &uninstaller, const fs::path &output, const Progress &progress) {
    auto input = prepare_windows_input(config, source, output);
    auto &product = input.product;
    auto &icon = input.icon;
    if (product.contains("branding") && product["branding"].value("style", "") == "classic" &&
        (input.branding.exe_header.empty() || input.branding.exe_sidebar.empty()))
        throw Error("Classic EXE branding requires header and sidebar bitmaps", ExitCode::arguments);
    auto inputs = std::move(input.files);
    inputs.insert(inputs.begin(), {std::string(state_directory) + "/" + uninstall_filename, uninstaller});
    inputs.emplace_back(std::string(state_directory) + "/" + notices_filename, fs::path{});
    std::sort(inputs.begin() + 1, inputs.end());
    // Reject collisions before creating output (including Windows case aliases).
    std::vector<FileEntry> preflight;
    for (const auto &i : inputs)
        preflight.push_back({i.first, 0, 0, 0, 0});
    check_entries(preflight, 0);
    {
        std::ifstream in(stub, std::ios::binary);
        const auto executable = pe_info(in, fs::file_size(stub));
        if (executable.machine != pe_machine(product_architecture(product)))
            throw Error("Setup stub architecture does not match the package configuration");
        if (executable.end != fs::file_size(stub))
            throw Error("Use an unsigned setup stub; sign the assembled installer");
    }
    {
        std::ifstream in(uninstaller, std::ios::binary);
        if (pe_info(in, fs::file_size(uninstaller)).machine !=
            pe_machine(product_architecture(product)))
            throw Error("Uninstaller architecture does not match the package configuration");
    }
    auto temporary = output;
    temporary += ".building";
    auto parts_directory = output;
    parts_directory += ".parts";
    check_no_links(temporary);
    check_no_links(parts_directory);
    if (fs::exists(temporary))
        throw Error("Remove stale package output: " + utf8(temporary));
    if (fs::exists(parts_directory))
        throw Error("Remove stale package work directory: " + utf8(parts_directory));
    try {
        fs::copy_file(stub, temporary);
#ifdef _WIN32
        update_resources(temporary, product, icon, input.branding);
#else
        if (!icon.empty() || !input.branding.exe_header.empty() || !input.branding.exe_sidebar.empty())
            throw Error("Branding executable resources requires Windows");
#endif
        if (!fs::create_directory(parts_directory))
            throw Error("Cannot create package work directory");
        struct CompressedEntry {
            FileEntry entry;
            std::size_t part = 0;
        };
        const auto part_count = parallel_worker_count(inputs.size());
        std::vector<fs::path> part_paths(part_count);
        std::vector<CompressedEntry> compressed(inputs.size());
        parallel_for(part_count, "Compressing EXE payload", progress, [&](std::size_t part) {
            auto part_path = parts_directory / ("payload-" + std::to_string(part + 1));
            part_paths[part] = part_path;
            std::ofstream part_output(part_path, std::ios::binary);
            if (!part_output)
                throw Error("Cannot create compressed payload shard");
            for (std::size_t i = part; i < inputs.size(); i += part_count) {
                auto entry = inputs[i].second.empty()
                                 ? deflate_text(license_notices(), inputs[i].first, part_output, 0)
                                 : deflate_file(inputs[i].second, inputs[i].first, part_output, 0);
                compressed[i] = {entry, part};
            }
            part_output.flush();
            if (!part_output)
                throw Error("Cannot flush compressed payload shard");
        });
        std::ofstream out(temporary, std::ios::binary | std::ios::app);
        out.seekp(0, std::ios::end);
        if (!out)
            throw Error("Cannot append installer payload");
        const auto payload = static_cast<std::uint64_t>(out.tellp());
        Json files = Json::array();
        std::vector<FileEntry> entries;
        std::vector<std::ifstream> part_inputs(part_count);
        for (std::size_t part = 0; part < part_count; ++part) {
            part_inputs[part].open(part_paths[part], std::ios::binary);
            if (!part_inputs[part])
                throw Error("Cannot open compressed payload shard");
        }
        for (auto &compressed_entry : compressed) {
            auto entry = compressed_entry.entry;
            const auto part_offset = entry.offset;
            entry.offset = static_cast<std::uint64_t>(out.tellp()) - payload;
            append_bytes(part_inputs[compressed_entry.part], out, part_offset, entry.compressed_size);
            files.push_back({{"path", entry.path},
                             {"size", entry.size},
                             {"compressedSize", entry.compressed_size},
                             {"offset", entry.offset},
                             {"crc", entry.crc}});
            entries.push_back(entry);
        }
        std::string manifest = Json{{"formatVersion", 1}, {"product", product}, {"files", files}}.dump();
        if (manifest.size() > max_manifest_size)
            throw Error("Manifest exceeds size limit");
        // SignTool aligns certificate data to eight bytes. Align our footer now
        // so its position is identical before and after Authenticode signing.
        while ((static_cast<std::uint64_t>(out.tellp()) + manifest.size() + footer_size) % 8)
            out.put('\0');
        const auto manifest_offset = static_cast<std::uint64_t>(out.tellp());
        check_entries(entries, manifest_offset - payload);
        out.write(manifest.data(), static_cast<std::streamsize>(manifest.size()));
        out.write(magic.data(), magic.size());
        number(out, payload, 8);
        number(out, manifest_offset, 8);
        number(out, manifest.size(), 8);
        number(out,
               mz_crc32(0, reinterpret_cast<const unsigned char *>(manifest.data()), manifest.size()),
               4);
        number(out, 1, 4);
        out.flush();
        if (!out || static_cast<std::uint64_t>(out.tellp()) > max_package_size)
            throw Error("Package too large or write failed");
        out.close();
        for (auto &part_input : part_inputs)
            part_input.close();
        fs::remove_all(parts_directory);
        verify_package(open_package(temporary));
        flush_file(temporary);
        publish_file(temporary, output);
    } catch (...) {
        std::error_code ec;
        fs::remove(temporary, ec);
        fs::remove_all(parts_directory, ec);
        throw;
    }
}
} // namespace installer
