/**
 *     S T R I N G _ C O N V E R S I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include "frontend/string_conversion.h"

#include <cwchar>

#ifdef _WIN32
#  include <windows.h>
#endif


int
creo_brl_wide_to_utf8(const wchar_t *wide_text, std::string *text)
{
    if (!wide_text || !text)
        return 0;

#ifdef _WIN32
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                                         NULL, 0, NULL, NULL);

    if (size <= 0)
        return 0;

    text->resize((size_t)size);
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide_text, -1,
                            &(*text)[0], size, NULL, NULL) != size) {
        text->clear();
        return 0;
    }

    text->resize((size_t)size - 1);
#else
    std::mbstate_t state = {};
    const wchar_t *source = wide_text;
    const size_t size = std::wcsrtombs(NULL, &source, 0, &state);

    if (size == (size_t)-1)
        return 0;

    text->resize(size + 1);
    state = std::mbstate_t();
    source = wide_text;
    if (std::wcsrtombs(&(*text)[0], &source, text->size(), &state) != size) {
        text->clear();
        return 0;
    }

    text->resize(size);
#endif

    return 1;
}
