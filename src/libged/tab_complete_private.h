/* Shared cursor/parser helpers for command-owned completion adapters. */
#ifndef LIBGED_TAB_COMPLETE_PRIVATE_H
#define LIBGED_TAB_COMPLETE_PRIVATE_H

#include <stddef.h>
#include <vector>

struct ged_input_parse {
    char **argv;
    size_t argc;
    size_t cursor_arg;
    size_t input_len;
    std::vector<size_t> char_starts;
    std::vector<size_t> char_ends;
    char *copy;
};

int ged_input_parse_line(struct ged_input_parse *parsed, const char *input, size_t cursor_pos);
void ged_input_parse_free(struct ged_input_parse *parsed);

#endif
