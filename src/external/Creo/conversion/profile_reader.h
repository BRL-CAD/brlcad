/**
 *             P R O F I L E _ R E A D E R . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_CONVERSION_PROFILE_READER_H
#define CREO_CONVERSION_PROFILE_READER_H

struct creo_brl_frontend_api;

void creo_brl_set_profile_frontend_api(const struct creo_brl_frontend_api *api);
void creo_brl_load_profile_options(const char *profile_path,
                                   const char *material_directory);

#endif /* CREO_CONVERSION_PROFILE_READER_H */
