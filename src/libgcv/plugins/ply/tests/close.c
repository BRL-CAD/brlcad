/*                    C L O S E . C
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

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/log.h"
#include "../rply.h"

static p_ply
write_sample(const char *path)
{
    p_ply writer = ply_create(path, PLY_ASCII, NULL, 0, NULL);
    if (!writer) return NULL;
    if (!ply_add_element(writer, "vertex", 1) || !ply_add_scalar_property(writer, "x", PLY_FLOAT) ||
	!ply_write_header(writer) || !ply_write(writer, 1.0)) {
	ply_close(writer);
	return NULL;
    }
    return writer;
}

int
main(int argc, const char **argv)
{
    char path[MAXPATHLEN] = {0};
    bu_setprogname(argv[0]);
    if (argc != 2) return 1;
    FILE *temporary = bu_temp_file(path, sizeof(path));
    if (!temporary) return 1;
    if (fclose(temporary)) return 1;
    p_ply normal = write_sample(path);
    int success = normal && ply_close(normal);
    bu_file_delete(path);
    if (!success) return 1;
    p_ply failing = write_sample(argv[1]);
    if (!failing) return 1;
    if (ply_close(failing)) {
	bu_log("PLY close accepted an output flush failure\n");
	return 1;
    }
    return 0;
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
