/**
 *       S T R I N G _ C O N V E R S I O N . H
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#ifndef CREO_FRONTEND_STRING_CONVERSION_H
#define CREO_FRONTEND_STRING_CONVERSION_H

#include <string>

int creo_brl_wide_to_utf8(const wchar_t *wide_text, std::string *text);

#endif /* CREO_FRONTEND_STRING_CONVERSION_H */
