/*                   B O O T S T R A P . C P P
 * BRL-CAD
 *
 * Copyright (c) 2012-2025 United States Government as represented by
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
/** @file bootstrap.cpp
 *
 * Brief description
 *
 */

#include "common.h"
#include "bio.h"

/*
 * Bootstrap app static initialization on Windows to add BRL-CAD's
 * library install path to list of paths searched for DLLs.  This is
 * useful in lieu of requiring PATH be set apriori, avoids registry
 * (and associated permissions required), and avoids putting libs in
 * same dir as binaries (i.e., consitent installation hierarchy).
 */
static bool
bootstrap()
{
  return SetDllDirectory(BRLCAD_ROOT "\\lib");
}


BOOL APIENTRY DllMain(HANDLE hModule, DWORD dwReason, LPVOID lpReserved)
{
  return bootstrap();
}


static bool initialized = bootstrap();
