/*                     M A T E R I A L . H
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
 * Material node fields and scene material accessors.
 */
/** @{ */
/* @file bsg/material.h */

#ifndef BSG_MATERIAL_H
#define BSG_MATERIAL_H

#include "common.h"
#include "bsg/defines.h"
#include "bsg/field.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"

__BEGIN_DECLS

BSG_EXPORT extern bsg_field_ref bsg_material_ref_diffuse_color_field(bsg_material_ref material);
BSG_EXPORT extern bsg_field_ref bsg_material_ref_specular_color_field(bsg_material_ref material);
BSG_EXPORT extern bsg_field_ref bsg_material_ref_emissive_color_field(bsg_material_ref material);
BSG_EXPORT extern bsg_field_ref bsg_material_ref_alpha_field(bsg_material_ref material);
BSG_EXPORT extern bsg_field_ref bsg_material_ref_shininess_field(bsg_material_ref material);
BSG_EXPORT extern bsg_field_ref bsg_material_ref_line_color_field(bsg_material_ref material);

BSG_EXPORT extern int bsg_material_ref_set_diffuse_color(bsg_material_ref material,
							 const unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_diffuse_color(bsg_material_ref material,
						     unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_set_specular_color(bsg_material_ref material,
							  const unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_specular_color(bsg_material_ref material,
						      unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_set_emissive_color(bsg_material_ref material,
							  const unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_emissive_color(bsg_material_ref material,
						      unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_set_alpha(bsg_material_ref material, double alpha);
BSG_EXPORT extern double bsg_material_ref_alpha(bsg_material_ref material);
BSG_EXPORT extern int bsg_material_ref_set_shininess(bsg_material_ref material, double shininess);
BSG_EXPORT extern double bsg_material_ref_shininess(bsg_material_ref material);
BSG_EXPORT extern int bsg_material_ref_set_line_color(bsg_material_ref material,
						      const unsigned char color[3]);
BSG_EXPORT extern int bsg_material_ref_line_color(bsg_material_ref material,
						  unsigned char color[3]);

BSG_EXPORT extern void
bsg_scene_material_set_rgb(bsg_scene_ref ref, unsigned char r, unsigned char g, unsigned char b);

BSG_EXPORT extern void
bsg_scene_material_get_rgb(bsg_scene_ref ref, unsigned char *r, unsigned char *g, unsigned char *b);

BSG_EXPORT extern void
bsg_scene_material_set_revision(bsg_scene_ref ref, uint32_t revision);

BSG_EXPORT extern uint32_t
bsg_scene_material_revision(bsg_scene_ref ref);

__END_DECLS

#endif /* BSG_MATERIAL_H */

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
