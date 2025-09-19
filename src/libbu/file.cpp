/*                      F I L E . C P P
 * BRL-CAD
 *
 * Copyright (c) 2004-2025 United States Government as represented by
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

#include <chrono>
#include <filesystem>
#include <system_error>

#include <string.h>
#include <ctype.h>
#ifdef HAVE_SYS_TYPES_H
#  include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif
#ifdef HAVE_PWD_H
#  include <pwd.h>
#endif
#ifdef HAVE_GRP_H
#  include <grp.h>
#endif

#include "bio.h"

#include "bu/app.h"
#include "bu/debug.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/parallel.h"
#include "bu/str.h"

#ifndef R_OK
#  define R_OK 4
#endif
#ifndef W_OK
#  define W_OK 2
#endif
#ifndef X_OK
#  define X_OK 1
#endif


int
bu_file_exists(const char *path, int *fd)
{
    struct stat sbuf;

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("Does [%s] exist? ", path);
    }

    if (!path || path[0] == '\0') {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("NO\n");
	}
	/* FAIL */
	return 0;
    }

    /* stdin is special case */
    if (BU_STR_EQUAL(path, "-")) {
	if (fd) {
	    *fd = fileno(stdin);
	}
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("YES\n");
	}
	return 1;
    }

    /* capture file descriptor if requested */
    if (fd) {
	*fd = open(path, O_RDONLY);
    }

    /* does it exist as a filesystem entity? */
    if (stat(path, &sbuf) == 0) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("YES\n");
	}
	/* OK */
	return 1;
    }

    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	bu_log("NO\n");
    }
    /* FAIL */
    return 0;
}

long long
bu_file_size(const char *path)
{
    long long fbytes = 0;
    if (!bu_file_exists(path, NULL))
	return -1;

#ifdef HAVE_SYS_STAT_H
    {
	struct stat sbuf;
	int ret = 0;
	int fd = open(path, O_RDONLY | O_BINARY);
	if (UNLIKELY(fd < 0)) {
	    return -1;
	}
	bu_semaphore_acquire(BU_SEM_SYSCALL);
	ret = fstat(fd, &sbuf);
	bu_semaphore_release(BU_SEM_SYSCALL);
	close(fd);
	if (UNLIKELY(ret < 0)) {
	    return -1;
	}
	fbytes = sbuf.st_size;
    }
#else
    {
	char buf[32768] = {0};
	FILE *fp = fopen(path, "rb");
	if (UNLIKELY(fp == NULL)) {
	    return NULL;
	}
	int got;
	fbytes = 0;

	bu_semaphore_acquire(BU_SEM_SYSCALL);
	while ((got = fread(buf, 1, sizeof(buf), fp)) > 0) {
	    fbytes += got;
	}
	fclose(fp);
	bu_semaphore_release(BU_SEM_SYSCALL);
	if (UNLIKELY(fbytes == 0)) {
	    return NULL;
	}
    }
#endif
    return fbytes;
}

// TODO - once C++20 is implemented and well supported, use that
// instead: https://stackoverflow.com/q/61030383
int64_t
bu_file_timestamp(const char *path)
{
    if (!path)
	return -1;

#ifdef HAVE_WINDOWS_H

    HANDLE h = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    FILETIME ft_lastwrite;
    int fret = GetFileTime(h, NULL, NULL, &ft_lastwrite);
    CloseHandle(h);
    if (!fret)
	return -1;

    // FILETIME is number of 100-nanosecond intervals since Jan 1, 1601.  For compatibility
    // with bu_gettime(), we need the number of microseconds since the UNIX epoch.

    // First convert FILETIME to a 64 bit integer
    ULARGE_INTEGER li;
    li.LowPart = ft_lastwrite.dwLowDateTime;
    li.HighPart = ft_lastwrite.dwHighDateTime;
    uint64_t fileTime100ns = li.QuadPart;

    // Number of 100-nanosecond intervals between 1601-01-01 and 1970-01-01
    const uint64_t EPOCH_DIFFERENCE_100NS = 116444736000000000ULL;

    // Correct for epoch difference
    uint64_t unixTime100ns = fileTime100ns - EPOCH_DIFFERENCE_100NS;

    // Convert to microseconds
    int64_t timestamp = static_cast<int64_t>(unixTime100ns / 10);

    return timestamp;

#elif HAVE_SYS_STAT_H

    struct stat sb;
    if (stat(path, &sb) != 0)
	return 0;
    long long sec_micro = sb.st_mtim.tv_sec * 1000000;
    long long nano_micro = sb.st_mtim.tv_nsec / 1000;
    int64_t timestamp = (int64_t)sec_micro + (int64_t)nano_micro;
    return timestamp;

#else

    // Currently unsupported
    return 0;

#endif
}

#ifdef HAVE_GETFULLPATHNAME
static int
file_compare_info(HANDLE handle1, HANDLE handle2)
{
    BOOL got1 = FALSE, got2 = FALSE;
    BY_HANDLE_FILE_INFORMATION file_info1, file_info2;

    if (handle1 != INVALID_HANDLE_VALUE) {
	got1 = GetFileInformationByHandle(handle1, &file_info1);
	CloseHandle(handle1);
    }

    if (handle2 != INVALID_HANDLE_VALUE) {
	got2 = GetFileInformationByHandle(handle2, &file_info2);
	CloseHandle(handle2);
    }

    /* On Windows, test if paths map to the same space on disk. */
    if (got1 && got2
	&& (file_info1.dwVolumeSerialNumber == file_info2.dwVolumeSerialNumber)
	&& (file_info1.nFileIndexLow == file_info2.nFileIndexLow)
	&& (file_info1.nFileIndexHigh = file_info2.nFileIndexHigh))
    {
	return 1;
    }

    return 0;
}
#endif /* HAVE_GETFULLPATHNAME */


int
bu_file_same(const char *fn1, const char *fn2)
{
    int ret = 0;
    char *rp1 = NULL;
    char *rp2 = NULL;

    if (UNLIKELY(!fn1 || !fn2)) {
	return 0;
    }

    if (UNLIKELY(fn1[0] == '\0' || fn2[0] == '\0')) {
	return 0;
    }

    /* stdin is a special case */
    if (BU_STR_EQUAL(fn1, fn2) && BU_STR_EQUAL(fn1, "-")) {
	return 1;
    }

    if (!bu_file_exists(fn1, NULL) || !bu_file_exists(fn2, NULL)) {
	return 0;
    }

    /* make sure symlinks to the same file report as same */
    rp1 = bu_file_realpath(fn1, NULL);
    rp2 = bu_file_realpath(fn2, NULL);

    /* pretend identical paths could be tested atomically.  same name
     * implies they're the same even if lookups would be different on
     * a fast-changing filesystem..
     */
    if (BU_STR_EQUAL(rp1, rp2)) {
	return 1;
    }

    /* Have different paths, so check whether they are the same thing
     * on disk.  Testing this is platform and filesystem-specific.
     */

    {

	/* We could build check for HAVE_GETFILEINFORMATIONBYHANDLE, but
	 * instead assume GetFullPathname implies this method will work as
	 * they're part of the same API.
	 */
#ifdef HAVE_GETFULLPATHNAME
	HANDLE handle1, handle2;

	handle1 = CreateFile(rp1, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	handle2 = CreateFile(rp2, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

	ret = file_compare_info(handle1, handle2);
#else
	/* Everywhere else, see if stat() maps to the same device and inode.
	 *
	 * NOTE: stat() works on Windows, but does not set an inode
	 * value for non-UNIX filesystems.
	 */
	struct stat sb1, sb2;
	if ((stat(rp1, &sb1) == 0) && (stat(rp2, &sb2) == 0) &&
	    (sb1.st_dev == sb2.st_dev) && (sb1.st_ino == sb2.st_ino)) {
	    ret = 1;
	}
#endif
    }

    bu_free(rp1, "free rp1");
    bu_free(rp2, "free rp2");

    return ret;
}

/**
 * common guts to the file access functions that returns truthfully if
 * the current user has the ability permission-wise to access the
 * specified file.
 */
static int
file_access(const char *path, int access_level)
{
    struct stat sb;

    /* 0 is root or Windows user */
    uid_t uid = 0;

    /* 0 is wheel or Windows group */
    gid_t gid = 0;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    if (stat(path, &sb) == -1) {
	return 0;
    }

    std::filesystem::perms p = std::filesystem::perms::none;
    try
    {
	p = std::filesystem::status(path).permissions();
    }
    catch (std::filesystem::filesystem_error const &)
    {
	bu_log("Error: unable to get permissions for %s\n", path);
	return 0;
    }
    if (p == std::filesystem::perms::unknown)
	return 0;


#ifdef HAVE_GETEUID
    uid = geteuid();
#endif
#ifdef HAVE_GETEGID
    gid = getegid();
#endif

    bool we_own = ((uid_t)sb.st_uid == uid);
    if (we_own) {
	switch (access_level) {
	    case R_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::owner_read))
		    return 1;
		break;
	    case W_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::owner_write))
		    return 1;
		break;
	    case X_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::owner_exec))
		    return 1;
		break;
	    default:
		bu_log("Error - unknown access level\n");
		return 0;
	};
    }

    bool in_grp = ((gid_t)sb.st_gid == gid);
    /* search group database to see if we're in the file's group */
#if defined(HAVE_PWD_H) && defined (HAVE_GRP_H)
    if (!in_grp) {
	struct passwd *pwdb = getpwuid(uid);
	if (pwdb && pwdb->pw_name) {
	    int i;
	    struct group *grdb = getgrgid(sb.st_gid);
	    for (i = 0; grdb && grdb->gr_mem[i]; i++) {
		if (BU_STR_EQUAL(grdb->gr_mem[i], pwdb->pw_name)) {
		    /* one of our other groups */
		    in_grp = true;
		}
	    }
	}
    }
#endif

    if (in_grp) {
	switch (access_level) {
	    case R_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::group_read))
		    return 1;
		break;
	    case W_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::group_write))
		    return 1;
		break;
	    case X_OK:
		if (std::filesystem::perms::none != (p & std::filesystem::perms::group_exec))
		    return 1;
		break;
	    default:
		bu_log("Error - unknown access level\n");
		return 0;
	};
    }

    /* check other */
    switch (access_level) {
	case R_OK:
	    if (std::filesystem::perms::none != (p & std::filesystem::perms::others_read))
		return 1;
	    break;
	case W_OK:
	    if (std::filesystem::perms::none != (p & std::filesystem::perms::others_write))
		return 1;
	    break;
	case X_OK:
	    if (std::filesystem::perms::none != (p & std::filesystem::perms::others_exec))
		return 1;
	    break;
	default:
	    bu_log("Error - unknown access level\n");
	    return 0;
    };

    // Shouldn't get here
    return 0;
}


int
bu_file_readable(const char *path)
{
    return file_access(path, R_OK);
}


int
bu_file_writable(const char *path)
{
    return file_access(path, W_OK);
}


int
bu_file_executable(const char *path)
{
    return file_access(path, X_OK);
}


int
bu_file_directory(const char *path)
{
    std::filesystem::file_status fs;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    try
    {
	fs = std::filesystem::status(path);
    }
    catch (std::filesystem::filesystem_error const &)
    {
	bu_log("Error: unable to get permissions for %s\n", path);
	return 0;
    }

    bool is_directory = std::filesystem::is_directory(fs);
    return (is_directory) ? 1 : 0;
}


int
bu_file_symbolic(const char *path)
{
    std::filesystem::file_status fs;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    try
    {
	fs = std::filesystem::status(path);
    }
    catch (std::filesystem::filesystem_error const &)
    {
	bu_log("Error: unable to get permissions for %s\n", path);
	return 0;
    }

    bool is_symlink = std::filesystem::is_symlink(fs);
    return (is_symlink) ? 1 : 0;
}


int
bu_file_delete(const char *path)
{
    int ret = 0;
    int fd = -1;
    struct stat sb;

    /* reject empty, special, or non-existent paths */
    if (!path
	|| BU_STR_EQUAL(path, "")
	|| BU_STR_EQUAL(path, ".")
	|| BU_STR_EQUAL(path, "..") )
    {
	return 0;
    }


#ifdef HAVE_WINDOWS_H
    if (bu_file_directory(path)) {
	ret = (RemoveDirectory(path)) ? 1 : 0;
    } else {
	ret = (DeleteFile(path)) ? 1 : 0;
    }
#else
    if (remove(path) == 0) {
	ret = 1;
    }
#endif

    if (!bu_file_exists(path, &fd) || fd == -1) {
	if (fd > -1)
	    close(fd);
	return 1;
    }

    /* Second pass, try to force deletion by changing file permissions (similar
     * to rm -f).
     */
#ifdef HAVE_WINDOWS_H
    /* Because we have to close the fd before we can successfully remove a file
     * on Windows, we also store the existing file attributes via the Windows
     * API mechanisms so we can restore them later using SetFileAttributes rather
     * than bu_fchmod */
    DWORD fattrs = GetFileAttributes(path);
    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS) && fattrs == INVALID_FILE_ATTRIBUTES) {
	bu_log("Warning, could not get file attributes for file %s!", path);
    }

    /* Go ahead and try the Windows API for removing READONLY, as long as we're here... */
    if (!SetFileAttributes(path, GetFileAttributes(path) & !FILE_ATTRIBUTE_READONLY)) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("bu_file_delete: warning, could not set file attributes on %s!", path);
	}
    }
    if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	if (fattrs == GetFileAttributes(path)) {
	    bu_log("bu_file_delete: warning, initial delete attempt failed but file attributes unchanged after SetFileAttributes call to update permissions on %s!", path);
	}
    }
#endif

    if (fstat(fd, &sb) == -1) {
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("Warning, fstat failed on file %s!", path);
	}
    }
    bu_fchmod(fd, (sb.st_mode|S_IRWXU));

    /* Permissions updated (hopefully), try delete again */
#ifdef HAVE_WINDOWS_H
    close(fd);  // If we don't close this here, file delete on Windows will fail
    if (bu_file_directory(path)) {
	ret = (RemoveDirectory(path)) ? 1 : 0;
    } else {
	ret = (DeleteFile(path)) ? 1 : 0;
    }
    if (UNLIKELY(!ret && (bu_debug & BU_DEBUG_PATHS))) {
	char buf[BUFSIZ];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), LANG_NEUTRAL, buf, BUFSIZ, NULL);
	bu_log("bu_file_delete: delete failed, Last Error was %s\n", (const char *)buf);
    }
#else
    if (remove(path) == 0) {
	ret = 1;
    }
#endif

    /* All boils down to whether the file still exists and if it does whether
     * remove thinks it succeeded. (We don't complain if we did succeed
     * according to our ret returns but someone else recreated the file in the
     * meantime.) */
    if (bu_file_exists(path, NULL) && !ret) {
	/* failure - restore original file permission */
#ifdef HAVE_WINDOWS_H
	SetFileAttributes(path, fattrs);
#else
	bu_fchmod(fd, sb.st_mode);
	close(fd);
#endif
	return 0;
    }

#ifdef HAVE_WINDOWS_H
    close(fd);
#endif

    return 1;
}

int
bu_fseek(FILE *stream, b_off_t offset, int origin)
{
    int ret;

#if defined(HAVE__FSEEKI64) && defined(SIZEOF_VOID_P) && SIZEOF_VOID_P == 8
    ret = _fseeki64(stream, offset, origin);
#else
    ret = fseek(stream, offset, origin);
#endif

    return ret;
}

b_off_t
bu_lseek(int fd, b_off_t offset, int whence)
{
    b_off_t ret;

#if defined(HAVE__LSEEKI64) && defined(SIZEOF_VOID_P) && SIZEOF_VOID_P == 8
    ret = _lseeki64(fd, offset, whence);
#else
    ret = lseek(fd, offset, whence);
#endif

    return ret;
}

b_off_t
bu_ftell(FILE *stream)
{
    b_off_t ret;

#if defined(HAVE__FTELLI64) && defined(SIZEOF_VOID_P) && SIZEOF_VOID_P == 8
    /* windows 64bit */
    ret = _ftelli64(stream);
#else
    ret = ftell(stream);
#endif

    return ret;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
