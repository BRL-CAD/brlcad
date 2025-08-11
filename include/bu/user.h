/*                          U S E R . H
 * BRL-CAD
 *
 * Copyright (c) 2023-2025 United States Government as represented by
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

#include "bu/defines.h"


/**
 * used to specify what name type is desired
 */
enum bu_name_type {
  BU_USER_ACCT, /**< user account name */
  BU_USER_REAL, /**< user real full name */
  BU_USER_AUTO /**< either account or real */
};


/**
 * Routine for obtaining user names
 *
 * Returns a given user's account name or real full name if available.
 * May return "unknown" if the name is not available.  If 'name' is
 * empty, then the current user's name will be returned.  If 'name' is
 * not empty, then it is used as a case-insensitive lookup string.
 *
 * If 'name' is NULL or size is 0, then dynamic memory will be
 * allocated with bu_malloc() and returned.
 *
 @code
 char name[128] = {0};
 (void)bu_user_name(BU_USER_AUTO, name, sizeof(name));

 char *account = bu_user_name(BU_USER_ACCT, NULL, 0);
 bu_free(account);

 char *name = bu_user_name(BU_USER_REAL, "username", 0);
 bu_free(name);
 @endcode
 */
BU_EXPORT char *
bu_user_name(enum bu_user_type type, char *name, size_t size);


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
