/*                B I T V _ P R I V A T E . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2026 United States Government as represented by the
 * U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef LIBBU_BITV_PRIVATE_H
#define LIBBU_BITV_PRIVATE_H

#include "common.h"
#include "bu/bitv.h"
#include "bu/magic.h"
#include "bu/list.h"

struct bu_bitv {
    struct bu_list l;
    size_t nbits;
    bitv_t bits[2];
};

#endif /* LIBBU_BITV_PRIVATE_H */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
