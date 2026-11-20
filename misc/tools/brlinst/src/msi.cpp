// SPDX-License-Identifier: BSD-2-Clause
#include "installer/msi.hpp"
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <bcrypt.h>
#include <fci.h>
#include <objbase.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
namespace installer::msi {
namespace {
constexpr std::uint32_t banner_bitmap_width = 493;
constexpr std::uint32_t banner_bitmap_height = 58;
constexpr std::uint32_t dialog_bitmap_width = 493;
constexpr std::uint32_t dialog_bitmap_height = 312;
struct Handle {
    MSIHANDLE value = 0;
    Handle() = default;
    explicit Handle(MSIHANDLE h) : value(h) {}
    Handle(const Handle &) = delete;
    Handle &operator=(const Handle &) = delete;
    ~Handle() {
        if (value)
            MsiCloseHandle(value);
    }
    operator MSIHANDLE() const {
        return value;
    }
};
std::wstring wide(const std::string &s) {
    return path_from_utf8(s).wstring();
}
fs::path preferred(fs::path path) {
    path.make_preferred();
    return path;
}
void report(const Progress &progress, const std::string &stage, std::uint64_t done = 0,
            std::uint64_t total = 0) {
    if (progress)
        progress(stage, done, total);
}
void check(UINT result, const std::string &what) {
    if (result == ERROR_SUCCESS)
        return;
    std::string detail;
    Handle error(MsiGetLastErrorRecord());
    if (error.value) {
        DWORD length = 0;
        wchar_t empty = 0;
        if (MsiFormatRecordW(0, error, &empty, &length) == ERROR_MORE_DATA) {
            std::wstring text(length + 1, L'\0');
            ++length;
            if (MsiFormatRecordW(0, error, text.data(), &length) == ERROR_SUCCESS) {
                text.resize(length);
                detail = ": " + utf8(fs::path(text));
            }
        }
    }
    throw Error(what + " (Windows Installer " + std::to_string(result) + ")" + detail);
}
std::wstring record_string(MSIHANDLE record, UINT field) {
    DWORD length = 0;
    wchar_t empty = 0;
    auto result = MsiRecordGetStringW(record, field, &empty, &length);
    if (result != ERROR_MORE_DATA && result != ERROR_SUCCESS)
        check(result, "Read MSI field");
    std::wstring text(length + 1, L'\0');
    ++length;
    check(MsiRecordGetStringW(record, field, text.data(), &length), "Read MSI field");
    text.resize(length);
    return text;
}
void query(MSIHANDLE db, const std::string &sql, MSIHANDLE parameters = 0) {
    Handle view;
    check(MsiDatabaseOpenViewW(db, wide(sql).c_str(), &view.value), "Prepare " + sql);
    check(MsiViewExecute(view, parameters), "Execute " + sql);
}
std::string guid_text(const std::array<unsigned char, 16> &bytes) {
    std::ostringstream out;
    out << '{' << std::uppercase << std::hex << std::setfill('0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            out << '-';
        out << std::setw(2) << static_cast<unsigned>(bytes[i]);
    }
    return out.str() + '}';
}
std::string stable_guid(const std::string &name) {
    // UUIDv8: first 128 SHA-256 bits with the version and variant bits set.
    // This identity scheme is part of the backend's v1 contract; do not change it.
    BCRYPT_ALG_HANDLE alg = nullptr;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0)
        throw Error("Cannot initialize SHA-256 identity generator");
    struct Close {
        BCRYPT_ALG_HANDLE h;
        ~Close() {
            BCryptCloseAlgorithmProvider(h, 0);
        }
    } close{alg};
    std::array<unsigned char, 32> hash{};
    if (BCryptHash(alg, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<char *>(name.data())),
                   static_cast<ULONG>(name.size()), hash.data(), static_cast<ULONG>(hash.size())) < 0)
        throw Error("Cannot generate MSI identity");
    std::array<unsigned char, 16> id{};
    std::copy_n(hash.begin(), id.size(), id.begin());
    id[6] = static_cast<unsigned char>((id[6] & 15) | 0x80);
    id[8] = static_cast<unsigned char>((id[8] & 63) | 0x80);
    return guid_text(id);
}
std::string random_guid() {
    GUID guid{};
    if (FAILED(CoCreateGuid(&guid)))
        throw Error("Cannot create package GUID");
    wchar_t text[40]{};
    if (!StringFromGUID2(guid, text, 40))
        throw Error("Cannot format package GUID");
    return utf8(fs::path(text));
}
void representable(const std::wstring &value) {
    if (value.empty())
        return;
    BOOL substituted = FALSE;
    if (!WideCharToMultiByte(1252, WC_NO_BEST_FIT_CHARS, value.data(), static_cast<int>(value.size()),
                             nullptr, 0, nullptr, &substituted) ||
        substituted)
        throw Error(
            "MSI v1 requires Windows-1252 product text and payload names; EXE supports broader Unicode");
}
void insert(MSIHANDLE view, const Table &table, const Json &row) {
    if (row.size() != table.columns.size())
        throw Error("Invalid MSI row width: " + table.name);
    Handle record(MsiCreateRecord(static_cast<UINT>(row.size())));
    if (!record.value)
        throw Error("Cannot allocate MSI record");
    for (std::size_t i = 0; i < row.size(); ++i) {
        auto field = static_cast<UINT>(i + 1);
        const auto &value = row[i];
        if (value.is_null())
            continue;
        if (value.is_number_integer()) {
            auto integer = value.get<std::int64_t>();
            if (integer <= INT_MIN || integer > INT_MAX)
                throw Error("MSI integer out of range");
            check(MsiRecordSetInteger(record, field, static_cast<int>(integer)), "Set MSI integer");
        } else if (value.is_object()) {
            auto path = preferred(path_from_utf8(value.at("stream").get<std::string>()));
            check_no_links(path);
            check(MsiRecordSetStreamW(record, field, path.c_str()), "Set MSI stream");
        } else {
            auto text = wide(value.get<std::string>());
            representable(text);
            check(MsiRecordSetStringW(record, field, text.c_str()), "Set MSI string");
        }
    }
    check(MsiViewModify(view, MSIMODIFY_INSERT, record), "Insert MSI row into " + table.name);
}
void write(const fs::path &path, const std::string &text) {
    std::ofstream out(path, std::ios::binary);
    if (!out.write(text.data(), static_cast<std::streamsize>(text.size())))
        throw Error("Cannot write " + utf8(path));
}
struct Cabinet {
    fs::path work;
    std::map<std::string, fs::path> sources;
    unsigned temporary = 0;
    fs::path path(const char *name) {
        auto it = sources.find(name);
        if (it != sources.end())
            return it->second;
        auto relative = path_from_utf8(name);
        if (relative.has_parent_path() || relative.empty())
            throw Error("Unexpected cabinet work filename");
        return work / relative;
    }
};
struct CabinetRange {
    std::size_t begin = 0, end = 0;
    std::string name;
    fs::path directory;
};
struct CabinetOutput {
    std::string name;
    fs::path path;
    std::size_t last_sequence = 0;
};
void *DIAMONDAPI cab_alloc(ULONG size) {
    return std::malloc(size);
}
void DIAMONDAPI cab_free(void *p) {
    std::free(p);
}
INT_PTR DIAMONDAPI cab_open(char *name, int flags, int, int *error, void *context) {
    try {
        auto &cab = *static_cast<Cabinet *>(context);
        auto path = preferred(cab.path(name));
        check_no_links(path);
        const bool writing = (flags & (_O_WRONLY | _O_RDWR)) != 0;
        // Source payloads are immutable inputs even if FCI asks unexpectedly.
        if (writing && cab.sources.count(name)) {
            *error = EACCES;
            return -1;
        }
        auto handle = CreateFileW(path.c_str(), writing ? GENERIC_READ | GENERIC_WRITE : GENERIC_READ,
                                  FILE_SHARE_READ, nullptr, flags & _O_CREAT ? CREATE_ALWAYS : OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            *error = static_cast<int>(GetLastError());
            return -1;
        }
        return reinterpret_cast<INT_PTR>(handle);
    } catch (...) {
        *error = EIO;
        return -1;
    }
}
UINT DIAMONDAPI cab_read(INT_PTR file, void *data, UINT count, int *error, void *) {
    DWORD done = 0;
    if (!ReadFile(reinterpret_cast<HANDLE>(file), data, count, &done, nullptr)) {
        *error = static_cast<int>(GetLastError());
        return static_cast<UINT>(-1);
    }
    return done;
}
UINT DIAMONDAPI cab_write(INT_PTR file, void *data, UINT count, int *error, void *) {
    DWORD done = 0;
    if (!WriteFile(reinterpret_cast<HANDLE>(file), data, count, &done, nullptr) || done != count) {
        *error = static_cast<int>(GetLastError());
        return static_cast<UINT>(-1);
    }
    return done;
}
int DIAMONDAPI cab_close(INT_PTR file, int *error, void *) {
    if (CloseHandle(reinterpret_cast<HANDLE>(file)))
        return 0;
    *error = static_cast<int>(GetLastError());
    return -1;
}
long DIAMONDAPI cab_seek(INT_PTR file, long distance, int origin, int *error, void *) {
    LARGE_INTEGER offset{}, result{};
    offset.QuadPart = distance;
    if (!SetFilePointerEx(reinterpret_cast<HANDLE>(file), offset, &result, static_cast<DWORD>(origin)) ||
        result.QuadPart > LONG_MAX) {
        *error = EIO;
        return -1;
    }
    return static_cast<long>(result.QuadPart);
}
int DIAMONDAPI cab_delete(char *name, int *error, void *context) {
    try {
        auto &cab = *static_cast<Cabinet *>(context);
        if (cab.sources.count(name)) {
            *error = EACCES;
            return -1;
        }
        if (DeleteFileW(preferred(cab.path(name)).c_str()))
            return 0;
    } catch (...) {
    }
    *error = EIO;
    return -1;
}
BOOL DIAMONDAPI cab_temp(char *name, int capacity, void *context) {
    try {
        auto &cab = *static_cast<Cabinet *>(context);
        auto text = "scratch" + std::to_string(++cab.temporary);
        if (text.size() + 1 > static_cast<std::size_t>(capacity))
            return FALSE;
        std::memcpy(name, text.c_str(), text.size() + 1);
        return TRUE;
    } catch (...) {
        return FALSE;
    }
}
int DIAMONDAPI cab_placed(PCCAB, char *, long, BOOL, void *) {
    return 0;
}
BOOL DIAMONDAPI cab_next(PCCAB, ULONG, void *) {
    return FALSE;
}
long DIAMONDAPI cab_status(UINT, ULONG, ULONG, void *) {
    return 0;
}
INT_PTR DIAMONDAPI cab_info(char *name, USHORT *date, USHORT *time, USHORT *attributes, int *error,
                            void *context) {
    *date = 33; // 1980-01-01, deterministic DOS timestamp
    *time = 0;
    *attributes = FILE_ATTRIBUTE_ARCHIVE;
    return cab_open(name, _O_RDONLY, 0, error, context);
}
void cabinet(const Model &model, const CabinetRange &range) {
    if (!fs::create_directory(range.directory))
        throw Error("Cannot create cabinet work directory");
    Cabinet context{range.directory, {}, 0};
    CCAB parameters{};
    parameters.cb = 0x7fffffff;
    parameters.cbFolderThresh = 64 * 1024 * 1024;
    parameters.setID = 1;
    if (range.name.size() >= sizeof(parameters.szCab))
        throw Error("Cabinet filename exceeds FCI limit");
    std::strcpy(parameters.szCab, range.name.c_str());
    ERF error{};
    auto handle = FCICreate(&error, cab_placed, cab_alloc, cab_free, cab_open, cab_read, cab_write, cab_close,
                            cab_seek, cab_delete, cab_temp, &parameters, &context);
    if (!handle)
        throw Error("Cannot create cabinet context");
    struct Destroy {
        HFCI h;
        ~Destroy() {
            FCIDestroy(h);
        }
    } destroy{handle};
    for (std::size_t i = range.begin; i < range.end; ++i) {
        const auto &file = model.files[i];
        auto token = "source" + file.id;
        auto member = file.id;
        context.sources.emplace(token, file.source);
        if (!FCIAddFile(handle, token.data(), member.data(), FALSE, cab_next, cab_status, cab_info,
                        tcompTYPE_MSZIP))
            throw Error("Cannot compress " + file.path + " (FCI " + std::to_string(error.erfOper) + ", " +
                        std::to_string(error.erfType) + ")");
    }
    if (!FCIFlushCabinet(handle, FALSE, cab_next, cab_status))
        throw Error("Cannot finish cabinet");
}
std::vector<CabinetRange> cabinet_ranges(const Model &model, const fs::path &work) {
    std::vector<CabinetRange> ranges;
    const auto count = parallel_worker_count(model.files.size());
    if (!count)
        return ranges;
    std::uint64_t remaining_bytes = 0;
    for (const auto &file : model.files)
        remaining_bytes += file.size;
    std::size_t begin = 0;
    for (std::size_t part = 0; part < count; ++part) {
        const auto remaining_parts = count - part;
        const auto target = (remaining_bytes + remaining_parts - 1) / remaining_parts;
        const auto last_allowed = model.files.size() - (remaining_parts - 1);
        std::uint64_t bytes = 0;
        std::size_t end = begin;
        while (end < last_allowed && (end == begin || bytes < target))
            bytes += model.files[end++].size;
        auto name = part ? "payload" + std::to_string(part + 1) + ".cab" : "payload.cab";
        ranges.push_back({begin, end, name, work / ("cabinet-" + std::to_string(part + 1))});
        begin = end;
        remaining_bytes -= bytes;
    }
    return ranges;
}
std::vector<CabinetOutput> cabinets(const Model &model, const fs::path &work, const Progress &progress) {
    auto ranges = cabinet_ranges(model, work);
    std::vector<CabinetOutput> outputs(ranges.size());
    parallel_for(ranges.size(), "Compressing MSI cabinets", progress, [&](std::size_t i) {
        cabinet(model, ranges[i]);
        outputs[i] = {ranges[i].name, ranges[i].directory / ranges[i].name, ranges[i].end};
    });
    return outputs;
}
void file_metadata(Model &model, const Progress &progress) {
    auto &rows = model.table("File").rows;
    std::vector<Json> hashes(model.files.size());
    parallel_for(model.files.size(), "Reading MSI file metadata", progress, [&](std::size_t i) {
        const auto &file = model.files[i];
        wchar_t version[256]{}, language[256]{};
        DWORD vn = 256, ln = 256;
        auto source = preferred(file.source);
        auto status = MsiGetFileVersionW(source.c_str(), version, &vn, language, &ln);
        if (status == ERROR_SUCCESS && vn) {
            rows[i][4] = utf8(fs::path(version));
            if (ln)
                rows[i][5] = utf8(fs::path(language));
        } else if (status == ERROR_FILE_INVALID || status == ERROR_FILE_NOT_FOUND ||
                   (status == ERROR_SUCCESS && !vn)) {
            MSIFILEHASHINFO hash{};
            hash.dwFileHashInfoSize = sizeof(hash);
            check(MsiGetFileHashW(source.c_str(), 0, &hash), "Hash " + file.path);
            hashes[i] = {file.id, 0, static_cast<std::int32_t>(hash.dwData[0]),
                         static_cast<std::int32_t>(hash.dwData[1]),
                         static_cast<std::int32_t>(hash.dwData[2]),
                         static_cast<std::int32_t>(hash.dwData[3])};
        } else
            check(status, "Read file version: " + file.path);
    });
    auto &hash_rows = model.table("MsiFileHash").rows;
    for (auto &hash : hashes)
        if (!hash.is_null())
            hash_rows.push_back(std::move(hash));
}
void summary(MSIHANDLE db, const Model &model, const Json &product) {
    Handle info;
    check(MsiGetSummaryInformationW(db, nullptr, 20, &info.value), "Open MSI summary");
    auto text = [&](UINT id, const std::string &value) {
        auto w = wide(value);
        representable(w);
        check(MsiSummaryInfoSetPropertyW(info, id, VT_LPSTR, 0, nullptr, w.c_str()),
              "Set MSI summary string");
    };
    auto number = [&](UINT id, int value, UINT type = VT_I4) {
        check(MsiSummaryInfoSetPropertyW(info, id, type, value, nullptr, nullptr), "Set MSI summary number");
    };
    number(1, 1252, VT_I2); // PID_CODEPAGE
    text(2, "Installation Database");
    text(3, model.name);
    text(4, product.at("publisher"));
    text(5, "Installer");
    text(7, std::string(model.architecture == Architecture::arm64 ? "Arm64" : "x64") + ";1033");
    text(9, random_guid());
    number(14, 500); // Windows Installer 5.0, needed for dual-purpose authoring
    number(15, 2); // Compressed dual-purpose package; machine scope may require elevation
    text(18, "brlinst");
    number(19, 2); // Read-only recommended
    check(MsiSummaryInfoPersist(info), "Persist MSI summary");
}
} // namespace
void verify(const fs::path &package, const Progress &progress) {
    auto native_package = preferred(package);
    report(progress, "Verifying MSI storage");
    check(MsiVerifyPackageW(native_package.c_str()), "Verify MSI storage");
    Handle db, view;
    check(MsiOpenDatabaseW(native_package.c_str(), MSIDBOPEN_READONLY, &db.value), "Open generated MSI");
    check(MsiDatabaseOpenViewW(db, L"SELECT `Name` FROM `_Tables`", &view.value), "Read MSI table list");
    check(MsiViewExecute(view, 0), "Query MSI tables");
    std::uint64_t checked_tables = 0;
    report(progress, "Verifying MSI tables");
    for (;;) {
        Handle record;
        auto result = MsiViewFetch(view, &record.value);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        check(result, "Fetch MSI table");
        auto name = record_string(record, 1);
        if (name.empty() || name.front() == L'_')
            continue;
        Handle table;
        check(MsiDatabaseOpenViewW(db, (L"SELECT * FROM `" + name + L"`").c_str(), &table.value),
              "Open MSI table");
        check(MsiViewExecute(table, 0), "Read MSI table");
        for (;;) {
            Handle item;
            result = MsiViewFetch(table, &item.value);
            if (result == ERROR_NO_MORE_ITEMS)
                break;
            check(result, "Fetch MSI row");
            auto validated = MsiViewModify(table, MSIMODIFY_VALIDATE, item);
            if (validated != ERROR_SUCCESS) {
                wchar_t column[256]{};
                DWORD length = 256;
                auto error = MsiViewGetErrorW(table, column, &length);
                throw Error("Invalid MSI row in " + utf8(fs::path(name)) + "." + utf8(fs::path(column)) +
                            " (database validation " + std::to_string(static_cast<int>(error)) + ")");
            }
        }
        report(progress, "Verifying MSI tables", ++checked_tables);
    }
    std::vector<std::pair<std::wstring, int>> cabinets;
    Handle media;
    check(MsiDatabaseOpenViewW(db,
                               L"SELECT `Cabinet`, `LastSequence` FROM `Media` ORDER BY `DiskId`",
                               &media.value),
          "Read MSI media");
    check(MsiViewExecute(media, 0), "Query MSI media");
    int previous_sequence = 0;
    for (;;) {
        Handle record;
        auto result = MsiViewFetch(media, &record.value);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        check(result, "Fetch MSI media");
        auto cabinet = record_string(record, 1);
        const int last_sequence = MsiRecordGetInteger(record, 2);
        if (cabinet.size() < 2 || cabinet.front() != L'#' || last_sequence == MSI_NULL_INTEGER ||
            last_sequence <= previous_sequence)
            throw Error("Invalid embedded MSI cabinet sequence");
        previous_sequence = last_sequence;
        cabinets.emplace_back(cabinet.substr(1), last_sequence);
    }
    if (cabinets.empty())
        throw Error("MSI has no embedded cabinets");
    Handle files;
    check(MsiDatabaseOpenViewW(db, L"SELECT `Sequence` FROM `File` ORDER BY `Sequence`", &files.value),
          "Read MSI file sequence");
    check(MsiViewExecute(files, 0), "Query MSI file sequence");
    int final_file_sequence = 0;
    for (;;) {
        Handle record;
        auto result = MsiViewFetch(files, &record.value);
        if (result == ERROR_NO_MORE_ITEMS)
            break;
        check(result, "Fetch MSI file sequence");
        final_file_sequence = MsiRecordGetInteger(record, 1);
    }
    if (final_file_sequence != previous_sequence)
        throw Error("MSI cabinet sequence does not cover every file");
    report(progress, "Verifying embedded cabinets", 0, cabinets.size());
    for (std::size_t i = 0; i < cabinets.size(); ++i) {
        Handle streams, parameters(MsiCreateRecord(1));
        if (!parameters.value)
            throw Error("Cannot allocate MSI cabinet query");
        check(MsiRecordSetStringW(parameters, 1, cabinets[i].first.c_str()),
              "Set MSI cabinet query");
        check(MsiDatabaseOpenViewW(db, L"SELECT `Data` FROM `_Streams` WHERE `Name` = ?", &streams.value),
              "Find embedded cabinet");
        check(MsiViewExecute(streams, parameters), "Read embedded cabinet");
        Handle payload;
        check(MsiViewFetch(streams, &payload.value),
              "Fetch embedded cabinet " + utf8(fs::path(cabinets[i].first)));
        char magic[4]{};
        DWORD count = 4;
        check(MsiRecordReadStream(payload, 1, magic, &count), "Read cabinet header");
        if (count != 4 || std::memcmp(magic, "MSCF", 4))
            throw Error("Missing MSI cabinet signature: " + utf8(fs::path(cabinets[i].first)));
        report(progress, "Verifying embedded cabinets", i + 1, cabinets.size());
    }
}
void create(const fs::path &config, const fs::path &source, const fs::path &output, const fs::path &actions,
            const Progress &progress) {
    report(progress, "Validating staged payload");
    auto input = prepare_windows_input(config, source, output);
    check_no_links(actions);
    if (!fs::is_regular_file(actions))
        throw Error("Missing brlinst-msi-actions.dll beside the generator");
    auto work = output;
    work += ".building";
    check_no_links(work);
    if (!fs::create_directory(work))
        throw Error("Remove stale MSI work directory: " + utf8(work));
    struct Cleanup {
        fs::path p;
        ~Cleanup() {
            std::error_code ec;
            fs::remove_all(p, ec);
        }
    } cleanup{work};
    if (input.product.contains("branding") &&
        input.product["branding"].value("style", "") == "classic") {
        report(progress, "Preparing MSI branding");
        if (input.branding.msi_banner.empty()) {
            if (input.branding.exe_header.empty())
                throw Error("Classic MSI branding requires an MSI banner or EXE header bitmap",
                            ExitCode::arguments);
            input.branding.msi_banner = work / "banner.bmp";
            make_bitmap_canvas(input.branding.exe_header, input.branding.msi_banner,
                               banner_bitmap_width, banner_bitmap_height);
        }
        if (input.branding.msi_dialog.empty()) {
            if (input.branding.exe_sidebar.empty())
                throw Error("Classic MSI branding requires an MSI dialog or EXE sidebar bitmap",
                            ExitCode::arguments);
            input.branding.msi_dialog = work / "dialog.bmp";
            make_bitmap_canvas(input.branding.exe_sidebar, input.branding.msi_dialog,
                               dialog_bitmap_width, dialog_bitmap_height);
        }
    }
    auto notices = work / "NOTICE.txt";
    write(notices, license_notices());
    report(progress, "Building MSI model", 0, input.files.size());
    auto model = make_model(input, notices, actions, stable_guid);
    report(progress, "Building MSI model", input.files.size(), input.files.size());
    // Hold read handles without write/delete sharing until both the metadata and
    // CAB have been written. A changing build tree must not produce a package
    // whose version/hash records describe different bytes from the payload.
    struct Locks {
        std::vector<HANDLE> files;
        ~Locks() {
            for (auto h : files)
                if (h != INVALID_HANDLE_VALUE)
                    CloseHandle(h);
        }
    } locks{std::vector<HANDLE>(model.files.size(), INVALID_HANDLE_VALUE)};
    parallel_for(model.files.size(), "Locking staged files", progress, [&](std::size_t i) {
        const auto &file = model.files[i];
        auto source = preferred(file.source);
        auto handle = CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw Error("Cannot lock staged file: " + file.path);
        locks.files[i] = handle;
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(handle, &size) || static_cast<std::uint64_t>(size.QuadPart) != file.size)
            throw Error("Staged file changed during packaging: " + file.path);
    });
    file_metadata(model, progress);
    auto cabinet_outputs = cabinets(model, work, progress);
    auto &media_rows = model.table("Media").rows;
    media_rows.clear();
    for (std::size_t i = 0; i < cabinet_outputs.size(); ++i)
        media_rows.push_back({i + 1, cabinet_outputs[i].last_sequence, nullptr,
                              "#" + cabinet_outputs[i].name, nullptr, nullptr});
    auto temporary = work / "package.msi";
    {
        Handle db;
        check(MsiOpenDatabaseW(preferred(temporary).c_str(), MSIDBOPEN_CREATE, &db.value),
              "Create MSI database");
        write(work / "codepage.idt", "\r\n\r\n1252\t_ForceCodepage\r\n");
        check(MsiDatabaseImportW(db, preferred(work).c_str(), L"codepage.idt"),
              "Set MSI database code page");
        report(progress, "Creating MSI tables", 0, model.tables.size());
        std::uint64_t created_tables = 0;
        for (const auto &t : model.tables) {
            query(db, sql_create(t));
            report(progress, "Creating MSI tables", ++created_tables, model.tables.size());
        }
        std::uint64_t row_count = 0;
        for (const auto &table : model.tables)
            row_count += table.rows.size();
        report(progress, "Writing MSI rows", 0, row_count);
        std::uint64_t written_rows = 0;
        for (const auto &t : model.tables) {
            Handle view;
            check(MsiDatabaseOpenViewW(db, wide("SELECT * FROM `" + t.name + "`").c_str(), &view.value),
                  "Open " + t.name + " for insertion");
            check(MsiViewExecute(view, 0), "Prepare " + t.name + " for insertion");
            for (const auto &r : t.rows) {
                insert(view, t, r);
                report(progress, "Writing MSI rows", ++written_rows, row_count);
            }
        }
        Table streams{"_Streams",
                      {{"Name", "CHAR(62)", "", false, true, false, "", 0},
                       {"Data", "OBJECT", "", false, false, false, "", 0}},
                      {}};
        report(progress, "Embedding MSI cabinets", 0, cabinet_outputs.size());
        Handle streams_view;
        check(MsiDatabaseOpenViewW(db, L"SELECT * FROM `_Streams`", &streams_view.value),
              "Open _Streams for insertion");
        check(MsiViewExecute(streams_view, 0), "Prepare _Streams for insertion");
        for (std::size_t i = 0; i < cabinet_outputs.size(); ++i) {
            const auto &cabinet_output = cabinet_outputs[i];
            insert(streams_view, streams,
                   Json{cabinet_output.name, {{"stream", utf8(cabinet_output.path)}}});
            report(progress, "Embedding MSI cabinets", i + 1, cabinet_outputs.size());
        }
        summary(db, model, input.product);
        report(progress, "Committing MSI database");
        check(MsiDatabaseCommit(db), "Commit MSI database");
    }
    if (fs::file_size(temporary) >= 0x7fffffffULL)
        throw Error("MSI exceeds the 2 GiB package limit");
    verify(temporary, progress);
    flush_file(temporary);
    publish_file(temporary, output);
}
} // namespace installer::msi
