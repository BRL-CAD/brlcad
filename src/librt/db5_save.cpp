/*                     D B 5 _ S A V E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
#include "common.h"

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "bio.h"
#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/path.h"
#include "bu/str.h"
#include "bu/uuid.h"
#include "bu/vls.h"
#include "raytrace.h"
#include "db_history_db5.h"
#include "db_history_file_lock.h"
#include "librt_private.h"

#if defined(_WIN32) && defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__) && !defined(__MSYS__)
#  define DB5_WIN32_EXPORT 1
#  if defined(FILE_RENAME_FLAG_POSIX_SEMANTICS) && defined(FILE_RENAME_FLAG_REPLACE_IF_EXISTS)
#    define DB5_WIN32_REPLACEMENT 1
#  endif
#elif defined(HAVE_FSYNC) && defined(HAVE_MKSTEMP) && defined(HAVE_SYS_FILE_H) && \
    defined(HAVE_SYS_STAT_H) && defined(HAVE_LSTAT) && defined(HAVE_UNISTD_H)
#  include <sys/file.h>
#  include <sys/stat.h>
#  if defined(LOCK_EX) && defined(LOCK_NB)
#    define DB5_POSIX_REPLACEMENT 1
#  endif
#endif

#if defined(DB5_POSIX_REPLACEMENT) || defined(DB5_WIN32_REPLACEMENT)
#  define DB5_ATOMIC_REPLACEMENT 1
#endif

#if defined(DB5_ATOMIC_REPLACEMENT) || defined(DB5_WIN32_EXPORT)
#  define DB5_ATOMIC_EXPORT 1
#endif

#ifdef DB5_WIN32_EXPORT
struct SnapshotFileState {
    BY_HANDLE_FILE_INFORMATION info;
    FILE_BASIC_INFO basic;
};
#elif defined(DB5_POSIX_REPLACEMENT)
using SnapshotFileState = struct stat;
#endif

struct db5_save {
#ifdef DB5_ATOMIC_REPLACEMENT
    std::string path;
    SnapshotFileState original = {};
#  ifdef DB5_POSIX_REPLACEMENT
    std::filesystem::file_time_type modified;
#  endif
    const struct db_i *base = nullptr;
    bool installed = false;
#endif
};

static void
db5_save_message(struct bu_vls *message, const char *reason)
{
    if (!message)
	return;
    bu_vls_trunc(message, 0);
    bu_vls_strcat(message, reason);
}

#ifdef DB5_ATOMIC_EXPORT
namespace {

class FileDescriptor {
public:
    explicit FileDescriptor(int descriptor) : fd(descriptor) {}
    ~FileDescriptor()
    {
	if (fd >= 0)
	    close(fd);
    }
    FileDescriptor(const FileDescriptor &) = delete;
    FileDescriptor &operator=(const FileDescriptor &) = delete;
    int fd;
};

class SnapshotDirectory {
public:
#ifdef DB5_WIN32_EXPORT
    explicit SnapshotDirectory(const std::string &path) :
	handle(CreateFileW(std::filesystem::path(path).c_str(), GENERIC_WRITE,
		    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
		    FILE_FLAG_BACKUP_SEMANTICS, nullptr))
    {}
    SnapshotDirectory(const SnapshotDirectory &) = delete;
    SnapshotDirectory &operator=(const SnapshotDirectory &) = delete;
    ~SnapshotDirectory()
    {
	if (valid())
	    CloseHandle(handle);
    }
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
    bool synchronize() const { return FlushFileBuffers(handle) != 0; }

private:
    HANDLE handle;
#else
    explicit SnapshotDirectory(const std::string &path) : descriptor(open(path.c_str(), O_RDONLY)) {}
    bool valid() const { return descriptor.fd >= 0; }
    bool synchronize() const { return fsync(descriptor.fd) == 0; }

private:
    FileDescriptor descriptor;
#endif
};

class TemporarySnapshot {
public:
#ifdef DB5_WIN32_EXPORT
    explicit TemporarySnapshot(const std::string &directory, SECURITY_ATTRIBUTES *security = nullptr,
	DWORD attributes = FILE_ATTRIBUTE_NORMAL)
#else
    explicit TemporarySnapshot(const std::string &directory)
#endif
    {
#ifdef DB5_WIN32_EXPORT
	// The UUID API writes a 128-bit identifier and its nul-terminated text.
	uint8_t id[16];
	uint8_t suffix[37];
	if (bu_uuid_create(id, 0, nullptr, nullptr) < 0 || bu_uuid_encode(id, suffix) != 0)
	    return;
	path = directory + "/.brlcad-db5-" + reinterpret_cast<const char *>(suffix);
	native_path = path;
	const DWORD create_attributes = attributes & ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
	const HANDLE handle = CreateFileW(native_path.c_str(), GENERIC_WRITE,
	    0, security, CREATE_NEW, create_attributes ? create_attributes : FILE_ATTRIBUTE_NORMAL, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
	    return;
	created = true;
	if (attributes & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED) {
	    // CreateFileW does not set the indexing attribute on a new file.
	    FILE_BASIC_INFO update = {};
	    update.FileAttributes = attributes & ~FILE_ATTRIBUTE_ENCRYPTED;
	    if (!SetFileInformationByHandle(handle, FileBasicInfo, &update, sizeof(update))) {
		CloseHandle(handle);
		return;
	    }
	}
	FileDescriptor descriptor(_open_osfhandle(reinterpret_cast<intptr_t>(handle),
	    _O_WRONLY | _O_BINARY | _O_NOINHERIT));
	if (descriptor.fd < 0)
	    CloseHandle(handle);
#else
	path = directory + "/.brlcad-db5-XXXXXX";
	FileDescriptor descriptor(mkstemp(path.data()));
	created = descriptor.fd >= 0;
#endif
	if (descriptor.fd >= 0) {
	    file = fdopen(descriptor.fd, "wb");
	    if (file)
		descriptor.fd = -1;
	}
    }
    ~TemporarySnapshot()
    {
	if (file)
	    fclose(file);
	if (created) {
#ifdef DB5_WIN32_EXPORT
	    DeleteFileW(native_path.c_str());
#else
	    bu_file_delete(path.c_str());
#endif
	}
    }
    TemporarySnapshot(const TemporarySnapshot &) = delete;
    TemporarySnapshot &operator=(const TemporarySnapshot &) = delete;

#ifdef DB5_WIN32_EXPORT
    bool finish()
#else
    bool finish(const struct stat *original = nullptr)
#endif
    {
	if (fflush(file) != 0)
	    return false;
	const int fd = fileno(file);
#ifdef DB5_WIN32_EXPORT
	const auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
	if (handle == INVALID_HANDLE_VALUE || !FlushFileBuffers(handle))
	    return false;
#else
	if (original) {
	    struct stat staged;
	    if (fstat(fd, &staged) != 0)
		return false;
	    if ((staged.st_uid != original->st_uid || staged.st_gid != original->st_gid) &&
		fchown(fd, original->st_uid, original->st_gid) != 0)
		return false;
	    constexpr mode_t permission_bits = S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX;
	    if (bu_fchmod(fd, original->st_mode & permission_bits) != 0)
		return false;
	}
	if (fsync(fd) != 0)
	    return false;
#endif

	FILE *finished = file;
	file = nullptr;
	return fclose(finished) == 0;
    }

#ifdef DB5_WIN32_REPLACEMENT
    bool replace(const std::filesystem::path &destination)
    {
	const auto &name = destination.native();
	const size_t length = name.size() * sizeof(wchar_t);
	if (length > std::numeric_limits<DWORD>::max() - sizeof(FILE_RENAME_INFO))
	    return false;
	// The Win32 path conversion expects a trailing nul even though the
	// counted filename sent to the filesystem excludes it.
	const DWORD size = static_cast<DWORD>(sizeof(FILE_RENAME_INFO) + length);
	std::vector<uint8_t> buffer(size);
	FILE_RENAME_INFO rename = {};
	rename.Flags = FILE_RENAME_FLAG_POSIX_SEMANTICS | FILE_RENAME_FLAG_REPLACE_IF_EXISTS;
	rename.FileNameLength = static_cast<DWORD>(length);
	memcpy(buffer.data(), &rename, offsetof(FILE_RENAME_INFO, FileName));
	memcpy(buffer.data() + offsetof(FILE_RENAME_INFO, FileName), name.c_str(), length + sizeof(wchar_t));

	const HANDLE handle = CreateFileW(native_path.c_str(), DELETE,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	    FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
	    return false;
	const bool moved = SetFileInformationByHandle(handle, FileRenameInfoEx, buffer.data(), size) != 0;
	CloseHandle(handle);
	return moved;
    }
#endif

#ifdef DB5_WIN32_EXPORT
    bool publish_new(const std::filesystem::path &destination)
    {
	// A sibling rename without copying or replacing keeps a raced
	// destination intact and only exposes the complete snapshot.
	if (!MoveFileExW(native_path.c_str(), destination.c_str(), MOVEFILE_WRITE_THROUGH))
	    return false;
	created = false;
	return true;
    }
#endif

    std::string path;
    FILE *file = nullptr;
    bool created = false;
#ifdef DB5_WIN32_EXPORT
private:
    std::filesystem::path native_path;
#endif
};

#ifdef DB5_WIN32_EXPORT
bool
read_windows_state(HANDLE handle, SnapshotFileState &state)
{
    return handle != INVALID_HANDLE_VALUE && GetFileType(handle) == FILE_TYPE_DISK &&
	GetFileInformationByHandle(handle, &state.info) && !(state.info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
	GetFileInformationByHandleEx(handle, FileBasicInfo, &state.basic, sizeof(state.basic));
}

bool
read_snapshot_file_state(int descriptor, SnapshotFileState &state)
{
    return read_windows_state(reinterpret_cast<HANDLE>(_get_osfhandle(descriptor)), state);
}

bool
same_contents(const SnapshotFileState &before, const SnapshotFileState &after)
{
    const auto &original = before.info;
    const auto &current = after.info;
    return current.dwVolumeSerialNumber == original.dwVolumeSerialNumber &&
	current.nFileIndexHigh == original.nFileIndexHigh && current.nFileIndexLow == original.nFileIndexLow &&
	current.nFileSizeHigh == original.nFileSizeHigh && current.nFileSizeLow == original.nFileSizeLow &&
	current.dwFileAttributes == original.dwFileAttributes &&
	after.basic.LastWriteTime.QuadPart == before.basic.LastWriteTime.QuadPart;
}

bool
same_state(const SnapshotFileState &before, const SnapshotFileState &after)
{
    return same_contents(before, after) && after.info.nNumberOfLinks == before.info.nNumberOfLinks &&
	after.basic.ChangeTime.QuadPart == before.basic.ChangeTime.QuadPart;
}
#else
bool
read_snapshot_file_state(int descriptor, SnapshotFileState &state)
{
    return fstat(descriptor, &state) == 0 && S_ISREG(state.st_mode);
}

bool
same_write_time(const struct stat &original, const struct stat &current)
{
#if defined(__APPLE__) && !defined(st_mtime)
    return current.st_mtime == original.st_mtime && current.st_mtimensec == original.st_mtimensec;
#elif defined(__APPLE__)
    return current.st_mtime == original.st_mtime &&
	current.st_mtimespec.tv_nsec == original.st_mtimespec.tv_nsec;
#else
    return current.st_mtime == original.st_mtime && current.st_mtim.tv_nsec == original.st_mtim.tv_nsec;
#endif
}

bool
same_contents(const struct stat &original, const struct stat &current)
{
    return S_ISREG(current.st_mode) && current.st_dev == original.st_dev && current.st_ino == original.st_ino &&
	current.st_size == original.st_size && current.st_mode == original.st_mode &&
	current.st_uid == original.st_uid && current.st_gid == original.st_gid &&
	same_write_time(original, current);
}

bool
same_state(const struct stat &original, const struct stat &current)
{
    return same_contents(original, current) && current.st_nlink == original.st_nlink &&
	current.st_ctime == original.st_ctime;
}
#endif

#ifdef DB5_WIN32_REPLACEMENT
bool
unchanged(const struct db5_save *save, int descriptor)
{
    SnapshotFileState visible;
    const auto path = std::filesystem::path(save->path);
    const HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
	FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
	return false;
    const bool identified = read_windows_state(handle, visible);
    CloseHandle(handle);
    if (!identified || !same_state(save->original, visible))
	return false;
    SnapshotFileState opened;
    return descriptor < 0 || (read_snapshot_file_state(descriptor, opened) && same_state(save->original, opened));
}

bool
read_snapshot_permissions(FILE *file, std::vector<uint8_t> &security)
{
    const HANDLE handle = reinterpret_cast<HANDLE>(_get_osfhandle(fileno(file)));
    const SECURITY_INFORMATION permissions = OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION |
	DACL_SECURITY_INFORMATION;
    DWORD length = 0;
    if (GetKernelObjectSecurity(handle, permissions, nullptr, 0, &length) ||
	GetLastError() != ERROR_INSUFFICIENT_BUFFER || !length)
	return false;
    security.resize(length);
    return GetKernelObjectSecurity(handle, permissions, security.data(), length, &length) != 0;
}
#elif defined(DB5_POSIX_REPLACEMENT)
bool
unchanged(const struct db5_save *save, int descriptor)
{
    struct stat path_state;
    if (lstat(save->path.c_str(), &path_state) != 0 || !same_state(save->original, path_state))
	return false;
    struct stat fd_state;
    if (descriptor >= 0 && (fstat(descriptor, &fd_state) != 0 || !same_state(save->original, fd_state)))
	return false;
    std::error_code error;
    const auto modified = std::filesystem::last_write_time(save->path, error);
    return !error && modified == save->modified;
}
#endif

bool
write_snapshot(FILE *file, const struct db_i *snapshot, const db_history::Pack *pack,
	      bool omit_archive, struct bu_vls *message, const db_history::SnapshotRecordReader &reader = {})
{
    db_history::Manifest expected;
    if (pack && !db_history::selected_manifest(*pack, expected)) {
	db5_save_message(message, "History does not contain a usable selected model");
	return false;
    }
    if (db5_fwrite_header(file) < 0)
	return false;

    db_history::Manifest actual;
    bool global = false;
    struct directory *dp;
    FOR_ALL_DIRECTORY_START(dp, snapshot) {
	if (!(dp->d_flags & RT_DIR_INMEM) && dp->d_addr == RT_DIR_PHONY_ADDR)
	    continue;

	struct bu_external record = BU_EXTERNAL_INIT_ZERO;
	if (reader ? !reader(dp, &record) : db_get_external(&record, dp, snapshot) < 0)
	    return false;

	struct db5_raw_internal raw;
	const auto *end = db5_get_raw_internal_bounded(&raw, record.ext_buf, record.ext_nbytes);
	bool valid = end && raw.object_length == record.ext_nbytes && raw.h_name_present &&
	    raw.h_dli == DB5HDR_HFLAGS_DLI_APPLICATION_DATA_OBJECT &&
	    BU_STR_EQUAL(dp->d_namep, reinterpret_cast<const char *>(raw.name.ext_buf));
	if (valid && BU_STR_EQUAL(dp->d_namep, DB5_GLOBAL_OBJECT_NAME)) {
	    valid = !global && raw.major_type == DB5_MAJORTYPE_ATTRIBUTE_ONLY;
	    global = valid;
	}
	bool skip = false;
	if (valid && (pack || omit_archive) && BU_STR_EQUAL(dp->d_namep, db_history::archive_name)) {
	    db_history::ByteView body;
	    skip = db_history::archive_payload(raw, body);
	    if (!skip && pack) {
		db5_save_message(message, "A user object occupies the embedded history name");
		valid = false;
	    }
	}
	bool written = valid;
	if (valid && !skip) {
	    written = !pack || db_history::add_model_record(actual, dp->d_namep, {record.ext_buf, record.ext_nbytes});
	    if (written)
		written = bu_fwrite_external(file, &record) == 0;
	}
	bu_free_external(&record);
	if (!written)
	    return false;
    } FOR_ALL_DIRECTORY_END;
    if (!global)
	return false;
    if (pack && expected != actual) {
	db5_save_message(message, "The selected archive model differs from the DB5 snapshot");
	return false;
    }
    return !pack || db_history::write_embedded_pack(file, *pack);
}

bool
write_archived_snapshot(FILE *file, const db_history::Pack &pack, const db_history::Manifest &model)
{
    using namespace db_history;
    if (db5_fwrite_header(file) < 0)
	return false;

    Bytes record;
    for (const auto &entry : model) {
	if (!read_archived_record(pack, entry.second, record))
	    return false;
	struct bu_external external = BU_EXTERNAL_INIT_ZERO;
	external.ext_buf = record.data();
	external.ext_nbytes = record.size();
	if (bu_fwrite_external(file, &external) != 0)
	    return false;
    }
    return write_embedded_pack(file, pack);
}

std::string
snapshot_directory(const char *path)
{
    char *parent = bu_path_dirname(path);
    std::string directory(parent);
    bu_free(parent, "DB5 save directory");
    return directory;
}

using OpenSnapshot = std::unique_ptr<struct db_i, decltype(&db_close)>;

OpenSnapshot
open_staged_snapshot(const TemporarySnapshot &temporary, const struct db_i *source, bool writable)
{
    OpenSnapshot staged(db_open(temporary.path.c_str(), writable ? DB_OPEN_READWRITE : DB_OPEN_READONLY), db_close);
    if (!staged || (writable && staged->dbi_read_only))
	return {nullptr, db_close};
    staged->i->dbi_use_comb_instance_ids = db_uses_instance_specifiers(source);
    if (db_dirbuild(staged.get()) != 0)
	staged.reset();
    return staged;
}

} // namespace
#endif

struct db5_save *
db5_save_begin(const struct db_i *base, struct bu_vls *message)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_REPLACEMENT
    if (!base || !base->dbi_filename || db_version(base) != 5 || !base->i->dbi_fp ||
	base->i->dbi_mf || !base->i->dbi_dir_built) {
	db5_save_message(message, "A built DB5 database opened for writing is required");
	return nullptr;
    }

    auto save = std::make_unique<struct db5_save>();
    save->base = base;
    char *path = bu_file_realpath(base->dbi_filename, nullptr);
    if (path) {
	save->path = path;
	bu_free(path, "DB5 save path");
    }
    const int fd = fileno(base->i->dbi_fp);
#ifdef DB5_WIN32_REPLACEMENT
    if (!read_snapshot_file_state(fd, save->original) || save->original.info.nNumberOfLinks != 1 ||
	(save->original.info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || !unchanged(save.get(), fd)) {
#else
    std::error_code error;
    save->modified = std::filesystem::last_write_time(save->path, error);
    if (error || fstat(fd, &save->original) != 0 || save->original.st_nlink != 1 || !unchanged(save.get(), fd)) {
#endif
	db5_save_message(message, "Could not identify an unchanged single-link DB5 file");
	return nullptr;
    }
    return save.release();
#else
    (void)base;
    db5_save_message(message, "Atomic DB5 replacement is unavailable on this platform");
    return nullptr;
#endif
}

static enum db5_save_result
replace_snapshot(struct db5_save *save, const struct db_i *snapshot, const db_history::Pack *pack,
		 struct db_i **replacement, struct bu_vls *message)
{
    db5_save_message(message, "");
    if (replacement)
	*replacement = DBI_NULL;
#ifdef DB5_ATOMIC_REPLACEMENT
    if (!save || save->installed || !snapshot || db_version(snapshot) != 5 || !replacement) {
	db5_save_message(message, "Invalid DB5 save or snapshot");
	return DB5_SAVE_ERROR;
    }
    if (!unchanged(save, -1)) {
	db5_save_message(message, "DB5 file changed since staging began");
	return DB5_SAVE_STALE;
    }
    if (pack) {
	const auto existing = db_history::inspect_db5_history(save->base);
	if (existing == db_history::EmbeddedStatus::Collision) {
	    db5_save_message(message, "A user object occupies the embedded history name");
	    return DB5_SAVE_ERROR;
	}
	if (existing != db_history::EmbeddedStatus::Current && existing != db_history::EmbeddedStatus::Missing) {
	    db5_save_message(message, "Cannot replace unreadable or unsupported embedded history");
	    return DB5_SAVE_ERROR;
	}
    }

    const std::string directory = snapshot_directory(save->path.c_str());
#ifdef DB5_WIN32_REPLACEMENT
    std::vector<uint8_t> descriptor;
    if (!read_snapshot_permissions(save->base->i->dbi_fp, descriptor)) {
	db5_save_message(message, "Could not read the permissions of the DB5 file");
	return DB5_SAVE_ERROR;
    }
    SECURITY_ATTRIBUTES security = {sizeof(SECURITY_ATTRIBUTES), descriptor.data(), FALSE};
    constexpr DWORD copy_attributes = FILE_ATTRIBUTE_ENCRYPTED | FILE_ATTRIBUTE_HIDDEN |
	FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    const DWORD attributes = save->original.info.dwFileAttributes & copy_attributes;
    TemporarySnapshot temporary(directory, &security, attributes ? attributes : FILE_ATTRIBUTE_NORMAL);
#else
    TemporarySnapshot temporary(directory);
#endif
    // The write must be complete and the new database usable before
    // replacing the only directory entry for the original file.
    if (!temporary.file || !write_snapshot(temporary.file, snapshot, pack, false, message) ||
#ifdef DB5_WIN32_REPLACEMENT
	!temporary.finish()) {
#else
	!temporary.finish(&save->original)) {
#endif
	if (message && bu_vls_strlen(message) == 0)
	    db5_save_message(message, "Could not write and synchronize the DB5 snapshot");
	return DB5_SAVE_ERROR;
    }

    auto staged = open_staged_snapshot(temporary, snapshot, true);
    if (!staged) {
	db5_save_message(message, "Could not reopen the staged DB5 snapshot");
	return DB5_SAVE_ERROR;
    }

    // Cooperating writers lock the inode then verify the directory entry
    // still names it. Each save opens its own descriptor for this lock.
#ifdef DB5_WIN32_REPLACEMENT
    db_history::FileLock existing(std::filesystem::path(save->path));
    if (!existing.acquire(db_history::FileLockMode::TryExclusive)) {
#else
    FileDescriptor existing(open(save->path.c_str(), O_RDONLY));
    if (existing.fd < 0 || flock(existing.fd, LOCK_EX | LOCK_NB) != 0) {
#endif
	db5_save_message(message, "Could not lock the destination DB5 file");
	return DB5_SAVE_ERROR;
    }
#ifdef DB5_WIN32_REPLACEMENT
    if (!unchanged(save, fileno(save->base->i->dbi_fp))) {
#else
    if (!unchanged(save, existing.fd)) {
#endif
	db5_save_message(message, "DB5 file changed during save");
	return DB5_SAVE_STALE;
    }
    const SnapshotDirectory parent(directory);
    if (!parent.valid()) {
	db5_save_message(message, "Could not open the database directory for synchronization");
	return DB5_SAVE_ERROR;
    }

    bu_free(staged->dbi_filename, "staged filename");
    staged->dbi_filename = bu_strdup(save->path.c_str());
#ifdef DB5_WIN32_REPLACEMENT
    if (!temporary.replace(std::filesystem::path(save->path))) {
#else
    std::error_code error;
    std::filesystem::rename(temporary.path, save->path, error);
    if (error) {
#endif
	db5_save_message(message, "Could not replace the DB5 file");
	return DB5_SAVE_ERROR;
    }

    temporary.created = false;
    save->installed = true;
    *replacement = staged.release();
    if (!parent.synchronize()) {
	db5_save_message(message, "DB5 file replaced but directory synchronization failed");
	return DB5_SAVE_SYNC_FAILED;
    }
    return DB5_SAVE_OK;
#else
    (void)save;
    (void)snapshot;
    (void)pack;
    db5_save_message(message, "Atomic DB5 replacement is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

enum db5_save_result
db5_save_replace(struct db5_save *save, const struct db_i *snapshot,
		 struct db_i **replacement, struct bu_vls *message)
{
    return replace_snapshot(save, snapshot, nullptr, replacement, message);
}

enum db5_save_result
db_history::save_db5_history(struct db5_save *save, const struct db_i *snapshot, const Pack &pack,
			    struct db_i **replacement, struct bu_vls *message)
{
    return replace_snapshot(save, snapshot, &pack, replacement, message);
}

bool
db_history::db5_history_save_is_current(const struct db5_save *save)
{
#ifdef DB5_ATOMIC_REPLACEMENT
    return save && !save->installed && unchanged(save, -1);
#else
    (void)save;
    return false;
#endif
}

#ifdef DB5_ATOMIC_EXPORT
enum class ExportFormat { DB5, Binary };

template <typename Writer>
static enum db5_save_result
publish_export(const struct db_i *snapshot, const char *destination, struct bu_vls *message, Writer write,
	       ExportFormat format = ExportFormat::DB5)
{
    if (!destination || !*destination) {
	db5_save_message(message, "A new export path is required");
	return DB5_SAVE_ERROR;
    }
    const std::string directory = snapshot_directory(destination);
#ifdef DB5_WIN32_EXPORT
    const std::filesystem::path native_destination(destination);
    if (GetFileAttributesW(native_destination.c_str()) != INVALID_FILE_ATTRIBUTES) {
	db5_save_message(message, "The export destination already exists");
	return DB5_SAVE_ERROR;
    }
#else
    struct stat existing;
    if (lstat(destination, &existing) == 0) {
	db5_save_message(message, "The export destination already exists");
	return DB5_SAVE_ERROR;
    }
#endif
    const SnapshotDirectory parent(directory);
    if (!parent.valid()) {
	db5_save_message(message, "Could not open the export directory for synchronization");
	return DB5_SAVE_ERROR;
    }

    const int source_fd = snapshot && snapshot->i->dbi_fp ? fileno(snapshot->i->dbi_fp) : -1;
    SnapshotFileState original;
    if (source_fd >= 0 && !read_snapshot_file_state(source_fd, original)) {
	db5_save_message(message, "Could not identify the source DB5 file");
	return DB5_SAVE_ERROR;
    }
#ifndef DB5_WIN32_EXPORT
    std::error_code error;
    std::filesystem::file_time_type source_modified;
    bool track_source_path = false;
    if (source_fd >= 0 && snapshot->dbi_filename) {
	struct stat visible;
	if (stat(snapshot->dbi_filename, &visible) == 0 && same_state(original, visible)) {
	    source_modified = std::filesystem::last_write_time(snapshot->dbi_filename, error);
	    track_source_path = !error;
	}
    }
#endif
    TemporarySnapshot temporary(directory);
    if (!temporary.file || !write(temporary.file) || !temporary.finish()) {
	if (message && !bu_vls_strlen(message))
	    db5_save_message(message, "Could not write and synchronize the export");
	return DB5_SAVE_ERROR;
    }
    if (format == ExportFormat::DB5 && !open_staged_snapshot(temporary, snapshot, false)) {
	db5_save_message(message, "Could not reopen the staged DB5 export");
	return DB5_SAVE_ERROR;
    }
    SnapshotFileState current;
    if (source_fd >= 0 && (!read_snapshot_file_state(source_fd, current) || !same_state(original, current))) {
	db5_save_message(message, "The source database changed during export");
	return DB5_SAVE_STALE;
    }
#ifdef DB5_WIN32_EXPORT
    if (!temporary.publish_new(native_destination)) {
	db5_save_message(message, "Could not publish the export to a new path");
	return DB5_SAVE_ERROR;
    }
#else
    if (track_source_path) {
	const auto modified = std::filesystem::last_write_time(snapshot->dbi_filename, error);
	if (error || modified != source_modified) {
	    db5_save_message(message, "The source database changed during export");
	    return DB5_SAVE_STALE;
	}
    }

    // Linking makes the fully written file visible and refuses a raced
    // destination. The sibling temporary name is removed afterwards.
    std::filesystem::create_hard_link(temporary.path, destination, error);
    if (error) {
	db5_save_message(message, "Could not publish the export to a new path");
	return DB5_SAVE_ERROR;
    }
    temporary.created = false;
    if (!bu_file_remove(temporary.path.c_str())) {
	db5_save_message(message, "Export published but its temporary name could not be removed");
	return DB5_SAVE_SYNC_FAILED;
    }
#endif
    if (!parent.synchronize()) {
	db5_save_message(message, "Export published but directory synchronization failed");
	return DB5_SAVE_SYNC_FAILED;
    }
    return DB5_SAVE_OK;
}
#endif

enum db5_save_result
db5_export_binunif(const struct db_i *snapshot, const char *name, const char *destination, struct bu_vls *message)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_EXPORT
    if (!snapshot || db_version(snapshot) != 5 || !name || !*name) {
	db5_save_message(message, "A DB5 model and binary object name are required");
	return DB5_SAVE_ERROR;
    }
    const auto *object = db_lookup(snapshot, name, LOOKUP_QUIET);
    if (!object || object->d_major_type != DB5_MAJORTYPE_BINARY_UNIF) {
	db5_save_message(message, "The source must name a uniform binary object");
	return DB5_SAVE_ERROR;
    }
    return publish_export(snapshot, destination, message, [&](FILE *file) {
	struct rt_db_internal internal = RT_DB_INTERNAL_INIT_ZERO;
	const std::unique_ptr<struct rt_db_internal, decltype(&rt_db_free_internal)> data(&internal, rt_db_free_internal);
	if (rt_db_get_internal(&internal, object, snapshot, nullptr) != ID_BINUNIF) {
	    db5_save_message(message, "Could not read the binary object");
	    return false;
	}
	const auto *binary = static_cast<const struct rt_binunif_internal *>(internal.idb_ptr);
	const size_t width = binary ? db5_type_sizeof_h_binu(binary->type) : 0;
	if (!width || !binary->u.uint8 || !binary->count || binary->count > SIZE_MAX / width) {
	    db5_save_message(message, "The binary object has no usable contents");
	    return false;
	}
	return fwrite(binary->u.uint8, width, binary->count, file) == binary->count;
    }, ExportFormat::Binary);
#else
    (void)snapshot;
    (void)name;
    (void)destination;
    db5_save_message(message, "Atomic binary export is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

enum db5_save_result
db_history::export_db5_snapshot(const struct db_i *snapshot, const char *destination,
			      const Pack *pack, struct bu_vls *message)
{
    return export_db5_snapshot(snapshot, destination, pack, message, {});
}

enum db5_save_result
db_history::export_db5_snapshot(const struct db_i *snapshot, const char *destination,
			       const Pack *pack, struct bu_vls *message, const SnapshotRecordReader &reader)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_EXPORT
    if (!snapshot || db_version(snapshot) != 5) {
	db5_save_message(message, "A DB5 model is required for export");
	return DB5_SAVE_ERROR;
    }
    return publish_export(snapshot, destination, message, [&](FILE *file) {
	return write_snapshot(file, snapshot, pack, true, message, reader);
    });
#else
    (void)snapshot;
    (void)destination;
    (void)pack;
    (void)reader;
    db5_save_message(message, "Atomic DB5 export is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

enum db5_save_result
db_history::export_db5_archive(const Pack &pack, const char *destination, struct bu_vls *message)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_EXPORT
    return publish_export(nullptr, destination, message, [&](FILE *file) {
	Manifest model;
	if (!selected_manifest(pack, model) || !verify_pack(pack, message))
	    return false;
	return write_archived_snapshot(file, pack, model);
    });
#else
    (void)pack;
    (void)destination;
    db5_save_message(message, "Atomic DB5 export is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

enum db5_save_result
db_history::export_db5_conflict(const struct db5_save *save, const struct db_i *snapshot,
			       const char *destination, struct bu_vls *message)
{
    db5_save_message(message, "");
#ifdef DB5_ATOMIC_REPLACEMENT
    if (!save || save->installed || !snapshot || db_version(snapshot) != 5) {
	db5_save_message(message, "The staged DB5 view is unavailable");
	return DB5_SAVE_ERROR;
    }
    const int fd = fileno(save->base->i->dbi_fp);
    SnapshotFileState base = {};
    const bool identified = read_snapshot_file_state(fd, base);
    // Replacing the pathname can unlink the old file while its descriptor
    // still supplies the original records. Only allow that metadata change.
#ifdef DB5_WIN32_REPLACEMENT
    const bool detached = base.info.nNumberOfLinks == 0;
#else
    const bool detached = base.st_nlink == 0;
#endif

    if (!identified || (!same_state(save->original, base) && !(detached && same_contents(save->original, base)))) {
	db5_save_message(message, "The original file contents changed. The staged session model cannot be recovered from them.");
	return DB5_SAVE_STALE;
    }
    return publish_export(save->base, destination, message, [&](FILE *file) {
	SnapshotFileState current;
	if (!read_snapshot_file_state(fd, current) || !same_state(base, current)) {
	    db5_save_message(message, "The original DB5 file changed during recovery");
	    return false;
	}
	return write_snapshot(file, snapshot, nullptr, true, message);
    });
#else
    (void)save;
    (void)snapshot;
    (void)destination;
    db5_save_message(message, "Atomic DB5 export is unavailable on this platform");
    return DB5_SAVE_ERROR;
#endif
}

void
db5_save_free(struct db5_save *save)
{
    delete save;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
