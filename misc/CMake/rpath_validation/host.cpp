/*
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
#include <cstdio>
#include <dlfcn.h>

int
main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    void *module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) {
        std::fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    auto version = reinterpret_cast<int (*)()>(dlsym(module, "module_version"));
    if (!version) {
        std::fprintf(stderr, "%s\n", dlerror());
        dlclose(module);
        return 1;
    }
    std::printf("%d\n", version());
    return dlclose(module) == 0 ? 0 : 1;
}
