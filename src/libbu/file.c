/*                         F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2016 United States Government as represented by
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

#include "bu/debug.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
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


#ifdef HAVE_GETFULLPATHNAME
HIDDEN int
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
    if (got1 && got2 &&
	(file_info1.dwVolumeSerialNumber == file_info2.dwVolumeSerialNumber) &&
	(file_info1.nFileIndexLow == file_info2.nFileIndexLow) &&
	(file_info1.nFileIndexHigh = file_info2.nFileIndexHigh)) {
	return 1;
    }

    return 0;
}
#endif /* HAVE_GETFULLPATHNAME */


int
bu_same_file(const char *fn1, const char *fn2)
{
    int ret = 0;
    char *rp1, *rp2;

    if (UNLIKELY(!fn1 || !fn2)) {
	return 0;
    }

    if (UNLIKELY(fn1[0] == '\0' || fn2[0] == '\0')) {
	return 0;
    }

    if (!bu_file_exists(fn1, NULL) || !bu_file_exists(fn2, NULL)) {
	return 0;
    }

    /* make sure symlinks to the same file report as same */
    rp1 = bu_realpath(fn1, NULL);
    rp2 = bu_realpath(fn2, NULL);

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


int
bu_same_fd(int fd1, int fd2)
{
    int ret = 0;

    if (UNLIKELY(fd1<0 || fd2<0)) {
	return 0;
    }

    {

/* Ditto bu_same_file() reasoning, assume GetFullPathname implies we have HANDLEs */
#ifdef HAVE_GETFULLPATHNAME
	HANDLE handle1, handle2;

	handle1 = _get_osfhandle(fd1);
	handle2 = _get_osfhandle(fd2);

	ret = file_compare_info(handle1, handle2);
#else
	/* are these files the same inode on same device?
	 *
	 * NOTE: fstat() works on Windows, but does not set an inode value
	 * for non-UNIX filesystems.
	 */
	struct stat sb1, sb2;
	if ((fstat(fd1, &sb1) == 0) && (fstat(fd2, &sb2) == 0) &&
	    (sb1.st_dev == sb2.st_dev) && (sb1.st_ino == sb2.st_ino)) {
	    ret = 1;
	}
#endif /* HAVE_GETFULLPATHNAME */

    }

    return ret;
}


/**
 * common guts to the file access functions that returns truthfully if
 * the current user has the ability permission-wise to access the
 * specified file.
 */
HIDDEN int
file_access(const char *path, int access_level)
{
    struct stat sb;
    int mask = 0;

    /* 0 is root or Windows user */
    uid_t uid = 0;

    /* 0 is wheel or Windows group */
    gid_t gid = 0;

    int usr_mask = S_IRUSR | S_IWUSR | S_IXUSR;
    int grp_mask = S_IRGRP | S_IWGRP | S_IXGRP;
    int oth_mask = S_IROTH | S_IWOTH | S_IXOTH;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    if (stat(path, &sb) == -1) {
	return 0;
    }

    if (access_level & R_OK) {
	mask = S_IRUSR | S_IRGRP | S_IROTH;
    }
    if (access_level & W_OK) {
	mask = S_IWUSR | S_IWGRP | S_IWOTH;
    }
    if (access_level & X_OK) {
	mask = S_IXUSR | S_IXGRP | S_IXOTH;
    }

#ifdef HAVE_GETEUID
    uid = geteuid();
#endif
#ifdef HAVE_GETEGID
    gid = getegid();
#endif

    if ((uid_t)sb.st_uid == uid) {
	/* we own it */
	return sb.st_mode & (mask & usr_mask);
    } else if ((gid_t)sb.st_gid == gid) {
	/* our primary group */
	return sb.st_mode & (mask & grp_mask);
    }

    /* search group database to see if we're in the file's group */
#if defined(HAVE_PWD_H) && defined (HAVE_GRP_H)
    {
	struct passwd *pwdb = getpwuid(uid);
	if (pwdb && pwdb->pw_name) {
	    int i;
	    struct group *grdb = getgrgid(sb.st_gid);
	    for (i = 0; grdb && grdb->gr_mem[i]; i++) {
		if (BU_STR_EQUAL(grdb->gr_mem[i], pwdb->pw_name)) {
		    /* one of our other groups */
		    return sb.st_mode & (mask & grp_mask);
		}
	    }
	}
    }
#endif

    /* check other */
    return sb.st_mode & (mask & oth_mask);
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
    struct stat sb;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    if (stat(path, &sb) == -1) {
	return 0;
    }

    return (S_ISDIR(sb.st_mode));
}


int
bu_file_symbolic(const char *path)
{
    struct stat sb;

    if (UNLIKELY(!path || (path[0] == '\0'))) {
	return 0;
    }

    if (stat(path, &sb) == -1) {
	return 0;
    }

    return (S_ISLNK(sb.st_mode));
}


int
bu_file_delete(const char *path)
{
    int fd = 0;
    int ret = 0;
    int retry = 0;
    struct stat sb;

    /* reject empty, special, or non-existent paths */
    if (!path
	|| BU_STR_EQUAL(path, "")
	|| BU_STR_EQUAL(path, ".")
	|| BU_STR_EQUAL(path, "..")
	|| !bu_file_exists(path, &fd))
    {
	return 0;
    }

    do {

	if (retry++) {
	    /* second pass, try to force deletion by changing file
	     * permissions (similar to rm -f).
	     */
	    if (fstat(fd, &sb) == -1) {
		break;
	    }
	    bu_fchmod(fd, (sb.st_mode|S_IRWXU));
	}

	if (remove(path) == 0)
	    ret = 1;

    } while (ret == 0 && retry < 2);
    close(fd);

    /* all boils down to whether the file still exists, not whether
     * remove thinks it succeeded.
     */
    if (bu_file_exists(path, &fd)) {
	/* failure */
	if (retry > 1) {
	    /* restore original file permission */
	    bu_fchmod(fd, sb.st_mode);
	}
	close(fd);
	return 0;
    }

    /* deleted */
    return 1;
}


int
bu_fseek(FILE *stream, off_t offset, int origin)
{
    int ret;

#if defined(HAVE__FSEEKI64) && defined(SIZEOF_VOID_P) && SIZEOF_VOID_P == 8
    ret = _fseeki64(stream, offset, origin);
#else
    ret = fseek(stream, offset, origin);
#endif

    return ret;
}


off_t
bu_ftell(FILE *stream)
{
    off_t ret;

#if defined(HAVE__FTELLI64) && defined(SIZEOF_VOID_P) && SIZEOF_VOID_P == 8
    /* windows 64bit */
    ret = _ftelli64(stream);
#else
    ret = ftell(stream);
#endif

    return ret;
}


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
