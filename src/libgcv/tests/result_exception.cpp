/* R E S U L T _ E X C E P T I O N . C P P
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

#include <stdexcept>

#include "gcv/runtime.h"
#include "rt/db_instance.h"

extern "C" int
gcv_result_exception(struct gcv_context *context, const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    context->dbip->dbi_read_only = 1;
    (void)gcv_report_loss(context, GCV_LOSS_METADATA, "fixture.exception", 1, "Before throwing");
    throw std::runtime_error("Conversion fixture exception");
}

extern "C" int
gcv_result_unknown_exception(struct gcv_context *UNUSED(context), const struct gcv_opts *UNUSED(options),
    const void *UNUSED(data), const char *UNUSED(target))
{
    throw GCV_CONVERSION_FAILED;
}

/*
 * Local Variables:
 * mode: C++
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
