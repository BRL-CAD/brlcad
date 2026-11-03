/**
 *              P R O F I L E _ P A T H . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file frontend/profile_path.cpp
 *
 * Toolkit-free profile-file precedence resolution for the Creo facade.
 */

#include "profile_path.h"

#include <ctype.h>
#include <string.h>


static const char CREO_BRL_PROFILE_FILE_NAME[] = "Creo_to_BRL.g";
static const char CREO_BRL_PATH_SEPARATOR = '\\';


static int
path_has_drive_prefix(const char *path)
{
    return path && isalpha((unsigned char)path[0]) && path[1] == ':';
}


static int
path_is_absolute(const char *path)
{
    return path && path[0] &&
           (path[0] == '/' || path[0] == '\\' ||
            (path_has_drive_prefix(path) &&
             (path[2] == '/' || path[2] == '\\')));
}


static const char *
drive_relative_name(const char *path, const char *working_directory)
{
    if (!path_has_drive_prefix(path))
        return path;

    if (!working_directory || !path_has_drive_prefix(working_directory) ||
        tolower((unsigned char)path[0]) !=
            tolower((unsigned char)working_directory[0]))
        return NULL;

    return path[2] ? path + 2 : NULL;
}


static int
path_join(const char *directory, const char *name, char *path, size_t path_size)
{
    const size_t directory_length = directory ? strlen(directory) : 0;
    const size_t name_length = name ? strlen(name) : 0;
    const int has_separator = directory_length > 0 &&
                              (directory[directory_length - 1] == '/' ||
                               directory[directory_length - 1] == '\\');
    const size_t separator_length = directory_length > 0 && !has_separator ? 1 : 0;

    if (!directory || !directory[0] || !name || !name[0] || !path ||
        directory_length + separator_length + name_length + 1 > path_size)
        return 0;

    memcpy(path, directory, directory_length);
    if (separator_length)
        path[directory_length] = CREO_BRL_PATH_SEPARATOR;
    memcpy(path + directory_length + separator_length, name, name_length + 1);
    return 1;
}


static int
select_candidate(const char *candidate,
                 creo_brl_profile_path_exists_fn_t path_exists,
                 void *context,
                 char *profile_path,
                 size_t profile_path_size)
{
    const size_t candidate_length = candidate ? strlen(candidate) : 0;

    if (!candidate || !candidate[0] || !path_exists || !path_exists(candidate, context) ||
        !profile_path || candidate_length >= profile_path_size)
        return 0;

    memmove(profile_path, candidate, candidate_length + 1);
    return 1;
}


extern "C" int
creo_brl_resolve_profile_path(
    const char *environment_profile,
    const char *working_directory,
    const char *bundled_profile_path,
    creo_brl_profile_path_exists_fn_t path_exists,
    void *context,
    char *profile_path,
    size_t profile_path_size)
{
    if (!profile_path || profile_path_size == 0)
        return 0;

    profile_path[0] = '\0';
    if (!path_exists)
        return 0;

    if (environment_profile && environment_profile[0]) {
        const char *candidate = environment_profile;

        if (!path_is_absolute(environment_profile)) {
            const char *relative_name =
                drive_relative_name(environment_profile, working_directory);

            if (!path_join(working_directory, relative_name, profile_path,
                           profile_path_size))
                candidate = NULL;
            else
                candidate = profile_path;
        }

        if (select_candidate(candidate, path_exists, context, profile_path,
                             profile_path_size))
            return 1;
    }

    if (path_join(working_directory, CREO_BRL_PROFILE_FILE_NAME,
                  profile_path, profile_path_size) &&
        select_candidate(profile_path, path_exists, context, profile_path,
                         profile_path_size))
        return 1;

    if (select_candidate(bundled_profile_path, path_exists, context, profile_path,
                         profile_path_size))
        return 1;

    profile_path[0] = '\0';
    return 0;
}
