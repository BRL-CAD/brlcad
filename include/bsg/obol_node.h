/*                   O B O L _ N O D E . H
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
/** @addtogroup libbsg
 *
 * @brief
 * Forward-compatibility stub for eventual Obol scene-graph integration.
 *
 * When Obol is vendored under libbsg the types and magic number defined
 * here will be used to distinguish native Obol nodes from BRL-CAD
 * scene elements.  For now this header is a placeholder that keeps
 * include paths consistent across development phases.
 */
/** @{ */
/* @file bsg/obol_node.h */

#ifndef BSG_OBOL_NODE_H
#define BSG_OBOL_NODE_H

#include "common.h"
#include "bsg/defines.h"

__BEGIN_DECLS

/** Magic number reserved for future Obol node structs. */
#define BSG_OBOL_NODE_MAGIC 0x4F424F4CUL /* "OBOL" */

/**
 * Placeholder for the future obol_node struct that will be used when
 * Obol is integrated under libbsg.  The struct is intentionally
 * empty until that work begins.
 */
struct bsg_obol_node_placeholder {
    uint32_t magic; /**< @brief will hold BSG_OBOL_NODE_MAGIC */
};

__END_DECLS

#endif /* BSG_OBOL_NODE_H */

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
