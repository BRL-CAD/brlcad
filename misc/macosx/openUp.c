/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License"). You may not use this file except in compliance with the
 * License. Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Shantonu Sen
 * openUp.c - program to set the "first-open-window" field of a volume
 *
 * Get the directory ID for the first argument, and set it as word 2
 * of the Finder Info fields for the volume it lives on
 *
 * cc -o openUp openUp.c
 * Usage: openUp /Volumes/Foo/OpenMe/
 *
 * This program is used to cause a window to open up when a volume is
 * mounted.  This is particularly useful for disk images made with
 * DiskCopy, such as those often used for application installation.
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/attr.h>
#include <sys/stat.h>
#include <sys/mount.h>

struct directoryinfo {
    unsigned long length;
    u_int32_t dirid;
};

struct volumeinfo {
    unsigned long length;
    u_int32_t finderinfo[8];
};


int main(int argc, char *argv[]) {

    char *path = NULL;
    struct attrlist alist;
    struct directoryinfo dirinfo;
    struct volumeinfo volinfo;
    struct statfs sfs;

    if (argc > 1) {
	path = argv[1];
    } else {
	printf("Usage: %s /Volumes/Foo/OpenMe/\n", argv[0]);
	exit(1);
    }

    bzero(&alist, sizeof(alist));
    alist.bitmapcount = 5;
    alist.commonattr = ATTR_CMN_OBJID;

    getattrlist(path, &alist, &dirinfo, sizeof(dirinfo), 0);

    printf("directory id: %lu\n", dirinfo.dirid);

    statfs(path, &sfs);

    printf("mountpoint: %s\n", sfs.f_mntonname);

    alist.commonattr = ATTR_CMN_FNDRINFO;
    alist.volattr = ATTR_VOL_INFO;

    getattrlist(sfs.f_mntonname, &alist, &volinfo, sizeof(volinfo), 0);
    volinfo.finderinfo[2] = dirinfo.dirid;
    setattrlist(sfs.f_mntonname, &alist, volinfo.finderinfo, sizeof(volinfo.finderinfo), 0);

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
