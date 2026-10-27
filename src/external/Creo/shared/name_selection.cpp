/**
 *         N A M E _ S E L E C T I O N . C P P
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */
/**
 * @file shared/name_selection.cpp
 *
 * Parameter-selector parsing and matching shared by capture and replay.
 */

#include "common.h"

#include "shared/name_selection.h"

#include <sstream>


static const char CREO_BRL_NAME_WHITESPACE[] = " \t\n\v\f\r";


static int
is_ascii_alphanumeric(unsigned char character)
{
    return (character >= 'A' && character <= 'Z') ||
           (character >= 'a' && character <= 'z') ||
           (character >= '0' && character <= '9');
}


std::string
creo_brl_name_text_lower(const std::string& text)
{
    std::string lowered = text;

    for (size_t index = 0; index < lowered.size(); ++index) {
        const unsigned char character = (unsigned char)lowered[index];

        if (character >= 'A' && character <= 'Z')
            lowered[index] = (char)(character - 'A' + 'a');
    }

    return lowered;
}


std::string
creo_brl_name_text_trim(const std::string& text)
{
    const size_t begin = text.find_first_not_of(CREO_BRL_NAME_WHITESPACE);

    if (begin == std::string::npos)
        return std::string();

    const size_t end = text.find_last_not_of(CREO_BRL_NAME_WHITESPACE);
    return text.substr(begin, end - begin + 1);
}


creo_brl_name_selector::creo_brl_name_selector(const std::string& text) :
    exact_name_(creo_brl_name_text_lower(text)),
    has_pattern_(0)
{
    int contains_pattern_character = 0;

    for (size_t index = 0; index < exact_name_.size(); ++index) {
        if (!is_ascii_alphanumeric((unsigned char)exact_name_[index])) {
            contains_pattern_character = 1;
            break;
        }
    }

    if (!contains_pattern_character)
        return;

    try {
        pattern_.assign(exact_name_, std::regex_constants::extended |
                                     std::regex_constants::nosubs |
                                     std::regex_constants::optimize);
        has_pattern_ = 1;
    } catch (const std::regex_error&) {
        /* An invalid configured expression has no regex fallback. */
    }
}


const std::string&
creo_brl_name_selector::exact_name() const
{
    return exact_name_;
}


int
creo_brl_name_selector::matches_exact(const std::string& parameter_name) const
{
    return creo_brl_name_text_lower(parameter_name) == exact_name_;
}


int
creo_brl_name_selector::matches_pattern(const std::string& parameter_name) const
{
    if (!has_pattern_)
        return 0;

    try {
        return std::regex_search(creo_brl_name_text_lower(parameter_name),
                                 pattern_);
    } catch (const std::regex_error&) {
        return 0;
    }
}


int
creo_brl_name_selector::has_pattern() const
{
    return has_pattern_;
}


std::vector<creo_brl_name_selector>
creo_brl_parse_name_selectors(const std::string& text)
{
    std::vector<creo_brl_name_selector> selectors;
    std::istringstream lines(text);
    std::string line;

    while (std::getline(lines, line)) {
        std::istringstream items(line);
        std::string item;

        while (std::getline(items, item, ',')) {
            const std::string trimmed = creo_brl_name_text_trim(item);

            if (!trimmed.empty())
                selectors.push_back(creo_brl_name_selector(trimmed));
        }
    }

    return selectors;
}
