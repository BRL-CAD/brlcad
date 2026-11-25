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

#if defined(DB_HISTORY_WIN32_FILE_LOCKS) || (defined(HAVE_FSYNC) && defined(HAVE_UNISTD_H))
#  define DB_HISTORY_DIRECTORY_SYNC 1
#endif

namespace db_history {

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
