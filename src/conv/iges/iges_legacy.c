/*                    I G E S _ L E G A C Y . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "iges_struct.h"
#include "iges_extern.h"
#include "iges_output.h"

int
iges_legacy_index(int de)
{
    if (de > 0 && de % 2 && (size_t)IGES_DE2INDEX(de) < totentities &&
	dir[IGES_DE2INDEX(de)] && dir[IGES_DE2INDEX(de)]->direct == de)
	return IGES_DE2INDEX(de);
    iges_output_legacy_warning(de, "invalid_legacy_reference",
	"ignored reference to a missing or invalid directory entry");
    return -1;
}

int
iges_legacy_count(size_t owner, int count, size_t stride)
{
    /* Every parameter occupies at least one byte.  Bound list work by
     * the owning entity's available parameter records, not an arbitrary
     * global count that could still allow billions of reads at EOF. */
    if (owner < totentities && dir[owner]->paramlines > 0 && stride && count >= 0 &&
	(size_t)count <= (size_t)dir[owner]->paramlines * (PARAMLEN + 1) / stride)
	return 1;
    iges_output_legacy_warning(owner < totentities ? dir[owner]->direct : 0,
	"invalid_legacy_count", "entity list count exceeds its available parameter data");
    return 0;
}

int
Read_property(size_t owner, int type, int form)
{
    int count = 0;
    int selected = 0;
    Readint(&count, "");
    if (!iges_legacy_count(owner, count, 1))
	return 0;
    for (int i = 0; i < count; ++i) {
	int de = 0;
	Readint(&de, "");
	(void)iges_legacy_index(de);
    }
    count = 0;
    Readint(&count, "");
    if (!iges_legacy_count(owner, count, 1))
	return 0;
    for (int i = 0; i < count; ++i) {
	int de = 0;
	Readint(&de, "");
	const int index = iges_legacy_index(de);
	if (index < 0 || dir[index]->type != type)
	    continue;
	if ((type == 406 && dir[index]->form == form) ||
	    (type == 422 && brlcad_att_de && dir[index]->referenced == brlcad_att_de))
	    selected = selected ? selected : de;
    }
    return selected;
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
