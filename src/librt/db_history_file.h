/*                   D B _ H I S T O R Y _ F I L E . H
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

#ifndef LIBRT_DB_HISTORY_FILE_H
#define LIBRT_DB_HISTORY_FILE_H

#include "common.h"

#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>

#include "bio.h"

#if defined(_WIN32) && defined(HAVE_WINDOWS_H) && !defined(__CYGWIN__) && !defined(__MSYS__)
#  define DB_HISTORY_WIN32_FILE_LOCKS 1
#  define DB_HISTORY_FILE_LOCKS 1
#elif defined(HAVE_SYS_FILE_H) && defined(HAVE_UNISTD_H)
#  include <sys/file.h>
#  if defined(LOCK_SH) && defined(LOCK_EX) && defined(LOCK_NB) && defined(O_CLOEXEC) && defined(O_NOFOLLOW)
#    define DB_HISTORY_FILE_LOCKS 1
#  endif
#endif

#ifndef DB_HISTORY_WIN32_FILE_LOCKS
#  include <sys/stat.h>
#endif

#if defined(DB_HISTORY_WIN32_FILE_LOCKS) || (defined(HAVE_FSYNC) && defined(HAVE_UNISTD_H))
#  define DB_HISTORY_DIRECTORY_SYNC 1
#endif

namespace db_history {

struct FileIdentity {
    uint64_t device;
    uint64_t index;

    bool operator==(const FileIdentity &other) const
    {
	return device == other.device && index == other.index;
    }

    bool operator!=(const FileIdentity &other) const { return !(*this == other); }
};

#ifdef DB_HISTORY_WIN32_FILE_LOCKS
inline FileIdentity
file_identity(const BY_HANDLE_FILE_INFORMATION &info)
{
    const auto index = (static_cast<uint64_t>(info.nFileIndexHigh) << std::numeric_limits<DWORD>::digits) |
	info.nFileIndexLow;
    return {info.dwVolumeSerialNumber, index};
}

inline std::optional<FileIdentity>
file_identity(HANDLE handle)
{
    BY_HANDLE_FILE_INFORMATION info;
    if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info) ||
	(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
	info.nNumberOfLinks != 1)
	return {};
    return file_identity(info);
}
#else
inline std::optional<FileIdentity>
file_identity(const struct stat &info)
{
    if (!S_ISREG(info.st_mode) || info.st_nlink != 1)
	return {};
    return FileIdentity{static_cast<uint64_t>(info.st_dev), static_cast<uint64_t>(info.st_ino)};
}

inline std::optional<FileIdentity>
file_identity(int fd)
{
    struct stat info;
    return fstat(fd, &info) == 0 ? file_identity(info) : std::nullopt;
}
#endif

// A missing file is distinct from one we cannot safely identify. On
// POSIX, never open and close an extra descriptor to LMDB's lock file.
inline bool
file_identity(const std::filesystem::path &path, std::optional<FileIdentity> &id)
{
    id.reset();
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
    const HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
	FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
	const auto error = GetLastError();
	return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    id = file_identity(handle);
    CloseHandle(handle);
#else
    struct stat info;
    if (lstat(path.c_str(), &info) != 0)
	return errno == ENOENT;
    id = file_identity(info);
#endif
    return id.has_value();
}

#ifdef DB_HISTORY_FILE_LOCKS
enum class FileLockMode { Shared, TryExclusive };

class FileLock {
public:
    explicit FileLock(const std::filesystem::path &path, bool create = false)
    {
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	// LMDB opens data.mdb for ordinary reads and writes while this lock is held.
	handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
	    nullptr, create ? OPEN_ALWAYS : OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (handle == INVALID_HANDLE_VALUE)
	    return;
	BY_HANDLE_FILE_INFORMATION info;
	if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info) ||
	    (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY))) {
	    CloseHandle(handle);
	    handle = INVALID_HANDLE_VALUE;
	}
#else
	const int access = O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK;
	fd = create ? open(path.c_str(), access | O_CREAT, 0660) : open(path.c_str(), access);
#endif
    }

    FileLock(const FileLock &) = delete;
    FileLock &operator=(const FileLock &) = delete;

    ~FileLock()
    {
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	if (handle == INVALID_HANDLE_VALUE)
	    return;
	if (locked) {
	    auto range = lock_range();
	    (void)UnlockFileEx(handle, 0, lock_length, 0, &range);
	}
	CloseHandle(handle);
#else
	if (fd >= 0)
	    close(fd);
#endif
    }

    bool acquire(FileLockMode mode)
    {
	contended = false;
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	if (handle == INVALID_HANDLE_VALUE)
	    return false;
	const DWORD flags = mode == FileLockMode::Shared ? 0 : LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY;
	auto range = lock_range();
	locked = LockFileEx(handle, flags, 0, lock_length, 0, &range) != 0;
	if (!locked)
	    contended = GetLastError() == ERROR_LOCK_VIOLATION;
	return locked;
#else
	if (fd < 0)
	    return false;
	const int flags = mode == FileLockMode::Shared ? LOCK_SH : LOCK_EX | LOCK_NB;
	int status;
	do {
	    status = flock(fd, flags);
	} while (status && errno == EINTR);
	contended = status && (errno == EAGAIN || errno == EWOULDBLOCK);
	return status == 0;
#endif
    }

    bool busy() const { return contended; }

    bool refers_to(const FileIdentity &id) const
    {
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	return file_identity(handle) == id;
#else
	return file_identity(fd) == id;
#endif
    }

private:
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
    static OVERLAPPED lock_range()
    {
	// Byte-range locks affect ordinary I/O. Reserve a byte beyond any
	// supported database size so LMDB can write through a separate handle.
	constexpr uint64_t offset = std::numeric_limits<int64_t>::max() - lock_length;
	OVERLAPPED range = {};
	range.Offset = static_cast<DWORD>(offset);
	range.OffsetHigh = static_cast<DWORD>(offset >> std::numeric_limits<DWORD>::digits);
	return range;
    }

    static constexpr DWORD lock_length = 1;
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool locked = false;
#else
    int fd = -1;
#endif
    bool contended = false;
};
#endif

#ifdef DB_HISTORY_DIRECTORY_SYNC
class DirectorySync {
public:
    explicit DirectorySync(const std::filesystem::path &path)
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	: handle(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		    nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr))
#else
	: fd(open(path.c_str(), O_RDONLY))
#endif
    {}

    DirectorySync(const DirectorySync &) = delete;
    DirectorySync &operator=(const DirectorySync &) = delete;

    ~DirectorySync()
    {
	if (!valid())
	    return;
#ifdef DB_HISTORY_WIN32_FILE_LOCKS
	CloseHandle(handle);
#else
	close(fd);
#endif
    }

#ifdef DB_HISTORY_WIN32_FILE_LOCKS
    bool valid() const { return handle != INVALID_HANDLE_VALUE; }
    bool synchronize() const { return FlushFileBuffers(handle) != 0; }

    bool refers_to(const std::filesystem::path &path) const
    {
	BY_HANDLE_FILE_INFORMATION original, visible;
	if (!GetFileInformationByHandle(handle, &original))
	    return false;
	const auto named = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES,
	    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
	    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
	if (named == INVALID_HANDLE_VALUE)
	    return false;
	const bool same = GetFileInformationByHandle(named, &visible) &&
	    (visible.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
	    !(visible.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
	    file_identity(original) == file_identity(visible);
	CloseHandle(named);
	return same;
    }

private:
    HANDLE handle;
#else
    bool valid() const { return fd >= 0; }
    bool synchronize() const
    {
	int status;
	do {
	    status = fsync(fd);
	} while (status && errno == EINTR);
	return status == 0;
    }

    bool refers_to(const std::filesystem::path &path) const
    {
	struct stat original, visible;
	return fstat(fd, &original) == 0 && lstat(path.c_str(), &visible) == 0 &&
	    S_ISDIR(visible.st_mode) && original.st_dev == visible.st_dev && original.st_ino == visible.st_ino;
    }

private:
    int fd;
#endif
};
#endif

} // namespace db_history

#endif /* LIBRT_DB_HISTORY_FILE_H */

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
