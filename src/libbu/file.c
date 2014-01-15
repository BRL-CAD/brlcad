/*                         F I L E . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2014 United States Government as represented by
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

#include "bu.h"


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
	if (UNLIKELY(bu_debug & BU_DEBUG_PATHS)) {
	    bu_log("YES\n");
	}
	/* OK */
	return 1;
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


int
bu_same_file(const char *fn1, const char *fn2)
{
    struct stat sb1, sb2;

    if (UNLIKELY(!fn1 || !fn2)) {
	return 0;
    }

    if (UNLIKELY(fn1[0] == '\0' || fn2[0] == '\0')) {
	return 0;
    }

    if (!bu_file_exists(fn1, NULL) || !bu_file_exists(fn2, NULL)) {
	return 0;
    }

    if ((stat(fn1, &sb1) == 0) &&
	(stat(fn2, &sb2) == 0) &&
	(sb1.st_dev == sb2.st_dev) &&
	(sb1.st_ino == sb2.st_ino)) {
	return 1;
    }

    return 0;
}


int
bu_same_fd(int fd1, int fd2)
{
    struct stat sb1, sb2;

    if (UNLIKELY(fd1<0 || fd2<0)) {
	return 0;
    }

    /* ares files the same inode on same device? */
    if ((fstat(fd1, &sb1) == 0) && (fstat(fd2, &sb2) == 0) && (sb1.st_dev == sb2.st_dev) && (sb1.st_ino == sb2.st_ino)) {
	return 1;
    }

    return 0;
}


/**
 * _ b u _ f i l e _ a c c e s s
 *
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

	ret = (remove(path) == 0) ? 0 : 1;

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
    } else {
	/* deleted */
	return 1;
    }
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
