/*                     B U _ I N I T . C P P
 * BRL-CAD
 *
 * Copyright (c) 2019-2021 United States Government as represented by
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
/** @file init.c
 *
 * Sole purpose of this file is to capture status and aspects of an
 * application and the application's environment when first run such
 * as initial working directory and available memory.  That is, it's
 * intended to be reflective, not generative or conditional logic.
 *
 * Please do NOT add application-specific logic here.
 *
 * NOTE: as this init is global to ALL applications before main(),
 * care must be taken to not write to STDOUT or STDERR or app output
 * may be corrupted, signals can be raised, or worse.
 *
 */

#include "common.h"

#include "bu/defines.h"
#include "bu/app.h"
#include "bu/parallel.h"
#include "bu/dylib.h"


/* These ARE exported outside LIBBU */
int BU_SEM_GENERAL;
int BU_SEM_SYSCALL;
int BU_SEM_BN_NOISE;
int BU_SEM_MAPPEDFILE;

/* These ARE NOT exported outside LIBBU */
extern "C" int BU_SEM_DATETIME;
extern "C" int BU_SEM_DIR;
extern "C" int BU_SEM_MALLOC;
extern "C" int BU_SEM_THREAD;


static void
libbu_init(void)
{
    char iwd[MAXPATHLEN] = {0};

    BU_SEMAPHORE_DEFINE(BU_SEM_GENERAL);
    BU_SEMAPHORE_DEFINE(BU_SEM_SYSCALL);
    BU_SEMAPHORE_DEFINE(BU_SEM_BN_NOISE);
    BU_SEMAPHORE_DEFINE(BU_SEM_MAPPEDFILE);
    BU_SEMAPHORE_DEFINE(BU_SEM_THREAD);
    BU_SEMAPHORE_DEFINE(BU_SEM_MALLOC);
    BU_SEMAPHORE_DEFINE(BU_SEM_DATETIME);
    BU_SEMAPHORE_DEFINE(BU_SEM_DIR);

    bu_getiwd(iwd, MAXPATHLEN);
}


static void
libbu_clear(void)
{
    (void)bu_dlunload();
    bu_semaphore_free();
}


struct libbu_initializer {
    /* constructor */
    libbu_initializer() {
	libbu_init();
    }
    /* destructor */
    ~libbu_initializer() {
	libbu_clear();
    }
};

static libbu_initializer LIBBU;


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
