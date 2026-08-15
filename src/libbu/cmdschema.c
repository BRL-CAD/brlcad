/*                    C M D S C H E M A . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "vmath.h"
#include "bu/color.h"
#include "bu/cmdschema.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/units.h"

#include "cmdschema_private.h"


static const char *
cmd_schema_keyword_canonical(const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values, const char *arg);
static int cmd_schema_range_valid(const struct bu_cmd_value_range *range,
	bu_cmd_value_t type, const char *arg);


const char *
bu_cmd_option_canonical(const struct bu_cmd_option *option)
{
    if (!option)
	return NULL;
    return option->canonical ? option->canonical :
	(option->longopt ? option->longopt : option->shortopt);
}


int
bu_cmd_option_is_valid(const struct bu_cmd_option *option)
{
    return option && (option->canonical || option->shortopt || option->longopt);
}


static size_t
cmd_schema_optional_scalar_token_count(size_t available, const char **argv)
{
    if (!available || !argv || !argv[0] || argv[0][0] == '-')
	return 0;
    return 1;
}


static size_t
cmd_schema_counted_vector3_token_count(size_t available, const char **argv)
{
    fastf_t xyz[3] = {0.0, 0.0, 0.0};
    int count;
    int consumed;

    if (!available || !argv || !argv[0] || !bu_cmd_integer_from_str(&count, argv[0]))
	return 0;
    if (available == 1)
	return 1;
    consumed = bu_cmd_vector3_from_argv(xyz, available - 1,
	(const char * const *)(argv + 1));
    return consumed > 0 ? (size_t)consumed + 1 : 1;
}


static const struct bu_cmd_arg_variant cmd_rgb_arg_variants[] = {
    BU_CMD_ARG_VARIANT("packed", "r/g/b", 1,
	"Slash-, comma-, or semicolon-separated RGB channels"),
    BU_CMD_ARG_VARIANT("components", "r g b", 3,
	"Three separate 8-bit RGB channel words"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_variant cmd_color_arg_variants[] = {
    BU_CMD_ARG_VARIANT("packed", "color", 1,
	"Named, hexadecimal, or packed RGB color"),
    BU_CMD_ARG_VARIANT("components", "r g b", 3,
	"Three separate RGB component words"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_variant cmd_vector3_arg_variants[] = {
    BU_CMD_ARG_VARIANT("packed", "x/y/z", 1,
	"Packed or quoted XYZ vector"),
    BU_CMD_ARG_VARIANT("components", "x y z", 3,
	"Three separate numeric component words"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_variant cmd_counted_vector3_arg_variants[] = {
    BU_CMD_ARG_VARIANT("packed", "count x/y/z", 2,
	"Count followed by one packed XYZ vector"),
    BU_CMD_ARG_VARIANT("components", "count x y z", 4,
	"Count followed by three numeric component words"),
    BU_CMD_ARG_VARIANT_NULL
};
static const struct bu_cmd_arg_variant cmd_optional_scalar_arg_variants[] = {
    BU_CMD_ARG_VARIANT("omitted", "", 0, "No option argument"),
    BU_CMD_ARG_VARIANT("value", "value", 1, "One scalar option argument"),
    BU_CMD_ARG_VARIANT_NULL
};

const struct bu_cmd_arg_shape bu_cmd_rgb_arg_shape = {
    BU_CMD_ARG_SHAPE_RGB, 1, 3,
    "packed r/g/b, r,g,b, or r;g;b; or three RGB channels", NULL,
    cmd_rgb_arg_variants
};

const struct bu_cmd_arg_shape bu_cmd_color_arg_shape = {
    BU_CMD_ARG_SHAPE_COLOR, 1, 3,
    "packed color or three RGB components", NULL, cmd_color_arg_variants
};

const struct bu_cmd_arg_shape bu_cmd_vector3_arg_shape = {
    BU_CMD_ARG_SHAPE_VECTOR3, 1, 3,
    "packed x/y/z, x,y,z, or x;y;z; quoted x y z; or three numeric components", NULL,
    cmd_vector3_arg_variants
};

const struct bu_cmd_arg_shape bu_cmd_counted_vector3_arg_shape = {
    BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 2, 4,
    "integer count plus packed or three-component XYZ vector",
    cmd_schema_counted_vector3_token_count, cmd_counted_vector3_arg_variants
};

const struct bu_cmd_arg_shape bu_cmd_optional_scalar_arg_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 0, 1,
    "one optional scalar token, excluding the next option",
    cmd_schema_optional_scalar_token_count, cmd_optional_scalar_arg_variants
};

/* Standard option macros intentionally leave arg_shape NULL.  In a Windows
 * DLL consumer, the address of exported data is not a valid C static
 * initializer (MSVC C2099); function pointers remain safe in that context. */
static const struct bu_cmd_arg_shape *
cmd_schema_option_arg_shape(const struct bu_cmd_option *option)
{
    if (!option)
	return NULL;
    if (option->arg_shape)
	return option->arg_shape;
    if (option->consume == bu_cmd_rgb_consume)
	return &bu_cmd_rgb_arg_shape;
    if (option->consume == bu_cmd_color_consume)
	return &bu_cmd_color_arg_shape;
    if (option->consume == bu_cmd_vector3_consume)
	return &bu_cmd_vector3_arg_shape;
    if (option->value_type == BU_CMD_VALUE_STRING &&
	option->arg_requirement == BU_CMD_ARG_OPTIONAL)
	return &bu_cmd_optional_scalar_arg_shape;
    return NULL;
}


int
bu_cmd_rgb_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    unsigned char rgb[3] = {0, 0, 0};
    int consumed = bu_rgb_from_argv(rgb, argc, (const char * const *)argv);

    if (consumed == 0 || (size_t)consumed != argc) {
	if (msg)
	    bu_vls_printf(msg, "RGB color must be r/g/b, r,g,b, r;g;b, or three channels\n");
	return -1;
    }
    if (storage && !bu_color_from_rgb_chars((struct bu_color *)storage, rgb))
	return -1;
    return 0;
}


int
bu_cmd_rgb_optional_validate(size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    unsigned char rgb[3] = {0, 0, 0};
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "optional RGB color";

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (cursor_arg > argc || (argc && !argv))
	return -1;
    if (argc > 3) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "RGB color accepts zero, one packed, or three components";
    } else if (argc && bu_rgb_from_argv(rgb, argc, (const char * const *)argv) != (int)argc) {
	int partial = argc < 3;
	for (size_t i = 0; partial && i < argc; i++)
	    partial = bu_rgb_channel_validate(NULL, argv[i]) == 0;
	state = partial ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	hint = partial ? "remaining RGB components required" : "invalid RGB color";
    }
    result->state = state;
    result->token_start = cursor_arg;
    result->token_end = cursor_arg;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_COLOR;
    result->semantic_provider = NULL;
    return 0;
}


int
bu_cmd_color_optional_validate(size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "optional color";

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (cursor_arg > argc || (argc && !argv))
	return -1;
    if (argc > 3) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "color accepts zero, one packed, or three RGB components";
    } else if (argc && bu_cmd_color_from_argv(&color, argc,
	(const char * const *)argv) != (int)argc) {
	int partial = argc < 3;
	for (size_t i = 0; partial && i < argc; i++) {
	    fastf_t value;
	    partial = bu_cmd_number_from_str(&value, argv[i]);
	}
	state = partial ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	hint = partial ? "remaining RGB components required" : "invalid color";
    }
    result->state = state;
    result->token_start = cursor_arg;
    result->token_end = cursor_arg;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_COLOR;
    result->semantic_provider = NULL;
    return 0;
}


int
bu_cmd_color_from_argv(struct bu_color *color, size_t argc, const char * const *argv)
{
    struct bu_color parsed = BU_COLOR_INIT_ZERO;
    struct bu_vls packed = BU_VLS_INIT_ZERO;

    if (!color || !argv || !argc || !argv[0])
	return 0;
    if (bu_color_from_str(&parsed, argv[0])) {
	*color = parsed;
	return 1;
    }
    if (argc < 3 || !argv[1] || !argv[2])
	return 0;
    bu_vls_printf(&packed, "%s/%s/%s", argv[0], argv[1], argv[2]);
    if (!bu_color_from_str(&parsed, bu_vls_addr(&packed))) {
	bu_vls_free(&packed);
	return 0;
    }
    bu_vls_free(&packed);
    *color = parsed;
    return 3;
}


int
bu_cmd_color_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    struct bu_color color = BU_COLOR_INIT_ZERO;
    int consumed = bu_cmd_color_from_argv(&color, argc, (const char * const *)argv);

    if (consumed == 0 || (size_t)consumed != argc) {
	if (msg)
	    bu_vls_printf(msg, "color must be a packed color or three RGB components\n");
	return -1;
    }
    if (storage)
	*((struct bu_color *)storage) = color;
    return 0;
}


static const char *
cmd_schema_skip_space(const char *str)
{
    while (str && *str && isspace((unsigned char)*str))
	str++;
    return str;
}


static int
cmd_schema_number_from_prefix(fastf_t *value, const char **str)
{
    char *end = NULL;
    double parsed;
    const char *start;

    if (!value || !str || !*str)
	return 0;
    start = *str;
    errno = 0;
    parsed = strtod(start, &end);
    if (end == start || errno == ERANGE || !isfinite(parsed) ||
	(sizeof(fastf_t) == sizeof(float) && (parsed > FLT_MAX || parsed < -FLT_MAX)))
	return 0;
    *value = (fastf_t)parsed;
    *str = end;
    return 1;
}


static int
cmd_schema_vector3_parse_packed(fastf_t *xyz, const char *arg)
{
    fastf_t parsed[3] = {0.0, 0.0, 0.0};
    const char *str = arg;
    char separator = '\0';

    if (!xyz || !arg)
	return 0;
    for (size_t i = 0; i < 3; i++) {
	const char *after_number = NULL;
	const char *after_space = NULL;
	char current_separator = '\0';

	str = cmd_schema_skip_space(str);
	if (!cmd_schema_number_from_prefix(&parsed[i], &str))
	    return 0;
	if (i == 2)
	    break;
	after_number = str;
	after_space = cmd_schema_skip_space(str);
	if (*after_space == '/' || *after_space == ',' || *after_space == ';') {
	    current_separator = *after_space;
	    str = after_space + 1;
	} else if (after_space != after_number) {
	    current_separator = ' ';
	    str = after_space;
	} else {
	    return 0;
	}
	if (!separator)
	    separator = current_separator;
	else if (separator != current_separator)
	    return 0;
    }
    if (*cmd_schema_skip_space(str) != '\0')
	return 0;
    VMOVE(xyz, parsed);
    return 1;
}


int
bu_cmd_vector3_from_argv(fastf_t *xyz, size_t argc, const char * const *argv)
{
    fastf_t parsed[3] = {0.0, 0.0, 0.0};

    if (!xyz || !argv || !argc || !argv[0])
	return 0;
    if (cmd_schema_vector3_parse_packed(parsed, argv[0])) {
	VMOVE(xyz, parsed);
	return 1;
    }
    if (argc < 3 || !argv[1] || !argv[2])
	return 0;
    for (size_t i = 0; i < 3; i++) {
	if (!bu_cmd_number_from_str(&parsed[i], argv[i]))
	    return 0;
    }
    VMOVE(xyz, parsed);
    return 3;
}


int
bu_cmd_counted_vector3_from_argv(int *count, fastf_t *xyz, size_t argc,
	const char * const *argv)
{
    int parsed_count;
    fastf_t parsed_xyz[3] = {0.0, 0.0, 0.0};
    int consumed;

    if (!count || !xyz || !argv || argc < 2 || !argv[0] || !argv[1] ||
	!bu_cmd_integer_from_str(&parsed_count, argv[0]))
	return 0;
    consumed = bu_cmd_vector3_from_argv(parsed_xyz, argc - 1, argv + 1);
    if (consumed <= 0)
	return 0;
    *count = parsed_count;
    VMOVE(xyz, parsed_xyz);
    return consumed + 1;
}


int
bu_cmd_vector3_consume(struct bu_vls *msg, size_t argc, const char **argv, void *storage)
{
    fastf_t xyz[3] = {0.0, 0.0, 0.0};
    int consumed = bu_cmd_vector3_from_argv(xyz, argc, (const char * const *)argv);

    if (consumed == 0 || (size_t)consumed != argc) {
	if (msg)
	    bu_vls_printf(msg, "XYZ vector must be x/y/z, x,y,z, x;y;z, quoted x y z, or three finite numbers\n");
	return -1;
    }
    if (storage)
	VMOVE((fastf_t *)storage, xyz);
    return 0;
}


int
bu_cmd_vector3_optional_validate(size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    fastf_t xyz[3] = {0.0, 0.0, 0.0};
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "optional XYZ vector";

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (cursor_arg > argc || (argc && !argv))
	return -1;
    if (argc > 3) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "XYZ vector accepts zero, one packed, or three components";
    } else if (argc && bu_cmd_vector3_from_argv(xyz, argc,
	(const char * const *)argv) != (int)argc) {
	int partial = argc < 3;
	for (size_t i = 0; partial && i < argc; i++)
	    partial = bu_cmd_number_from_str(&xyz[i], argv[i]);
	state = partial ? BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	hint = partial ? "remaining XYZ components required" : "invalid XYZ vector";
    }
    result->state = state;
    result->token_start = cursor_arg;
    result->token_end = cursor_arg;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_VECTOR;
    result->semantic_provider = NULL;
    return 0;
}


int
bu_cmd_vector3_required_validate(size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    int ret;

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (cursor_arg > argc || (argc && !argv))
	return -1;
    ret = bu_cmd_vector3_optional_validate(argc, argv, cursor_arg, result);
    if (ret || argc)
	return ret;
    result->state = BU_CMD_VALIDATE_INCOMPLETE;
    result->hint = "XYZ vector required";
    return 0;
}


int
bu_cmd_integer_from_str(int *value, const char *arg)
{
    char *end = NULL;
    long parsed;

    if (!value || !arg || !arg[0])
	return 0;
    errno = 0;
    parsed = strtol(arg, &end, 0);
    if (errno == ERANGE || !end || *end || parsed > INT_MAX || parsed < INT_MIN)
	return 0;
    *value = (int)parsed;
    return 1;
}


int
bu_cmd_bool_from_str(int *value, const char *arg)
{
    int parsed;

    if (!value || !arg || !arg[0])
	return 0;
    parsed = bu_str_true(arg);
    if (parsed != 0 && parsed != 1)
	return 0;
    *value = parsed;
    return 1;
}


int
bu_cmd_hex_integer_from_str(unsigned int *value, const char *arg)
{
    char *end = NULL;
    unsigned long parsed;

    if (!value || !arg || !arg[0] || arg[0] == '-')
	return 0;
    errno = 0;
    parsed = strtoul(arg, &end, 16);
    if (errno == ERANGE || !end || *end || parsed > UINT_MAX)
	return 0;
    *value = (unsigned int)parsed;
    return 1;
}


int
bu_cmd_integer_pair_from_argv(int pair[2], size_t argc, const char * const *argv)
{
    int parsed[2] = {0, 0};

    if (!pair || !argv || argc != 2 || !argv[0] || !argv[1])
	return 0;
    if (!bu_cmd_integer_from_str(&parsed[0], argv[0]) ||
	!bu_cmd_integer_from_str(&parsed[1], argv[1]))
	return 0;
    pair[0] = parsed[0];
    pair[1] = parsed[1];
    return 2;
}


int
bu_cmd_integer_pair_optional_validate(size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    bu_cmd_validate_state_t state = BU_CMD_VALIDATE_VALID;
    const char *hint = "optional integer pair";

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (cursor_arg > argc || (argc && !argv))
	return -1;
    if (argc > 2) {
	state = BU_CMD_VALIDATE_INVALID;
	hint = "integer pair accepts zero or two components";
    } else {
	for (size_t i = 0; i < argc; i++) {
	    int value = 0;
	    if (!bu_cmd_integer_from_str(&value, argv[i])) {
		state = BU_CMD_VALIDATE_INVALID;
		hint = "invalid integer component";
		break;
	    }
	}
	if (state == BU_CMD_VALIDATE_VALID && argc == 1) {
	    state = BU_CMD_VALIDATE_INCOMPLETE;
	    hint = "second integer component required";
	}
    }
    result->state = state;
    result->token_start = cursor_arg;
    result->token_end = cursor_arg;
    result->expected = BU_CMD_EXPECT_OPERAND;
    result->hint = hint;
    result->completion_type = BU_CMD_VALUE_INTEGER;
    result->semantic_provider = NULL;
    return 0;
}


static int
cmd_schema_long_from_str(long *value, const char *arg, int base)
{
    char *end = NULL;
    long parsed;

    if (!value || !arg || !arg[0])
	return 0;
    errno = 0;
    parsed = strtol(arg, &end, base);
    if (errno == ERANGE || !end || *end)
	return 0;
    *value = parsed;
    return 1;
}


int
bu_cmd_long_from_str(long *value, const char *arg)
{
    return cmd_schema_long_from_str(value, arg, 0);
}


int
bu_cmd_hex_long_from_str(long *value, const char *arg)
{
    return cmd_schema_long_from_str(value, arg, 16);
}


int
bu_cmd_number_from_str(fastf_t *value, const char *arg)
{
    const char *end = arg;
    fastf_t parsed;

    if (!value || !arg || !arg[0])
	return 0;
    if (!cmd_schema_number_from_prefix(&parsed, &end) || *end)
	return 0;
    *value = parsed;
    return 1;
}


int
bu_cmd_char_from_str(char *value, const char *arg)
{
    if (!value || !arg || !arg[0])
	return 0;
    *value = arg[0];
    return 1;
}


int
bu_cmd_units_from_str(double *value, const char *arg)
{
    double parsed;

    if (!value || !arg || !arg[0])
	return 0;
    parsed = bu_mm_value(arg);
    if (!isfinite(parsed) || parsed <= 0.0)
	return 0;
    *value = parsed;
    return 1;
}


int
bu_cmd_units_validate(struct bu_vls *msg, const char *arg)
{
    double value;

    if (bu_cmd_units_from_str(&value, arg))
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected a valid BRL-CAD unit expression: %s\n",
		arg ? arg : "");
    return -1;
}


int
bu_cmd_iso639_1_validate(struct bu_vls *msg, const char *arg)
{
    static const char * const codes[] = {
	"ab", "aa", "af", "ak", "sq", "am", "ar", "an", "hy", "as", "av", "ae",
	"ay", "az", "bm", "ba", "eu", "be", "bn", "bh", "bi", "nb", "bs", "br",
	"bg", "my", "es", "ca", "km", "ch", "ce", "ny", "zh", "za", "cu", "cv",
	"kw", "co", "cr", "hr", "cs", "da", "dv", "nl", "dz", "en", "eo", "et",
	"ee", "fo", "fj", "fi", "fr", "ff", "gd", "gl", "lg", "ka", "de", "ki",
	"el", "kl", "gn", "gu", "ht", "ha", "he", "hz", "hi", "ho", "hu", "is",
	"io", "ig", "id", "ia", "ie", "iu", "ik", "ga", "it", "ja", "jv", "kn",
	"kr", "ks", "kk", "rw", "ky", "kv", "kg", "ko", "kj", "ku", "lo", "la",
	"lv", "lb", "li", "ln", "lt", "lu", "mk", "mg", "ms", "ml", "mt", "gv",
	"mi", "mr", "mh", "ro", "mn", "na", "nv", "nd", "nr", "ng", "ne", "se",
	"no", "nn", "ii", "oc", "oj", "or", "om", "os", "pi", "pa", "ps", "fa",
	"pl", "pt", "qu", "rm", "rn", "ru", "sm", "sg", "sa", "sc", "sr", "sn",
	"sd", "si", "sk", "sl", "so", "st", "su", "sw", "ss", "sv", "tl", "ty",
	"tg", "ta", "tt", "te", "th", "bo", "ti", "to", "ts", "tn", "tr", "tk",
	"tw", "ug", "uk", "ur", "uz", "ve", "vi", "vo", "wa", "cy", "fy", "wo",
	"xh", "yi", "yo", "za", "zu", NULL
    };

    if (arg && strlen(arg) == 2) {
	for (size_t i = 0; codes[i]; i++) {
	    if (BU_STR_EQUAL(arg, codes[i]))
		return 0;
	}
    }
    if (msg)
	bu_vls_printf(msg, "expected a lower-case ISO 639-1 language code: %s\n", arg ? arg : "");
    return -1;
}


int
bu_cmd_man_section_validate(struct bu_vls *msg, const char *arg)
{
    /* These are the stable BRL-CAD manual trees.  Keep the native parser
     * independent of the retired bu/opt.h compatibility macro. */
    static const char manual_sections[] = "135n";

    if (arg && strlen(arg) == 1 && strchr(manual_sections, arg[0]))
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected a BRL-CAD manual-page section: %s\n", arg ? arg : "");
    return -1;
}


static int
cmd_schema_integer_range_validate(struct bu_vls *msg, const char *arg,
	long minimum, const char *description)
{
    int value;


    if (bu_cmd_integer_from_str(&value, arg) && value >= minimum)
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected %s: %s\n", description, arg ? arg : "");
    return -1;
}


static int
cmd_schema_number_range_validate(struct bu_vls *msg, const char *arg,
	double minimum, int inclusive, const char *description)
{
    fastf_t value;


    if (bu_cmd_number_from_str(&value, arg) &&
	(inclusive ? value >= minimum : value > minimum))
	return 0;
    if (msg)
	bu_vls_printf(msg, "expected %s: %s\n", description, arg ? arg : "");
    return -1;
}


int
bu_cmd_positive_integer_validate(struct bu_vls *msg, const char *arg)
{
    return cmd_schema_integer_range_validate(msg, arg, 1, "a positive integer");
}


int
bu_cmd_nonnegative_integer_validate(struct bu_vls *msg, const char *arg)
{
    return cmd_schema_integer_range_validate(msg, arg, 0, "a nonnegative integer");
}


int
bu_cmd_positive_number_validate(struct bu_vls *msg, const char *arg)
{
    return cmd_schema_number_range_validate(msg, arg, 0.0, 0, "a positive number");
}


int
bu_cmd_nonnegative_number_validate(struct bu_vls *msg, const char *arg)
{
    return cmd_schema_number_range_validate(msg, arg, 0.0, 1, "a nonnegative number");
}


static const struct bu_cmd_option *
cmd_schema_find_canonical(const struct bu_cmd_schema *schema, const char *canonical)
{
    size_t i = 0;

    if (!schema || !schema->options || !canonical)
	return NULL;

    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (!option->alias_of && BU_STR_EQUAL(bu_cmd_option_canonical(option), canonical))
	    return option;
	i++;
    }
    return NULL;
}


static const struct bu_cmd_option *
cmd_schema_find_option(const struct bu_cmd_schema *schema, const char *name, int longopt)
{
    size_t i = 0;

    if (!schema || !schema->options || !name)
	return NULL;

    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	const char *spelling = longopt ? option->longopt : option->shortopt;
	if (spelling && BU_STR_EQUAL(spelling, name)) {
	    if (option->alias_of)
		return cmd_schema_find_canonical(schema, option->alias_of);
	    return option;
	}
	i++;
    }
    return NULL;
}


static const char *
cmd_schema_option_name(const struct bu_cmd_option *option)
{
    if (!option)
	return "option";
    const char *name = bu_cmd_option_canonical(option);
    return name ? name : "option";
}


static int
cmd_schema_has_options(const struct bu_cmd_schema *schema)
{
    return schema && schema->options && bu_cmd_option_is_valid(&schema->options[0]);
}


static int
cmd_schema_has_long_options(const struct bu_cmd_schema *schema)
{
    size_t i = 0;

    if (!schema || !schema->options)
	return 0;
    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (!option->hidden && !option->alias_of && option->longopt &&
	    option->longopt[0])
	    return 1;
	i++;
    }
    return 0;
}


/* A standalone marker is structural only while the option phase is active.
 * Optionless schemas accept it solely as the first word.  Once an
 * options-first schema has consumed an operand, a later "--" is an ordinary
 * operand in every scanner, including execution parsing and validation. */
static int
cmd_schema_is_end_marker(const struct bu_cmd_schema *schema, const char *arg,
	int options_allowed, size_t operand_count)
{
    return options_allowed && arg && BU_STR_EQUAL(arg, "--") &&
	(cmd_schema_has_options(schema) || operand_count == 0);
}


static const struct bu_cmd_option *
cmd_schema_lookup_token(const struct bu_cmd_schema *schema, const char *arg)
{
    const char *name = NULL;
    const char *eq = NULL;
    const struct bu_cmd_option *option = NULL;
    int longopt = 0;

    if (!arg || arg[0] != '-' || !arg[1])
	return NULL;

    longopt = arg[1] == '-';
    name = arg + (longopt ? 2 : 1);
    eq = strchr(name, '=');
    if (!eq)
	return cmd_schema_find_option(schema, name, longopt);

    size_t name_len = (size_t)(eq - name);
    char *name_copy = (char *)bu_malloc(name_len + 1, "command schema option name");
    memcpy(name_copy, name, name_len);
    name_copy[name_len] = '\0';
    option = cmd_schema_find_option(schema, name_copy, longopt);
    bu_free(name_copy, "command schema option name");
    return option;
}


static int
cmd_schema_set_value(const struct bu_cmd_option *option, void *data, const char *arg, struct bu_vls *msg)
{
    char *storage = NULL;
    const char *keyword = NULL;

    if (option->storage_offset == BU_CMD_STORAGE_NONE) {
	if (msg)
	    bu_vls_printf(msg, "--%s is syntax-only and has no execution storage binding\n",
		cmd_schema_option_name(option));
	return -1;
    }
	storage = (char *)data + option->storage_offset;

    if (option->value_type == BU_CMD_VALUE_KEYWORD)
	keyword = cmd_schema_keyword_canonical(option->value_keywords,
	    option->keyword_values, arg);
    if (option->value_type == BU_CMD_VALUE_KEYWORD && !keyword &&
	!option->validate) {
	if (msg)
	    bu_vls_printf(msg, "invalid keyword for --%s: %s\n",
		cmd_schema_option_name(option), arg);
	return -1;
    }
    if (option->value_type != BU_CMD_VALUE_FLAG &&
	!cmd_schema_range_valid(&option->range, option->value_type, arg)) {
	if (msg)
	    bu_vls_printf(msg, "value for --%s is outside its legal range: %s\n",
		cmd_schema_option_name(option), arg ? arg : "");
	return -1;
    }
    if (option->value_type != BU_CMD_VALUE_FLAG && option->validate && option->validate(msg, arg) != 0)
	return -1;

    switch (option->value_type) {
	case BU_CMD_VALUE_FLAG:
	    if (option->repeat)
		(*((int *)storage))++;
	    else
		*((int *)storage) = 1;
	    return 0;
	case BU_CMD_VALUE_BOOL:
	{
	    int value;
	    if (!bu_cmd_bool_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid boolean for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((int *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_INTEGER:
	{
	    int value;
	    if (!bu_cmd_integer_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid integer for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((int *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_HEX_INTEGER:
	{
	    unsigned int value;
	    if (!bu_cmd_hex_integer_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid hexadecimal integer for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((unsigned int *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_LONG:
	{
	    long value;
	    if (option->arg_requirement == BU_CMD_ARG_NONE) {
		if (option->repeat)
		    (*((long *)storage))++;
		else
		    *((long *)storage) = 1;
		return 0;
	    }
	    if (!bu_cmd_long_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid long integer for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((long *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_HEX_LONG:
	{
	    long value;
	    if (!bu_cmd_hex_long_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid hexadecimal long integer for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((long *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_NUMBER:
	{
	    fastf_t value;
	    if (!bu_cmd_number_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid number for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((fastf_t *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_CHAR:
	{
	    char value;
	    if (!bu_cmd_char_from_str(&value, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid character for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    *((char *)storage) = value;
	    return 0;
	}
	case BU_CMD_VALUE_VLS:
	    if (!arg || !arg[0]) {
		if (msg)
		    bu_vls_printf(msg, "string argument expected for --%s\n", cmd_schema_option_name(option));
		return -1;
	    }
	    if (bu_vls_strlen((struct bu_vls *)storage) > 0)
		bu_vls_putc((struct bu_vls *)storage, ' ');
	    bu_vls_strcat((struct bu_vls *)storage, arg);
	    return 0;
	case BU_CMD_VALUE_STRING:
	case BU_CMD_VALUE_VECTOR:
	case BU_CMD_VALUE_MATRIX:
	case BU_CMD_VALUE_KEYWORD:
	case BU_CMD_VALUE_DB_OBJECT:
	case BU_CMD_VALUE_DB_PATH:
	case BU_CMD_VALUE_FILE:
	case BU_CMD_VALUE_RAW:
	    *((const char **)storage) = option->value_type == BU_CMD_VALUE_KEYWORD ?
		(keyword ? keyword : arg) : arg;
	    return 0;
	case BU_CMD_VALUE_COLOR:
	    if (!bu_color_from_str((struct bu_color *)storage, arg)) {
		if (msg)
		    bu_vls_printf(msg, "invalid color for --%s: %s\n", cmd_schema_option_name(option), arg);
		return -1;
	    }
	    return 0;
	case BU_CMD_VALUE_CUSTOM:
	    if (!option->custom_parse)
		return -1;
	    return option->custom_parse(msg, arg, storage);
	default:
	    break;
    }

    return -1;
}


static size_t
cmd_schema_option_min_tokens(const struct bu_cmd_option *option)
{
    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);

    if (!option || option->arg_requirement == BU_CMD_ARG_NONE)
	return 0;
    if (shape)
	return shape->min_tokens;
    return option->arg_requirement == BU_CMD_ARG_OPTIONAL ? 0 : 1;
}


static size_t
cmd_schema_option_max_tokens(const struct bu_cmd_option *option)
{
    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);

    if (!option || option->arg_requirement == BU_CMD_ARG_NONE)
	return 0;
    if (shape)
	return shape->max_tokens;
    return 1;
}


/* Most shaped arguments consume their declared maximum greedily.  RGB and
 * vector3 are standard variable-width shapes: a packed first token must leave
 * the next word available to the command, while three individual components
 * are consumed together. */
static size_t
cmd_schema_option_argument_count(const struct bu_cmd_option *option,
	size_t available, const char **argv)
{
    size_t count = available;
    size_t max_tokens = cmd_schema_option_max_tokens(option);
    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);

    if (max_tokens != BU_CMD_COUNT_UNLIMITED && count > max_tokens)
	count = max_tokens;
    if (shape && shape->token_count) {
	size_t selected = shape->token_count(count, argv);
	return selected <= count ? selected : 0;
    }
    if (shape && shape->kind == BU_CMD_ARG_SHAPE_RGB) {
	unsigned char rgb[3] = {0, 0, 0};
	return (size_t)bu_rgb_from_argv(rgb, count, (const char * const *)argv);
    }
    if (shape && shape->kind == BU_CMD_ARG_SHAPE_COLOR) {
	struct bu_color color = BU_COLOR_INIT_ZERO;
	return (size_t)bu_cmd_color_from_argv(&color, count, (const char * const *)argv);
    }
    if (shape && shape->kind == BU_CMD_ARG_SHAPE_VECTOR3) {
	fastf_t xyz[3] = {0.0, 0.0, 0.0};
	return (size_t)bu_cmd_vector3_from_argv(xyz, count, (const char * const *)argv);
    }
    return count;
}


static int
cmd_schema_rgb_partial(const struct bu_cmd_option *option, size_t available,
	const char **argv)
{
	const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
    if (!shape || shape->kind != BU_CMD_ARG_SHAPE_RGB ||
	available == 0 || available >= 3)
	return 0;
    for (size_t i = 0; i < available; i++) {
	if (bu_rgb_channel_validate(NULL, argv[i]) != 0)
	    return 0;
    }
    return 1;
}


static int
cmd_schema_color_partial(const struct bu_cmd_option *option, size_t available,
	const char **argv)
{
	const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
    if (!shape || shape->kind != BU_CMD_ARG_SHAPE_COLOR ||
	available == 0 || available >= 3)
	return 0;
    for (size_t i = 0; i < available; i++) {
	fastf_t value;
	if (!bu_cmd_number_from_str(&value, argv[i]))
	    return 0;
    }
    return 1;
}


static int
cmd_schema_vector3_partial(const struct bu_cmd_option *option, size_t available,
	const char **argv)
{
	const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
    if (!shape || shape->kind != BU_CMD_ARG_SHAPE_VECTOR3 ||
	available == 0 || available >= 3)
	return 0;
    for (size_t i = 0; i < available; i++) {
	fastf_t value;
	if (!bu_cmd_number_from_str(&value, argv[i]))
	    return 0;
    }
    return 1;
}


static int
cmd_schema_option_attached_allowed(const struct bu_cmd_option *option)
{
    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
    return cmd_schema_option_max_tokens(option) == 1 ||
	(shape && (shape->kind == BU_CMD_ARG_SHAPE_RGB ||
	    shape->kind == BU_CMD_ARG_SHAPE_COLOR ||
	    shape->kind == BU_CMD_ARG_SHAPE_VECTOR3));
}


/* Accept the conventional -ovalue spelling for a one-letter short option
 * that takes an argument.  Exact option names and all-flag clusters are
 * resolved first, so this helper cannot reinterpret either form. */
static const struct bu_cmd_option *
cmd_schema_attached_short_option(const struct bu_cmd_schema *schema,
	const char *arg, const char **value)
{
    const struct bu_cmd_option *option;
    char short_name[2] = {'\0', '\0'};

    if (value)
	*value = NULL;
    if (!schema || !arg || arg[0] != '-' || arg[1] == '-' ||
	!arg[1] || !arg[2])
	return NULL;

    short_name[0] = arg[1];
    option = cmd_schema_find_option(schema, short_name, 0);
    if (!option || option->arg_requirement == BU_CMD_ARG_NONE ||
	!cmd_schema_option_attached_allowed(option))
	return NULL;

    if (value)
	*value = arg[2] == '=' ? arg + 3 : arg + 2;
    return option;
}


static int
cmd_schema_apply_option_arguments(const struct bu_cmd_option *option, void *data,
	size_t argc, const char **argv, struct bu_vls *msg)
{
    void *storage = NULL;

    if (!option || !data)
	return -1;
    if (option->storage_offset == BU_CMD_STORAGE_NONE) {
	if (msg)
	    bu_vls_printf(msg, "--%s is syntax-only and has no execution storage binding\n",
		cmd_schema_option_name(option));
	return -1;
    }
    storage = (char *)data + option->storage_offset;
    if (option->consume)
	return option->consume(msg, argc, argv, storage);
    if (argc == 0) {
	/* An optional custom argument may deliberately use its absent form as
	 * a state transition (for example, --list with an optional =json
	 * refinement).  Typed optional values retain the conventional no-op. */
	return option->value_type == BU_CMD_VALUE_CUSTOM ?
	    cmd_schema_set_value(option, data, NULL, msg) : 0;
    }
    if (argc != 1) {
	if (msg)
	    bu_vls_printf(msg, "--%s requires a shape consumer for %lu arguments\n",
		cmd_schema_option_name(option), (unsigned long)argc);
	return -1;
    }
    return cmd_schema_set_value(option, data, argv[0], msg);
}


/* A conventional short option cluster is unambiguous only when every member
 * is a no-argument flag.  Leave all other forms to the ordinary option parser
 * so that a future attached-argument convention is not guessed incorrectly. */
static int
cmd_schema_short_flag_cluster(const struct bu_cmd_schema *schema, const char *arg,
	void *data, struct bu_vls *msg, int apply)
{
    if (!schema || !arg || arg[0] != '-' || arg[1] == '-' || !arg[1] || !arg[2])
	return 0;

    for (size_t i = 1; arg[i]; i++) {
	char short_name[2] = {arg[i], '\0'};
	const struct bu_cmd_option *option = cmd_schema_find_option(schema, short_name, 0);
	if (!option || option->arg_requirement != BU_CMD_ARG_NONE)
	    return 0;
	if (apply && cmd_schema_set_value(option, data, NULL, msg) != 0)
	    return -1;
    }

    return 1;
}


static int
cmd_schema_keyword_matches(const struct bu_cmd_value_keyword *keyword, const char *arg)
{

    if (!keyword || !keyword->canonical || !arg)
	return 0;
    if (BU_STR_EQUAL(keyword->canonical, arg))
	return 1;
    if (keyword->aliases) {
	for (size_t i = 0; keyword->aliases[i]; i++) {
	    if (BU_STR_EQUAL(keyword->aliases[i], arg))
		return 1;
	}
    }
    return 0;
}


static const char *
cmd_schema_keyword_canonical(const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values, const char *arg)
{
    if (!arg)
	return NULL;
    if (keyword_values) {
	for (size_t i = 0; keyword_values[i].canonical; i++) {
	    if (cmd_schema_keyword_matches(&keyword_values[i], arg))
		return keyword_values[i].canonical;
	}
	return NULL;
    }
    if (keywords) {
	for (size_t i = 0; keywords[i]; i++) {
	    if (BU_STR_EQUAL(keywords[i], arg))
		return keywords[i];
	}
	return NULL;
    }
    return arg;
}


static int
cmd_schema_range_valid(const struct bu_cmd_value_range *range,
	bu_cmd_value_t type, const char *arg)
{
    if (!range || range->kind == BU_CMD_RANGE_NONE)
	return 1;
    if (!arg)
	return 0;

    if (range->kind == BU_CMD_RANGE_INTEGER) {
	long value = 0;

	switch (type) {
	    case BU_CMD_VALUE_INTEGER:
	    {
		int ivalue;
		if (!bu_cmd_integer_from_str(&ivalue, arg))
		    return 0;
		value = (long)ivalue;
		break;
	    }
	    case BU_CMD_VALUE_HEX_INTEGER:
	    {
		unsigned int uvalue;
		if (!bu_cmd_hex_integer_from_str(&uvalue, arg))
		    return 0;
#if LONG_MAX < UINT_MAX
		if (uvalue > (unsigned int)LONG_MAX)
		    return 0;
#endif
		value = (long)uvalue;
		break;
	    }
	    case BU_CMD_VALUE_LONG:
		if (!bu_cmd_long_from_str(&value, arg))
		    return 0;
		break;
	    case BU_CMD_VALUE_HEX_LONG:
		if (!bu_cmd_hex_long_from_str(&value, arg))
		    return 0;
		break;
	    default:
		return 0;
	}
	if (range->has_minimum &&
	    (value < range->integer_minimum ||
	     (!range->minimum_inclusive && value == range->integer_minimum)))
	    return 0;
	if (range->has_maximum &&
	    (value > range->integer_maximum ||
	     (!range->maximum_inclusive && value == range->integer_maximum)))
	    return 0;
	return 1;
    }

    if (range->kind == BU_CMD_RANGE_NUMBER) {
	fastf_t value;
	if (type != BU_CMD_VALUE_NUMBER ||
	    !bu_cmd_number_from_str(&value, arg))
	    return 0;
	if (range->has_minimum &&
	    (range->minimum_inclusive ? value < range->number_minimum :
		value <= range->number_minimum))
	    return 0;
	if (range->has_maximum &&
	    (range->maximum_inclusive ? value > range->number_maximum :
		value >= range->number_maximum))
	    return 0;
	return 1;
    }

    return 0;
}


/* Syntax-time scalar checking.  This deliberately does not touch the
 * command's storage object: completion and highlighting must be side-effect
 * free.  Custom parsers receive a NULL storage pointer for this purpose. */
static int
cmd_schema_value_valid(const struct bu_cmd_option *option, const char *arg)
{
    int valid = 0;

    if (!option || !arg)
	return 0;
    switch (option->value_type) {
	case BU_CMD_VALUE_BOOL:
	{
	    int value;
	    valid = bu_cmd_bool_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_INTEGER:
	{
	    int value;
	    valid = bu_cmd_integer_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_HEX_INTEGER:
	{
	    unsigned int value;
	    valid = bu_cmd_hex_integer_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_LONG:
	{
	    long value;
	    valid = option->arg_requirement == BU_CMD_ARG_NONE || bu_cmd_long_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_HEX_LONG:
	{
	    long value;
	    valid = bu_cmd_hex_long_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_NUMBER:
	{
	    fastf_t value;
	    valid = bu_cmd_number_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_CHAR:
	{
	    char value;
	    valid = bu_cmd_char_from_str(&value, arg);
	    break;
	}
	case BU_CMD_VALUE_STRING:
	case BU_CMD_VALUE_VECTOR:
	case BU_CMD_VALUE_MATRIX:
	case BU_CMD_VALUE_KEYWORD:
	case BU_CMD_VALUE_DB_OBJECT:
	case BU_CMD_VALUE_DB_PATH:
	case BU_CMD_VALUE_FILE:
	case BU_CMD_VALUE_RAW:
	    valid = 1;
	    break;
	case BU_CMD_VALUE_VLS:
	    valid = arg[0] != '\0';
	    break;
	case BU_CMD_VALUE_COLOR:
	{
	    struct bu_color color = BU_COLOR_INIT_ZERO;
	    valid = bu_color_from_str(&color, arg);
	    break;
	}
	case BU_CMD_VALUE_CUSTOM:
	    valid = option->custom_parse && option->custom_parse(NULL, arg, NULL) == 0;
	    break;
	case BU_CMD_VALUE_FLAG:
	default:
	    valid = 1;
	    break;
    }

    /* Static keyword rows define the accepted vocabulary unless a custom
     * validator is present.  In that case the callback is authoritative and
     * the static rows remain the concise canonical completion vocabulary. */
    return valid && (option->value_type != BU_CMD_VALUE_KEYWORD ||
	option->validate ||
	cmd_schema_keyword_canonical(option->value_keywords, option->keyword_values, arg)) &&
	cmd_schema_range_valid(&option->range, option->value_type, arg) &&
	(!option->validate || option->validate(NULL, arg) == 0);
}


/* A leading '-' normally begins an option, but a negative numeric positional
 * value has long been accepted by bu_opt users without a preceding '--'.
 * Preserve that useful convention narrowly: only a numeric operand role may
 * reclaim an otherwise unknown dash-leading token.  String/raw operands must
 * continue to use '--' so misspelled options are diagnosed. */
static int
cmd_schema_dash_numeric_operand_valid(const struct bu_cmd_schema *schema,
	size_t operand_index, const char *arg)
{
    const struct bu_cmd_operand *operand;

    if (!schema || !arg || arg[0] != '-' || !arg[1])
	return 0;
    operand = bu_cmd_schema_operand(schema, operand_index);
    if (!operand)
	return 0;
    switch (operand->value_type) {
	case BU_CMD_VALUE_INTEGER:
	case BU_CMD_VALUE_HEX_INTEGER:
	case BU_CMD_VALUE_LONG:
	case BU_CMD_VALUE_HEX_LONG:
	case BU_CMD_VALUE_NUMBER:
	    return bu_cmd_operand_validate(operand, arg);
	default:
	    break;
    }
    return 0;
}


static const struct bu_cmd_parse_binding *
cmd_schema_binding(const struct bu_cmd_schema *schema,
	const struct bu_cmd_option *option,
	const struct bu_cmd_parse_binding *bindings)
{
    ptrdiff_t index;

    if (!schema || !schema->options || !option || !bindings)
	return NULL;
    index = option - schema->options;
    return index >= 0 ? &bindings[index] : NULL;
}


static const struct bu_cmd_option *
cmd_schema_find_bu_opt_name(const struct bu_cmd_schema *schema,
	const char *name)
{
    if (!schema || !schema->options || !name)
	return NULL;
    for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	const struct bu_cmd_option *option = &schema->options[i];
	if ((option->shortopt && BU_STR_EQUAL(option->shortopt, name)) ||
	    (option->longopt && BU_STR_EQUAL(option->longopt, name)))
	    return option;
    }
    return NULL;
}


/* Decode main's original bu_opt option spellings without invoking callbacks.
 * A short option followed by more characters is intentionally left
 * ambiguous here: its callback must decide, in one real invocation, whether
 * the tail is an attached argument or the remainder of a flag cluster. */
static const struct bu_cmd_option *
cmd_schema_bu_opt_option(const struct bu_cmd_schema *schema, const char *arg,
	const char **attached, const char **short_tail)
{
    const struct bu_cmd_option *option;

    if (attached)
	*attached = NULL;
    if (short_tail)
	*short_tail = NULL;
    if (!schema || !arg || arg[0] != '-' || !arg[1] ||
	BU_STR_EQUAL(arg, "--"))
	return NULL;

    if (arg[1] == '-') {
	const char *name = arg + 2;
	const char *eq = strchr(name, '=');
	char *copy = NULL;

	if (eq) {
	    size_t len = (size_t)(eq - name);
	    copy = (char *)bu_malloc(len + 1, "bu_opt long option name");
	    memcpy(copy, name, len);
	    copy[len] = '\0';
	    option = cmd_schema_find_bu_opt_name(schema, copy);
	    bu_free(copy, "bu_opt long option name");
	    if (attached)
		*attached = eq + 1;
	    return option;
	}
	return cmd_schema_find_bu_opt_name(schema, name);
    }

    {
	/* Historical bu_opt accepts either descriptor spelling after one or
	 * two dashes.  Check a complete spelling before interpreting a
	 * one-dash word as a short-option cluster or attached value. */
	option = cmd_schema_find_bu_opt_name(schema, arg + 1);
	if (option)
	    return option;
	char short_name[2] = {arg[1], '\0'};
	const char *remainder = arg + 2;
	option = cmd_schema_find_bu_opt_name(schema, short_name);
	if (option && remainder[0] && short_tail)
	    *short_tail = remainder;
	return option;
    }
}


int
_bu_cmd_schema_parse_bound(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[],
	const struct bu_cmd_parse_binding *bindings, unsigned int flags)
{
    int i = 0;
    int end_options = 0;
    int ret = -1;
    int interspersed = 0;
    size_t known_count = 0;
    size_t operand_count = 0;
    const char **known_args = NULL;
    const char **operand_args = NULL;

    int bu_opt_syntax = (flags & BU_CMD_PARSE_INTERNAL_BU_OPT_SYNTAX) != 0;
    int pass_unknown = (flags & BU_CMD_PARSE_INTERNAL_PASS_UNKNOWN) != 0;
    int leftovers_first = (flags & BU_CMD_PARSE_INTERNAL_LEFTOVERS_FIRST) != 0;
    int honor_end_marker = (flags & BU_CMD_PARSE_INTERNAL_END_MARKER) != 0;

    if (!schema || argc < 0 || (argc > 0 && !argv))
	return -1;

    /* With no declared options, every word is an operand.  This matters for
     * raw-text commands such as echo: a literal leading '-' is not an option
     * merely because it resembles one. */
    if (!cmd_schema_has_options(schema)) {
	if (argc > 0 && !argv[0])
	    return -1;
	return argc > 0 && cmd_schema_is_end_marker(schema, argv[0], 1, 0) ? 1 : 0;
    }

    if (!data && !bindings)
	return -1;

    interspersed = schema->parse_policy == BU_CMD_PARSE_INTERSPERSED;
    if (interspersed && argc > 0) {
	known_args = (const char **)bu_calloc((size_t)argc, sizeof(*known_args),
	    "interspersed command option arguments");
	operand_args = (const char **)bu_calloc((size_t)argc, sizeof(*operand_args),
	    "interspersed command operands");
    }

    while (i < argc) {
	const char *arg = argv[i];
	const char *name = NULL;
	const char *eq = NULL;
	const char *short_attached = NULL;
	const struct bu_cmd_option *option = NULL;
	int longopt = 0;
	const struct bu_cmd_parse_binding *binding = NULL;
	const char *opt_attached = NULL;
	const char *opt_short_tail = NULL;

	if (!arg) {
	    if (msg)
		bu_vls_printf(msg, "null command argument\n");
	    goto done;
	}
    if ((!bu_opt_syntax || honor_end_marker) &&
	    cmd_schema_is_end_marker(schema, arg, !end_options, operand_count)) {
	    end_options = 1;
	    if (interspersed)
		known_args[known_count++] = arg;
	    i++;
	    continue;
	}
	if (end_options || arg[0] != '-' || !arg[1]) {
	    if (!interspersed) {
		ret = i;
		goto done;
	    }
	    operand_args[operand_count++] = arg;
	    i++;
	    continue;
	}

	if (bu_opt_syntax) {
	    option = cmd_schema_bu_opt_option(schema, arg, &opt_attached,
		&opt_short_tail);
	    if (!option) {
		if (pass_unknown) {
		    if (!interspersed) {
			ret = i;
			goto done;
		    }
		    operand_args[operand_count++] = arg;
		    i++;
		    continue;
		}
		if (msg)
		    bu_vls_printf(msg, "unknown option: %s\n", arg);
		goto done;
	    }
	    binding = cmd_schema_binding(schema, option, bindings);
	    if (!binding)
		goto done;

	    if (opt_short_tail) {
		size_t cluster_start = 1;

		/* The bu_opt adapter records whether a descriptor is a flag, so no
		 * speculative callback is needed for standard flags.  An opaque
		 * opaque callback remains inherently ambiguous; invoke it once with
		 * real storage and use its return value as the historical contract. */
		if (option->arg_requirement != BU_CMD_ARG_NONE) {
		    int used;

		    if (!binding->opt_process) {
			if (msg)
			    bu_vls_printf(msg,
				"option argument processor is missing: -%c\n", arg[1]);
			goto done;
		    }
		    {
			size_t available = (size_t)(argc - i);
			const char *first = opt_short_tail[0] == '=' ?
			    opt_short_tail + 1 : opt_short_tail;
			const char **opt_argv = (const char **)bu_calloc(available,
			    sizeof(*opt_argv), "bu_opt attached option arguments");

			opt_argv[0] = first;
			for (size_t ai = 1; ai < available; ai++)
			    opt_argv[ai] = argv[i + (int)ai];
			used = binding->opt_process(msg, available, opt_argv,
			    binding->storage);
			bu_free((void *)opt_argv,
			    "bu_opt attached option arguments");
			if (used < 0 || used > (int)available) {
			    if (msg)
				bu_vls_printf(msg,
				    "invalid attached argument for option: %s\n", arg);
			    goto done;
			}
			if (used > 0) {
			    if (interspersed) {
				known_args[known_count++] = arg;
				for (int ai = 1; ai < used; ai++)
				    known_args[known_count++] = argv[i + ai];
			    }
			    i += used;
			    continue;
			}
			cluster_start = 2;
		    }
		}

		/* Validate the entire cluster before mutating any option storage.
		 * Under bu_opt's interspersed policy a partially-known word is an
		 * untouched pass-through operand. */
		for (size_t ci = cluster_start; arg[ci]; ci++) {
		    char short_name[2] = {arg[ci], '\0'};
		    const struct bu_cmd_option *flag =
			cmd_schema_find_bu_opt_name(schema, short_name);
		    const struct bu_cmd_parse_binding *flag_binding =
			cmd_schema_binding(schema, flag, bindings);

		    if (!flag || !flag_binding) {
			if (pass_unknown) {
			    if (!interspersed) {
				ret = i;
				goto done;
			    }
			    operand_args[operand_count++] = arg;
			    i++;
			    goto bu_opt_next_arg;
			}
			if (msg)
			    bu_vls_printf(msg, "unknown option: -%c\n", arg[ci]);
			goto done;
		    }
		}

		for (size_t ci = cluster_start; arg[ci]; ci++) {
		    char short_name[2] = {arg[ci], '\0'};
		    const struct bu_cmd_option *flag =
			cmd_schema_find_bu_opt_name(schema, short_name);
		    const struct bu_cmd_parse_binding *flag_binding =
			cmd_schema_binding(schema, flag, bindings);
		    int flag_used = 0;

		    if (flag_binding->opt_process)
			flag_used = flag_binding->opt_process(msg, 0, NULL,
			    flag_binding->storage);
		    else if (flag_binding->storage)
			*((int *)flag_binding->storage) = 1;
		    if (flag_used != 0) {
			if (msg)
			    bu_vls_printf(msg,
				"option requires an argument: -%c\n", arg[ci]);
			goto done;
		    }
		}
		if (interspersed)
		    known_args[known_count++] = arg;
		i++;
	bu_opt_next_arg:
		continue;
	    }

	    if (!binding->opt_process) {
		if (opt_attached) {
		    if (msg)
			bu_vls_printf(msg,
			    "option does not take an argument: %s\n", arg);
		    goto done;
		}
		if (binding->storage)
		    *((int *)binding->storage) = 1;
		if (interspersed)
		    known_args[known_count++] = arg;
		i++;
		continue;
	    }

	    if (opt_attached) {
		size_t available = (size_t)(argc - i);
		const char **opt_argv = (const char **)bu_calloc(available,
		    sizeof(*opt_argv), "bu_opt attached option arguments");
		int used;

		opt_argv[0] = opt_attached;
		for (size_t ai = 1; ai < available; ai++)
		    opt_argv[ai] = argv[i + (int)ai];
		used = binding->opt_process(msg, available, opt_argv,
		    binding->storage);
		bu_free((void *)opt_argv,
		    "bu_opt attached option arguments");
		if (used <= 0) {
		    if (msg)
			bu_vls_printf(msg,
			    "invalid attached argument for option: %s\n", arg);
		    goto done;
		}
		if (interspersed)
		    known_args[known_count++] = arg;
		if (used > (int)available)
		    goto done;
		if (interspersed) {
		    for (int ai = 1; ai < used; ai++)
			known_args[known_count++] = argv[i + ai];
		}
		i += used;
		continue;
	    }

	    {
		int used = binding->opt_process(msg, (size_t)(argc - i - 1),
		    argv + i + 1, binding->storage);
		if (used < 0 || used > argc - i - 1) {
		    if (msg)
			bu_vls_printf(msg, "invalid argument for option: %s\n", arg);
		    goto done;
		}
		if (interspersed) {
		    known_args[known_count++] = arg;
		    for (int ai = 0; ai < used; ai++)
			known_args[known_count++] = argv[i + 1 + ai];
		}
		i += used + 1;
		continue;
	    }
	}

	longopt = arg[1] == '-';
	name = arg + (longopt ? 2 : 1);
	eq = strchr(name, '=');
	if (eq) {
	    size_t name_len = (size_t)(eq - name);
	    char *name_copy = (char *)bu_malloc(name_len + 1, "command schema option name");
	    memcpy(name_copy, name, name_len);
	    name_copy[name_len] = '\0';
	    option = cmd_schema_find_option(schema, name_copy, longopt);
	    bu_free(name_copy, "command schema option name");
	} else {
	    option = cmd_schema_find_option(schema, name, longopt);
	}
	if (!option && !longopt && !eq)
	    option = cmd_schema_attached_short_option(schema, arg, &short_attached);
	if (!option) {
	    if (cmd_schema_dash_numeric_operand_valid(schema, operand_count, arg)) {
		if (!interspersed) {
		    ret = i;
		    goto done;
		}
		operand_args[operand_count++] = arg;
		i++;
		continue;
	    }
	    int cluster = cmd_schema_short_flag_cluster(schema, arg, data, msg, 1);
	    if (cluster > 0) {
		if (interspersed)
		    known_args[known_count++] = arg;
		i++;
		continue;
	    }
	    if (cluster < 0)
		goto done;
	    if (pass_unknown) {
		if (!interspersed) {
		    ret = i;
		    goto done;
		}
		operand_args[operand_count++] = arg;
		i++;
		continue;
	    }
	    if (msg)
		bu_vls_printf(msg, "unknown option: %s\n", arg);
	    goto done;
	}
	if (option->arg_requirement == BU_CMD_ARG_NONE) {
	    if (eq) {
		if (msg)
		    bu_vls_printf(msg, "option does not take an argument: %s\n", arg);
		goto done;
	    }
	    if (cmd_schema_set_value(option, data, NULL, msg) != 0)
		goto done;
	    if (interspersed)
		known_args[known_count++] = arg;
	    i++;
	    continue;
	}
	if (short_attached) {
	    if (!short_attached[0] ||
		cmd_schema_apply_option_arguments(option, data, 1,
		    &short_attached, msg) != 0)
		goto done;
	    if (interspersed)
		known_args[known_count++] = arg;
	    i++;
	    continue;
	}
	if (eq) {
	    const char *value = eq + 1;
	    if (!cmd_schema_option_attached_allowed(option)) {
		if (msg)
		    bu_vls_printf(msg, "option does not accept an attached multi-token argument: %s\n", arg);
		goto done;
	    }
	    if (cmd_schema_apply_option_arguments(option, data, 1, &value, msg) != 0)
		goto done;
	    if (interspersed)
		known_args[known_count++] = arg;
	    i++;
	    continue;
	}

	size_t available = (size_t)(argc - i - 1);
	size_t min_tokens = cmd_schema_option_min_tokens(option);
	size_t consume = cmd_schema_option_argument_count(option, available, argv + i + 1);
	if (consume < min_tokens) {
	    if (msg)
		bu_vls_printf(msg, "option argument expected: %s\n", arg);
	    goto done;
	}
	if (cmd_schema_apply_option_arguments(option, data, consume, argv + i + 1, msg) != 0)
	    goto done;
	if (interspersed) {
	    known_args[known_count++] = arg;
	    for (size_t ai = 0; ai < consume; ai++)
		known_args[known_count++] = argv[i + 1 + (int)ai];
	}
	i += (int)(consume + 1);
    }

    ret = i;

done:
    if (ret >= 0 && interspersed) {
	if (leftovers_first) {
	    for (size_t ai = 0; ai < operand_count; ai++)
		argv[ai] = operand_args[ai];
	    for (size_t ai = 0; ai < known_count; ai++)
		argv[operand_count + ai] = known_args[ai];
	    ret = (int)operand_count;
	} else {
	    for (size_t ai = 0; ai < known_count; ai++)
		argv[ai] = known_args[ai];
	    for (size_t ai = 0; ai < operand_count; ai++)
		argv[known_count + ai] = operand_args[ai];
	    ret = (int)known_count;
	}
    }
    if (known_args)
	bu_free((void *)known_args, "interspersed command option arguments");
    if (operand_args)
	bu_free((void *)operand_args, "interspersed command operands");
    return ret;
}


int
bu_cmd_schema_parse(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[])
{
    return _bu_cmd_schema_parse_bound(schema, data, msg, argc, argv, NULL,
	BU_CMD_PARSE_INTERNAL_NONE);
}


int
bu_cmd_schema_parse_known(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[])
{
    return _bu_cmd_schema_parse_bound(schema, data, msg, argc, argv, NULL,
	BU_CMD_PARSE_INTERNAL_PASS_UNKNOWN);
}


int
bu_cmd_schema_parse_complete(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[])
{
    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
    int operand_index;

    if (argc < 0 || (argc > 0 && !argv))
	return -1;

    if (bu_cmd_schema_validate(schema, (size_t)argc, argv, (size_t)argc, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	if (msg && result.hint)
	    bu_vls_printf(msg, "%s\n", result.hint);
	bu_cmd_validate_result_clear(&result);
	return -1;
    }
    bu_cmd_validate_result_clear(&result);
    operand_index = bu_cmd_schema_parse(schema, data, msg, argc, argv);
    return operand_index < 0 ? -1 : operand_index;
}


static void
cmd_schema_option_spelling(struct bu_vls *spellings, const char *name,
	int longopt, const struct bu_cmd_option *value_option)
{
    const char *argument;

    if (!spellings || BU_STR_EMPTY(name) || !value_option)
	return;
    if (bu_vls_strlen(spellings))
	bu_vls_strcat(spellings, ", ");
    bu_vls_printf(spellings, longopt ? "--%s" : "-%s", name);
    argument = value_option->argument;
    if (value_option->arg_requirement == BU_CMD_ARG_NONE || BU_STR_EMPTY(argument))
	return;
    if (value_option->arg_requirement == BU_CMD_ARG_OPTIONAL && argument[0] != '[')
	bu_vls_printf(spellings, " [%s]", argument);
    else
	bu_vls_printf(spellings, " %s", argument);
}


static char *
cmd_schema_describe_selected(const struct bu_cmd_schema *schema,
	const char * const *selected, int include_constraints)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    size_t i = 0;

    if (!schema || !schema->options)
	return NULL;

    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	struct bu_vls spellings = BU_VLS_INIT_ZERO;

	if (option->alias_of || option->hidden) {
	    i++;
	    continue;
	}
	if (selected) {
	    size_t selected_i;
	    int found = 0;
	    for (selected_i = 0; selected[selected_i]; selected_i++) {
		if (BU_STR_EQUAL(bu_cmd_option_canonical(option), selected[selected_i])) {
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		i++;
		continue;
	    }
	}
	cmd_schema_option_spelling(&spellings, option->shortopt, 0, option);
	cmd_schema_option_spelling(&spellings, option->longopt, 1, option);
	for (size_t ai = 0; bu_cmd_option_is_valid(&schema->options[ai]); ai++) {
	    const struct bu_cmd_option *alias = &schema->options[ai];
	    if (alias->hidden || !alias->alias_of ||
		    !BU_STR_EQUAL(alias->alias_of,
			bu_cmd_option_canonical(option)))
		continue;
	    cmd_schema_option_spelling(&spellings, alias->shortopt, 0, option);
	    cmd_schema_option_spelling(&spellings, alias->longopt, 1, option);
	}
	bu_vls_printf(&out, "  %-34s %s\n", bu_vls_addr(&spellings),
		option->help ? option->help : "");
	bu_vls_free(&spellings);
	i++;
    }

    if (include_constraints && schema->validation.constraints) {
	for (i = 0; schema->validation.constraints[i].options; i++) {
	    const char *hint = schema->validation.constraints[i].hint;
	    if (hint && hint[0])
		bu_vls_printf(&out, "  %-34s %s\n", "Constraint:", hint);
	}
    }

    char *result = bu_strdup(bu_vls_addr(&out));
    bu_vls_free(&out);
    return result;
}


char *
bu_cmd_schema_describe_selected(const struct bu_cmd_schema *schema,
	const char * const *selected)
{
    return cmd_schema_describe_selected(schema, selected, 1);
}


char *
bu_cmd_schema_describe(const struct bu_cmd_schema *schema)
{
    return bu_cmd_schema_describe_selected(schema, NULL);
}


static void
cmd_schema_help_domain_heading(struct bu_vls *out, int *heading)
{
    if (!*heading) {
	bu_vls_strcat(out, "\nAccepted values:\n");
	*heading = 1;
    }
}


static void
cmd_schema_case_condition_help(struct bu_vls *out,
	const struct bu_cmd_schema *schema,
	const struct bu_cmd_schema_case *cmd_case)
{
    size_t option_count = 0;

    if (!out || !cmd_case)
	return;
    if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS) {
	bu_vls_strcat(out, "otherwise");
	return;
    }
    while (cmd_case->options && cmd_case->options[option_count])
	option_count++;
    switch (cmd_case->condition) {
	case BU_CMD_CONDITION_ANY_OPTION_PRESENT:
	    bu_vls_strcat(out, "when any of ");
	    break;
	case BU_CMD_CONDITION_NO_OPTION_PRESENT:
	    bu_vls_strcat(out, "when none of ");
	    break;
	case BU_CMD_CONDITION_ALL_OPTIONS_PRESENT:
	    bu_vls_strcat(out, option_count > 1 ? "when all of " : "when ");
	    break;
	default:
	    bu_vls_strcat(out, "when ");
	    break;
    }
    for (size_t oi = 0; oi < option_count; oi++) {
	const struct bu_cmd_option *option = cmd_schema_find_canonical(schema,
	    cmd_case->options[oi]);
	if (oi)
	    bu_vls_strcat(out, ", ");
	if (option && !BU_STR_EMPTY(option->longopt))
	    bu_vls_printf(out, "--%s", option->longopt);
	else if (option && !BU_STR_EMPTY(option->shortopt))
	    bu_vls_printf(out, "-%s", option->shortopt);
	else
	    bu_vls_printf(out, "--%s", cmd_case->options[oi]);
    }
}


static void
cmd_schema_help_keywords(struct bu_vls *out, int *heading, const char *label,
	const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values)
{
    if (!out || !heading || BU_STR_EMPTY(label) || (!keywords && !keyword_values))
	return;

    cmd_schema_help_domain_heading(out, heading);
    bu_vls_printf(out, "  %s:\n", label);
    if (keyword_values) {
	for (size_t i = 0; keyword_values[i].canonical; i++) {
	    const struct bu_cmd_value_keyword *keyword = &keyword_values[i];
	    bu_vls_printf(out, "    %s", keyword->canonical);
	    if (keyword->aliases && keyword->aliases[0]) {
		bu_vls_strcat(out, " (aliases: ");
		for (size_t ai = 0; keyword->aliases[ai]; ai++) {
		    if (ai)
			bu_vls_strcat(out, ", ");
		    bu_vls_strcat(out, keyword->aliases[ai]);
		}
		bu_vls_putc(out, ')');
	    }
	    if (!BU_STR_EMPTY(keyword->description))
		bu_vls_printf(out, " - %s", keyword->description);
	    bu_vls_putc(out, '\n');
	}
	return;
    }

    bu_vls_strcat(out, "    ");
    for (size_t i = 0; keywords[i]; i++) {
	if (i)
	    bu_vls_strcat(out, ", ");
	bu_vls_strcat(out, keywords[i]);
    }
    bu_vls_putc(out, '\n');
}


static void
cmd_schema_help_range(struct bu_vls *out, int *heading, const char *label,
	const struct bu_cmd_value_range *range)
{
    if (!out || !heading || BU_STR_EMPTY(label) || !range ||
	    range->kind == BU_CMD_RANGE_NONE)
	return;

    cmd_schema_help_domain_heading(out, heading);
    bu_vls_printf(out, "  %s: ", label);
    if (!range->has_minimum && !range->has_maximum) {
	bu_vls_strcat(out, range->kind == BU_CMD_RANGE_INTEGER ? "integer\n" : "number\n");
	return;
    }
    if (range->has_minimum) {
	bu_vls_printf(out, "value %s ", range->minimum_inclusive ? ">=" : ">");
	if (range->kind == BU_CMD_RANGE_INTEGER)
	    bu_vls_printf(out, "%ld", range->integer_minimum);
	else
	    bu_vls_printf(out, "%.17g", (double)range->number_minimum);
    }
    if (range->has_minimum && range->has_maximum)
	bu_vls_strcat(out, " and ");
    if (range->has_maximum) {
	if (!range->has_minimum)
	    bu_vls_strcat(out, "value ");
	bu_vls_printf(out, "%s ", range->maximum_inclusive ? "<=" : "<");
	if (range->kind == BU_CMD_RANGE_INTEGER)
	    bu_vls_printf(out, "%ld", range->integer_maximum);
	else
	    bu_vls_printf(out, "%.17g", (double)range->number_maximum);
    }
    bu_vls_putc(out, '\n');
}


static void
cmd_schema_usage_atom(struct bu_vls *out, const char *atom, size_t minimum,
	size_t maximum)
{
    if (!out || BU_STR_EMPTY(atom) || maximum == 0)
	return;
    for (size_t i = 0; i < minimum; i++)
	bu_vls_printf(out, " %s", atom);
    if (maximum == BU_CMD_COUNT_UNLIMITED) {
	bu_vls_printf(out, " [%s ...]", atom);
	return;
    }
    for (size_t i = minimum; i < maximum; i++)
	bu_vls_printf(out, " [%s]", atom);
}


static void
cmd_schema_usage_operand(struct bu_vls *out, const struct bu_cmd_operand *operand)
{
    size_t minimum;
    size_t maximum;

    if (!out || !operand)
	return;
    minimum = operand->min_count;
    maximum = operand->max_count;

    /* A shaped operand names one semantic value which may occupy more than
     * one argv token (for example, a packed vector or three XYZ words).
     * When its cardinality exactly describes that shape, show the semantic
     * value once rather than suggesting several independent values. */
    if (operand->shape && operand->shape->max_tokens > 1 &&
	operand->shape->max_tokens != BU_CMD_COUNT_UNLIMITED &&
	maximum == operand->shape->max_tokens &&
	(minimum == 0 || minimum == operand->shape->min_tokens)) {
	minimum = minimum ? 1 : 0;
	maximum = 1;
    }
    cmd_schema_usage_atom(out, operand->name, minimum, maximum);
}


static void
cmd_schema_help_arg_variants(struct bu_vls *out, int *heading,
	const char *label, const struct bu_cmd_arg_shape *shape)
{
    if (!out || !heading || BU_STR_EMPTY(label) || !shape || !shape->variants)
	return;

    if (!*heading) {
	bu_vls_strcat(out, "\nArgument forms:\n");
	*heading = 1;
    }
    bu_vls_printf(out, "  %s:\n", label);
    for (size_t i = 0; shape->variants[i].name; i++) {
	const struct bu_cmd_arg_variant *variant = &shape->variants[i];
	bu_vls_printf(out, "    %-16s", variant->name);
	if (!BU_STR_EMPTY(variant->syntax))
	    bu_vls_printf(out, " %-20s", variant->syntax);
	else
	    bu_vls_printf(out, " %-20s", "(no argument)");
	if (!BU_STR_EMPTY(variant->help))
	    bu_vls_printf(out, " %s", variant->help);
	bu_vls_putc(out, '\n');
    }
}


char *
bu_cmd_schema_usage(const struct bu_cmd_schema *schema, const char *invocation)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    int have_options = 0;

    if (!schema)
	return NULL;
    if (schema->validation.cases) {
	for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	    const struct bu_cmd_schema_case *cmd_case = &schema->validation.cases[ci];
	    struct bu_cmd_schema view = *schema;
	    char *case_usage;

	    view.operands = cmd_case->operands;
	    view.operand_groups = cmd_case->operand_groups;
	    view.validation.cases = NULL;
	    case_usage = bu_cmd_schema_usage(&view, invocation);
	    if (case_usage) {
		bu_vls_strcat(&out, case_usage);
		bu_free(case_usage, "command case usage");
	    }
	}
	{
	    char *result = bu_vls_strdup(&out);
	    bu_vls_free(&out);
	    return result;
	}
    }
    if (BU_STR_EMPTY(invocation))
	invocation = schema->name;
    bu_vls_printf(&out, "Usage: %s", invocation ? invocation : "");
    if (schema->options) {
	for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    if (!option->alias_of && !option->hidden) {
		have_options = 1;
		break;
	    }
	}
    }
    if (have_options)
	bu_vls_strcat(&out, " [options]");
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++)
	    cmd_schema_usage_operand(&out, &schema->operands[i]);
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    struct bu_vls atom = BU_VLS_INIT_ZERO;
	    bu_vls_putc(&atom, '(');
	    for (size_t ri = 0; group->roles && group->roles[ri].name; ri++) {
		if (ri)
		    bu_vls_putc(&atom, ' ');
		bu_vls_strcat(&atom, group->roles[ri].name);
	    }
	    bu_vls_putc(&atom, ')');
	    cmd_schema_usage_atom(&out, bu_vls_cstr(&atom), group->min_count,
		group->max_count);
	    bu_vls_free(&atom);
	}
    }
    bu_vls_putc(&out, '\n');
    char *result = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return result;
}


char *
bu_cmd_schema_help(const struct bu_cmd_schema *schema, const char *invocation)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    struct bu_vls label = BU_VLS_INIT_ZERO;
    char *usage;
    char *options;
    int domain_heading = 0;
    int variants_heading = 0;

    if (!schema)
	return NULL;
    usage = bu_cmd_schema_usage(schema, invocation);
    if (usage) {
	bu_vls_strcat(&out, usage);
	bu_free(usage, "command schema usage");
    }
    if (!BU_STR_EMPTY(schema->help))
	bu_vls_printf(&out, "\n%s\n", schema->help);

    options = cmd_schema_describe_selected(schema, NULL, 0);
    if (options && options[0])
	bu_vls_printf(&out, "\nOptions:\n%s", options);
    if (options)
	bu_free(options, "command schema option help");

    if (schema->operands && schema->operands[0].name) {
	bu_vls_strcat(&out, "\nOperands:\n");
	for (size_t i = 0; schema->operands[i].name; i++)
	    bu_vls_printf(&out, "  %-34s %s\n", schema->operands[i].name,
		schema->operands[i].help ? schema->operands[i].help : "");
    }
    if (schema->operand_groups && schema->operand_groups[0].name) {
	bu_vls_strcat(&out, "\nRepeated groups:\n");
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    bu_vls_printf(&out, "  %-34s %s\n", group->name,
		group->help ? group->help : "");
	    for (size_t ri = 0; group->roles && group->roles[ri].name; ri++)
		bu_vls_printf(&out, "    %-32s %s\n", group->roles[ri].name,
		    group->roles[ri].help ? group->roles[ri].help : "");
	}
    }
    if (schema->options) {
	for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
	    if (option->alias_of || option->hidden || !shape || !shape->variants)
		continue;
	    bu_vls_trunc(&label, 0);
	    if (!BU_STR_EMPTY(option->longopt))
		bu_vls_printf(&label, "--%s", option->longopt);
	    else if (!BU_STR_EMPTY(option->shortopt))
		bu_vls_printf(&label, "-%s", option->shortopt);
	    cmd_schema_help_arg_variants(&out, &variants_heading,
		bu_vls_cstr(&label), shape);
	}
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++)
	    cmd_schema_help_arg_variants(&out, &variants_heading,
		schema->operands[i].name, schema->operands[i].shape);
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    for (size_t ri = 0; group->roles && group->roles[ri].name; ri++) {
		const struct bu_cmd_operand *role = &group->roles[ri];
		bu_vls_sprintf(&label, "%s.%s", group->name, role->name);
		cmd_schema_help_arg_variants(&out, &variants_heading,
		    bu_vls_cstr(&label), role->shape);
	    }
	}
    }
    if (schema->validation.cases) {
	for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	    const struct bu_cmd_schema_case *cmd_case =
		&schema->validation.cases[ci];
	    for (size_t oi = 0; cmd_case->operands &&
		    cmd_case->operands[oi].name; oi++) {
		const struct bu_cmd_operand *operand = &cmd_case->operands[oi];
		if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS)
		    bu_vls_sprintf(&label, "%s", operand->name);
		else
		    bu_vls_sprintf(&label, "%s.%s", cmd_case->name, operand->name);
		cmd_schema_help_arg_variants(&out, &variants_heading,
		    bu_vls_cstr(&label), operand->shape);
	    }
	    for (size_t gi = 0; cmd_case->operand_groups &&
		    cmd_case->operand_groups[gi].name; gi++) {
		const struct bu_cmd_operand_group *group =
		    &cmd_case->operand_groups[gi];
		for (size_t ri = 0; group->roles && group->roles[ri].name; ri++) {
		    const struct bu_cmd_operand *role = &group->roles[ri];
		    if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS)
			bu_vls_sprintf(&label, "%s.%s", group->name, role->name);
		    else
			bu_vls_sprintf(&label, "%s.%s.%s", cmd_case->name,
			    group->name, role->name);
		    cmd_schema_help_arg_variants(&out, &variants_heading,
			bu_vls_cstr(&label), role->shape);
		}
	    }
	}
    }
    if (schema->options) {
	for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    if (option->alias_of || option->hidden)
		continue;
	    bu_vls_trunc(&label, 0);
	    if (!BU_STR_EMPTY(option->longopt))
		bu_vls_printf(&label, "--%s", option->longopt);
	    else if (!BU_STR_EMPTY(option->shortopt))
		bu_vls_printf(&label, "-%s", option->shortopt);
	    cmd_schema_help_keywords(&out, &domain_heading, bu_vls_cstr(&label),
		option->value_keywords, option->keyword_values);
	    cmd_schema_help_range(&out, &domain_heading, bu_vls_cstr(&label),
		&option->range);
	}
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    cmd_schema_help_keywords(&out, &domain_heading, operand->name,
		operand->value_keywords, operand->keyword_values);
	    cmd_schema_help_range(&out, &domain_heading, operand->name,
		&operand->range);
	}
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    for (size_t ri = 0; group->roles && group->roles[ri].name; ri++) {
		const struct bu_cmd_operand *role = &group->roles[ri];
		bu_vls_sprintf(&label, "%s.%s", group->name, role->name);
		cmd_schema_help_keywords(&out, &domain_heading, bu_vls_cstr(&label),
		    role->value_keywords, role->keyword_values);
		cmd_schema_help_range(&out, &domain_heading, bu_vls_cstr(&label),
		    &role->range);
	    }
	}
    }
    if (schema->validation.cases) {
	for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	    const struct bu_cmd_schema_case *cmd_case =
		&schema->validation.cases[ci];
	    for (size_t oi = 0; cmd_case->operands &&
		    cmd_case->operands[oi].name; oi++) {
		const struct bu_cmd_operand *operand = &cmd_case->operands[oi];
		if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS)
		    bu_vls_sprintf(&label, "%s", operand->name);
		else
		    bu_vls_sprintf(&label, "%s.%s", cmd_case->name, operand->name);
		cmd_schema_help_keywords(&out, &domain_heading, bu_vls_cstr(&label),
		    operand->value_keywords, operand->keyword_values);
		cmd_schema_help_range(&out, &domain_heading, bu_vls_cstr(&label),
		    &operand->range);
	    }
	    for (size_t gi = 0; cmd_case->operand_groups &&
		    cmd_case->operand_groups[gi].name; gi++) {
		const struct bu_cmd_operand_group *group =
		    &cmd_case->operand_groups[gi];
		for (size_t ri = 0; group->roles && group->roles[ri].name; ri++) {
		    const struct bu_cmd_operand *role = &group->roles[ri];
		    if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS)
			bu_vls_sprintf(&label, "%s.%s", group->name, role->name);
		    else
			bu_vls_sprintf(&label, "%s.%s.%s", cmd_case->name,
			    group->name, role->name);
		    cmd_schema_help_keywords(&out, &domain_heading,
			bu_vls_cstr(&label), role->value_keywords,
			role->keyword_values);
		    cmd_schema_help_range(&out, &domain_heading,
			bu_vls_cstr(&label), &role->range);
		}
	    }
	}
    }
    if (schema->validation.constraints) {
	int heading = 0;
	for (size_t i = 0; schema->validation.constraints[i].options; i++) {
	    const char *hint = schema->validation.constraints[i].hint;
	    if (BU_STR_EMPTY(hint))
		continue;
	    if (!heading) {
		bu_vls_strcat(&out, "\nConstraints:\n");
		heading = 1;
	    }
	    bu_vls_printf(&out, "  %s\n", hint);
	}
    }
    if (schema->validation.cases) {
	bu_vls_strcat(&out, "\nForms:\n");
	for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	    const struct bu_cmd_schema_case *cmd_case =
		&schema->validation.cases[ci];
	    struct bu_vls condition = BU_VLS_INIT_ZERO;
	    cmd_schema_case_condition_help(&condition, schema, cmd_case);
	    bu_vls_printf(&out, "  %-34s %s\n", bu_vls_cstr(&condition),
		cmd_case->help ? cmd_case->help : "");
	    bu_vls_free(&condition);
	}
    }
    char *result = bu_vls_strdup(&out);
    bu_vls_free(&label);
    bu_vls_free(&out);
    return result;
}


int
bu_cmd_schema_help_append(struct bu_vls *output,
	const struct bu_cmd_schema *schema, const char *invocation)
{
    char *help;

    if (!output)
	return -1;
    help = bu_cmd_schema_help(schema, invocation);
    if (!help)
	return -1;
    bu_vls_strcat(output, help);
    bu_free(help, "command schema help");
    return 0;
}


void
bu_cmd_json_string(struct bu_vls *out, const char *value)
{
    const char *p = value ? value : "";

    bu_vls_putc(out, '"');
    while (*p) {
	unsigned char c = (unsigned char)*p;
	if (*p == '"' || *p == '\\') {
	    bu_vls_putc(out, '\\');
	    bu_vls_putc(out, *p);
	} else if (*p == '\n') {
	    bu_vls_strcat(out, "\\n");
	} else if (*p == '\r') {
	    bu_vls_strcat(out, "\\r");
	} else if (*p == '\t') {
	    bu_vls_strcat(out, "\\t");
	} else if (c < 0x20) {
	    bu_vls_printf(out, "\\u%04x", (unsigned int)c);
	} else {
	    bu_vls_putc(out, *p);
	}
	p++;
    }
    bu_vls_putc(out, '"');
}


static void
cmd_schema_json_keyword_values(struct bu_vls *out, const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values)
{
    size_t i = 0;

    bu_vls_strcat(out, "\"values\":[");
    if (keyword_values) {
	for (i = 0; keyword_values[i].canonical; i++) {
	    if (i)
		bu_vls_putc(out, ',');
	    bu_cmd_json_string(out, keyword_values[i].canonical);
	}
    } else if (keywords) {
	for (i = 0; keywords[i]; i++) {
	    if (i)
		bu_vls_putc(out, ',');
	    bu_cmd_json_string(out, keywords[i]);
	}
    }
    bu_vls_strcat(out, "],\"keyword_values\":[");
    if (keyword_values) {
	for (i = 0; keyword_values[i].canonical; i++) {
	    const struct bu_cmd_value_keyword *keyword = &keyword_values[i];
	    if (i)
		bu_vls_putc(out, ',');
	    bu_vls_strcat(out, "{\"canonical\":");
	    bu_cmd_json_string(out, keyword->canonical);
	    bu_vls_strcat(out, ",\"aliases\":[");
	    if (keyword->aliases) {
		for (size_t ai = 0; keyword->aliases[ai]; ai++) {
		    if (ai)
			bu_vls_putc(out, ',');
		    bu_cmd_json_string(out, keyword->aliases[ai]);
		}
	    }
	    bu_vls_strcat(out, "],\"description\":");
	    bu_cmd_json_string(out, keyword->description);
	    bu_vls_putc(out, '}');
	}
    }
    bu_vls_putc(out, ']');
}


static void
cmd_schema_json_range(struct bu_vls *out,
	const struct bu_cmd_value_range *range)
{
    if (!range || range->kind == BU_CMD_RANGE_NONE) {
	bu_vls_strcat(out, "null");
	return;
    }

    bu_vls_strcat(out, "{\"kind\":");
    bu_cmd_json_string(out, range->kind == BU_CMD_RANGE_INTEGER ?
	"integer" : "number");
    bu_vls_strcat(out, ",\"minimum\":");
    if (!range->has_minimum)
	bu_vls_strcat(out, "null");
    else if (range->kind == BU_CMD_RANGE_INTEGER)
	bu_vls_printf(out, "%ld", range->integer_minimum);
    else
	bu_vls_printf(out, "%.17g", (double)range->number_minimum);
    bu_vls_strcat(out, ",\"maximum\":");
    if (!range->has_maximum)
	bu_vls_strcat(out, "null");
    else if (range->kind == BU_CMD_RANGE_INTEGER)
	bu_vls_printf(out, "%ld", range->integer_maximum);
    else
	bu_vls_printf(out, "%.17g", (double)range->number_maximum);
    bu_vls_printf(out, ",\"minimum_inclusive\":%s,\"maximum_inclusive\":%s}",
	range->minimum_inclusive ? "true" : "false",
	range->maximum_inclusive ? "true" : "false");
}


static const char *
cmd_schema_value_name(bu_cmd_value_t value_type)
{
    switch (value_type) {
	case BU_CMD_VALUE_FLAG: return "flag";
	case BU_CMD_VALUE_BOOL: return "bool";
	case BU_CMD_VALUE_INTEGER: return "integer";
	case BU_CMD_VALUE_HEX_INTEGER: return "hex_integer";
	case BU_CMD_VALUE_LONG: return "long";
	case BU_CMD_VALUE_HEX_LONG: return "hex_long";
	case BU_CMD_VALUE_NUMBER: return "number";
	case BU_CMD_VALUE_CHAR: return "char";
	case BU_CMD_VALUE_VECTOR: return "vector";
	case BU_CMD_VALUE_MATRIX: return "matrix";
	case BU_CMD_VALUE_COLOR: return "color";
	case BU_CMD_VALUE_KEYWORD: return "keyword";
	case BU_CMD_VALUE_STRING: return "string";
	case BU_CMD_VALUE_VLS: return "vls";
	case BU_CMD_VALUE_DB_OBJECT: return "db_object";
	case BU_CMD_VALUE_DB_PATH: return "db_path";
	case BU_CMD_VALUE_FILE: return "file";
	case BU_CMD_VALUE_RAW: return "raw";
	case BU_CMD_VALUE_CUSTOM: return "custom";
	default: break;
    }
    return "unknown";
}

static const char *
cmd_schema_policy_name(bu_cmd_parse_policy_t policy)
{
    switch (policy) {
	case BU_CMD_PARSE_INTERSPERSED: return "interspersed";
	case BU_CMD_PARSE_OPTIONS_FIRST: return "options_first";
	case BU_CMD_PARSE_STOP_AT_FIRST_OPERAND: return "stop_at_first_operand";
	default: break;
    }
    return "unknown";
}


static const char *
cmd_schema_arg_requirement_name(bu_cmd_arg_requirement_t requirement)
{
    switch (requirement) {
	case BU_CMD_ARG_REQUIRED: return "required";
	case BU_CMD_ARG_OPTIONAL: return "optional";
	case BU_CMD_ARG_NONE: return "none";
	default: break;
    }
    return "unknown";
}


static const char *
cmd_schema_arg_shape_name(bu_cmd_arg_shape_kind_t kind)
{
    switch (kind) {
	case BU_CMD_ARG_SHAPE_SCALAR: return "scalar";
	case BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE: return "token_sequence";
	case BU_CMD_ARG_SHAPE_COMMA_LIST: return "comma_list";
	case BU_CMD_ARG_SHAPE_KEY_VALUE_LIST: return "key_value_list";
	case BU_CMD_ARG_SHAPE_AXIS_KEYED: return "axis_keyed";
	case BU_CMD_ARG_SHAPE_RANGE_PATTERN: return "range_pattern";
	case BU_CMD_ARG_SHAPE_RGB: return "rgb";
	case BU_CMD_ARG_SHAPE_COLOR: return "color";
	case BU_CMD_ARG_SHAPE_VECTOR3: return "vector3";
	case BU_CMD_ARG_SHAPE_CUSTOM: return "custom";
	default: break;
    }
    return "unknown";
}


static const char *
cmd_schema_constraint_condition_name(bu_cmd_constraint_condition_t condition)
{
    switch (condition) {
	case BU_CMD_CONDITION_ALWAYS: return "always";
	case BU_CMD_CONDITION_ANY_OPTION_PRESENT: return "any_option_present";
	case BU_CMD_CONDITION_NO_OPTION_PRESENT: return "no_option_present";
	case BU_CMD_CONDITION_ALL_OPTIONS_PRESENT: return "all_options_present";
	default: break;
    }
    return "unknown";
}


static const char *
cmd_schema_constraint_kind_name(bu_cmd_constraint_kind_t kind)
{
    switch (kind) {
	case BU_CMD_CONSTRAINT_OPTION_COUNT: return "option_count";
	case BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT: return "option_occurrence_count";
	case BU_CMD_CONSTRAINT_OPERAND_COUNT: return "operand_count";
	case BU_CMD_CONSTRAINT_OPTION_REQUIRES: return "option_requires";
	case BU_CMD_CONSTRAINT_OPTION_CONFLICTS: return "option_conflicts";
	default: break;
    }
    return "unknown";
}


static void
cmd_schema_json_arg_shape(struct bu_vls *out,
	const struct bu_cmd_arg_shape *shape)
{
    if (!shape) {
	bu_vls_strcat(out, "null");
	return;
    }

    bu_vls_strcat(out, "{\"kind\":");
    bu_cmd_json_string(out, cmd_schema_arg_shape_name(shape->kind));
    bu_vls_printf(out, ",\"min_tokens\":%lu,\"max_tokens\":",
	(unsigned long)shape->min_tokens);
    if (shape->max_tokens == BU_CMD_COUNT_UNLIMITED)
	bu_vls_strcat(out, "null");
    else
	bu_vls_printf(out, "%lu", (unsigned long)shape->max_tokens);
    bu_vls_strcat(out, ",\"description\":");
    bu_cmd_json_string(out, shape->description);
    bu_vls_strcat(out, ",\"variants\":[");
    if (shape->variants) {
	for (size_t i = 0; shape->variants[i].name; i++) {
	    const struct bu_cmd_arg_variant *variant = &shape->variants[i];
	    if (i)
		bu_vls_putc(out, ',');
	    bu_vls_strcat(out, "{\"name\":");
	    bu_cmd_json_string(out, variant->name);
	    bu_vls_strcat(out, ",\"syntax\":");
	    bu_cmd_json_string(out, variant->syntax);
	    bu_vls_printf(out, ",\"token_count\":%lu,\"help\":",
		(unsigned long)variant->token_count);
	    bu_cmd_json_string(out, variant->help);
	    bu_vls_putc(out, '}');
	}
    }
    bu_vls_strcat(out, "]}");
}


char *
bu_cmd_schema_describe_json(const struct bu_cmd_schema *schema)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    size_t i = 0;
    int comma = 0;

    if (!schema)
	return NULL;

    bu_vls_strcat(&out, "{\"kind\":\"native\",\"name\":");
    bu_cmd_json_string(&out, schema->name);
    bu_vls_strcat(&out, ",\"help\":");
    bu_cmd_json_string(&out, schema->help);
    bu_vls_strcat(&out, ",\"parse_policy\":");
    bu_cmd_json_string(&out, cmd_schema_policy_name(schema->parse_policy));
    bu_vls_strcat(&out, ",\"options\":[");
    if (schema->options) {
	while (bu_cmd_option_is_valid(&schema->options[i])) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    const struct bu_cmd_arg_shape *shape = cmd_schema_option_arg_shape(option);
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"short\":");
	    bu_cmd_json_string(&out, option->shortopt);
	    bu_vls_strcat(&out, ",\"long\":");
	    bu_cmd_json_string(&out, option->longopt);
	    bu_vls_strcat(&out, ",\"canonical\":");
	    bu_cmd_json_string(&out, bu_cmd_option_canonical(option));
	    bu_vls_strcat(&out, ",\"alias_of\":");
	    bu_cmd_json_string(&out, option->alias_of);
	    bu_vls_strcat(&out, ",\"argument\":");
	    bu_cmd_json_string(&out, option->argument);
	    bu_vls_strcat(&out, ",\"argument_requirement\":");
	    bu_cmd_json_string(&out, cmd_schema_arg_requirement_name(option->arg_requirement));
	    bu_vls_strcat(&out, ",\"argument_shape\":");
	    cmd_schema_json_arg_shape(&out, shape);
	    bu_vls_strcat(&out, ",\"type\":");
	    bu_cmd_json_string(&out, cmd_schema_value_name(option->value_type));
	    bu_vls_strcat(&out, ",\"semantic_provider\":");
	    bu_cmd_json_string(&out, option->semantic_provider);
	    bu_vls_strcat(&out, ",");
	    cmd_schema_json_keyword_values(&out, option->value_keywords, option->keyword_values);
	    bu_vls_strcat(&out, ",\"range\":");
	    cmd_schema_json_range(&out, &option->range);
	    bu_vls_printf(&out, ",\"repeat\":%s,\"hidden\":%s,\"help\":",
		option->repeat ? "true" : "false", option->hidden ? "true" : "false");
	    bu_cmd_json_string(&out, option->help);
	    bu_vls_putc(&out, '}');
	    comma = 1;
	    i++;
	}
    }
    bu_vls_strcat(&out, "],\"terminal_options\":[");
    comma = 0;
    if (schema->validation.terminal_flags & BU_CMD_TERMINAL_HELP) {
	bu_cmd_json_string(&out, "help");
	comma = 1;
    }
    if (schema->validation.terminal_flags & BU_CMD_TERMINAL_VERSION) {
	if (comma)
	    bu_vls_putc(&out, ',');
	bu_cmd_json_string(&out, "version");
	comma = 1;
    }
    for (size_t ti = 0; schema->validation.terminal_options &&
	schema->validation.terminal_options[ti]; ti++) {
	if (comma)
	    bu_vls_putc(&out, ',');
	bu_cmd_json_string(&out, schema->validation.terminal_options[ti]);
	comma = 1;
    }
    bu_vls_strcat(&out, "],\"operands\":[");
    comma = 0;
    i = 0;
    if (schema->operands) {
	while (schema->operands[i].name) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"name\":");
	    bu_cmd_json_string(&out, operand->name);
	    bu_vls_printf(&out, ",\"min\":%lu,\"max\":", (unsigned long)operand->min_count);
	    if (operand->max_count == BU_CMD_COUNT_UNLIMITED)
		bu_vls_strcat(&out, "null");
	    else
		bu_vls_printf(&out, "%lu", (unsigned long)operand->max_count);
	    bu_vls_strcat(&out, ",\"help\":");
	    bu_cmd_json_string(&out, operand->help);
	    bu_vls_strcat(&out, ",\"type\":");
	    bu_cmd_json_string(&out, cmd_schema_value_name(operand->value_type));
	    bu_vls_strcat(&out, ",\"shape\":");
	    cmd_schema_json_arg_shape(&out, operand->shape);
	    bu_vls_strcat(&out, ",\"semantic_provider\":");
	    bu_cmd_json_string(&out, operand->semantic_provider);
	    bu_vls_strcat(&out, ",");
	    cmd_schema_json_keyword_values(&out, operand->value_keywords, operand->keyword_values);
	    bu_vls_strcat(&out, ",\"range\":");
	    cmd_schema_json_range(&out, &operand->range);
	    bu_vls_putc(&out, '}');
	    comma = 1;
	    i++;
	}
    }
    bu_vls_strcat(&out, "],\"operand_groups\":[");
    comma = 0;
    if (schema->operand_groups) {
	for (i = 0; schema->operand_groups[i].name; i++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[i];
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"name\":");
	    bu_cmd_json_string(&out, group->name);
	    bu_vls_printf(&out, ",\"min\":%lu,\"max\":",
		(unsigned long)group->min_count);
	    if (group->max_count == BU_CMD_COUNT_UNLIMITED)
		bu_vls_strcat(&out, "null");
	    else
		bu_vls_printf(&out, "%lu", (unsigned long)group->max_count);
	    bu_vls_strcat(&out, ",\"help\":");
	    bu_cmd_json_string(&out, group->help);
	    bu_vls_strcat(&out, ",\"roles\":[");
	    if (group->roles) {
		for (size_t ri = 0; group->roles[ri].name; ri++) {
		    const struct bu_cmd_operand *role = &group->roles[ri];
		    if (ri)
			bu_vls_putc(&out, ',');
		    bu_vls_strcat(&out, "{\"name\":");
		    bu_cmd_json_string(&out, role->name);
		    bu_vls_strcat(&out, ",\"help\":");
		    bu_cmd_json_string(&out, role->help);
		    bu_vls_strcat(&out, ",\"type\":");
		    bu_cmd_json_string(&out, cmd_schema_value_name(role->value_type));
		    bu_vls_strcat(&out, ",\"shape\":");
		    cmd_schema_json_arg_shape(&out, role->shape);
		    bu_vls_strcat(&out, ",\"semantic_provider\":");
		    bu_cmd_json_string(&out, role->semantic_provider);
		    bu_vls_strcat(&out, ",");
		    cmd_schema_json_keyword_values(&out, role->value_keywords,
			role->keyword_values);
		    bu_vls_strcat(&out, ",\"range\":");
		    cmd_schema_json_range(&out, &role->range);
		    bu_vls_putc(&out, '}');
		}
	    }
	    bu_vls_strcat(&out, "]}");
	    comma = 1;
	}
    }
    bu_vls_strcat(&out, "],\"constraints\":[");
    comma = 0;
    if (schema->validation.constraints) {
	for (i = 0; schema->validation.constraints[i].options; i++) {
	    const struct bu_cmd_constraint *constraint = &schema->validation.constraints[i];
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"kind\":");
	    bu_cmd_json_string(&out, cmd_schema_constraint_kind_name(constraint->kind));
	    bu_vls_strcat(&out, ",\"condition\":");
	    bu_cmd_json_string(&out, cmd_schema_constraint_condition_name(constraint->condition));
	    bu_vls_strcat(&out, ",\"options\":[");
	    for (size_t oi = 0; constraint->options[oi]; oi++) {
		if (oi)
		    bu_vls_putc(&out, ',');
		bu_cmd_json_string(&out, constraint->options[oi]);
	    }
	    bu_vls_printf(&out, "],\"min\":%lu,\"max\":", (unsigned long)constraint->min_count);
	    if (constraint->max_count == BU_CMD_COUNT_UNLIMITED)
		bu_vls_strcat(&out, "null");
	    else
		bu_vls_printf(&out, "%lu", (unsigned long)constraint->max_count);
	    bu_vls_strcat(&out, ",\"hint\":");
	    bu_cmd_json_string(&out, constraint->hint);
	    bu_vls_putc(&out, '}');
	    comma = 1;
	}
    }
    bu_vls_strcat(&out, "],\"operand_cases\":[");
    comma = 0;
    if (schema->validation.cases) {
	for (i = 0; schema->validation.cases[i].name; i++) {
	    const struct bu_cmd_schema_case *cmd_case =
		&schema->validation.cases[i];
	    struct bu_cmd_schema view = *schema;
	    char *case_schema;

	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"name\":");
	    bu_cmd_json_string(&out, cmd_case->name);
	    bu_vls_strcat(&out, ",\"help\":");
	    bu_cmd_json_string(&out, cmd_case->help);
	    bu_vls_strcat(&out, ",\"condition\":");
	    bu_cmd_json_string(&out,
		cmd_schema_constraint_condition_name(cmd_case->condition));
	    bu_vls_strcat(&out, ",\"options\":[");
	    for (size_t oi = 0; cmd_case->options && cmd_case->options[oi]; oi++) {
		if (oi)
		    bu_vls_putc(&out, ',');
		bu_cmd_json_string(&out, cmd_case->options[oi]);
	    }
	    bu_vls_strcat(&out, "],\"schema\":");
	    view.operands = cmd_case->operands;
	    view.operand_groups = cmd_case->operand_groups;
	    view.validation.cases = NULL;
	    case_schema = bu_cmd_schema_describe_json(&view);
	    bu_vls_strcat(&out, case_schema ? case_schema : "null");
	    if (case_schema)
		bu_free(case_schema, "command case schema JSON");
	    bu_vls_putc(&out, '}');
	    comma = 1;
	}
    }
    bu_vls_strcat(&out, "]}");

    char *result = bu_strdup(bu_vls_addr(&out));
    bu_vls_free(&out);
    return result;
}


static void
cmd_schema_clear_completion_candidates(struct bu_cmd_validate_result *result)
{
    if (!result)
	return;
    if (result->completion_candidates) {
	for (size_t i = 0; i < result->completion_count; i++)
	    bu_free((void *)result->completion_candidates[i], "command schema completion candidate");
	bu_free((void *)result->completion_candidates, "command schema completion candidates");
    }
    result->completion_candidates = NULL;
    result->completion_count = 0;
}


void
bu_cmd_validate_result_init(struct bu_cmd_validate_result *result)
{
    if (!result)
	return;
    *result = (struct bu_cmd_validate_result)BU_CMD_VALIDATE_RESULT_NULL;
}


void
bu_cmd_validate_result_clear(struct bu_cmd_validate_result *result)
{
    if (!result)
	return;
    cmd_schema_clear_completion_candidates(result);
    bu_cmd_validate_result_init(result);
}


void
bu_cmd_keyword_candidates(struct bu_cmd_validate_result *result,
	const char * const *values, const char *prefix)
{
    size_t count = 0;
    size_t prefix_len;

    if (!result)
	return;
    cmd_schema_clear_completion_candidates(result);
    if (!values)
	return;
    prefix = prefix ? prefix : "";
    prefix_len = strlen(prefix);
    for (size_t i = 0; values[i]; i++)
	if (!prefix_len || bu_strncmp(values[i], prefix, prefix_len) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "command schema keyword candidates");
    for (size_t i = 0; values[i]; i++)
	if (!prefix_len || bu_strncmp(values[i], prefix, prefix_len) == 0)
	    result->completion_candidates[result->completion_count++] = bu_strdup(values[i]);
}


void
bu_cmd_validate_result_set(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, unsigned int expected,
	bu_cmd_value_t type, const char *hint, const char *semantic_provider)
{
    bu_cmd_validate_result_clear(result);
    result->state = state;
    result->token_start = token;
    result->token_end = token;
    result->expected = expected;
    result->completion_type = type;
    result->hint = hint;
    result->semantic_provider = semantic_provider;
}


static void
cmd_schema_set_result(struct bu_cmd_validate_result *result,
	bu_cmd_validate_state_t state, size_t token, unsigned int expected,
	bu_cmd_value_t type, const char *hint, const char *semantic_provider)
{
    bu_cmd_validate_result_set(result, state, token, expected, type, hint,
	semantic_provider);
}


static int
cmd_schema_option_arguments_valid(const struct bu_cmd_option *option,
	size_t argc, const char **argv)
{
    if (!option)
	return 0;
    if (option->consume)
	return option->consume(NULL, argc, argv, NULL) == 0;
    for (size_t i = 0; i < argc; i++) {
	if (!cmd_schema_value_valid(option, argv[i]))
	    return 0;
    }
    return 1;
}


static int
cmd_schema_prefix_match(const char *candidate, const char *prefix)
{
    return !prefix || !prefix[0] || bu_strncmp(candidate, prefix, strlen(prefix)) == 0;
}


static void
cmd_schema_add_option_candidates(const struct bu_cmd_schema *schema,
	struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;
    size_t i = 0;

    if (!schema || !schema->options)
	return;
    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (!option->hidden && !option->alias_of) {
	    if (option->shortopt && strlen(option->shortopt) == 1) {
		char spelling[3] = {'-', option->shortopt[0], '\0'};
		if (cmd_schema_prefix_match(spelling, prefix)) count++;
	    }
	    if (option->longopt && option->longopt[0]) {
		struct bu_vls spelling = BU_VLS_INIT_ZERO;
		bu_vls_printf(&spelling, "--%s", option->longopt);
		if (cmd_schema_prefix_match(bu_vls_addr(&spelling), prefix)) count++;
		bu_vls_free(&spelling);
	    }
	}
	i++;
    }
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1, sizeof(char *), "command schema completion candidates");
    result->completion_count = 0;
    i = 0;
    while (bu_cmd_option_is_valid(&schema->options[i])) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (!option->hidden && !option->alias_of) {
	    if (option->shortopt && strlen(option->shortopt) == 1) {
		char spelling[3] = {'-', option->shortopt[0], '\0'};
		if (cmd_schema_prefix_match(spelling, prefix))
		    result->completion_candidates[result->completion_count++] = bu_strdup(spelling);
	    }
	    if (option->longopt && option->longopt[0]) {
		struct bu_vls spelling = BU_VLS_INIT_ZERO;
		bu_vls_printf(&spelling, "--%s", option->longopt);
		if (cmd_schema_prefix_match(bu_vls_addr(&spelling), prefix))
		    result->completion_candidates[result->completion_count++] = bu_strdup(bu_vls_addr(&spelling));
		bu_vls_free(&spelling);
	    }
	}
	i++;
    }
}


static void
cmd_schema_append_completion_candidate(struct bu_cmd_validate_result *result,
	const char *candidate)
{
    if (!result || !candidate || !candidate[0])
	return;
    for (size_t i = 0; i < result->completion_count; i++)
	if (BU_STR_EQUAL(result->completion_candidates[i], candidate))
	    return;
    result->completion_candidates = (const char **)bu_realloc(
	(void *)result->completion_candidates,
	(result->completion_count + 2) * sizeof(char *),
	"command schema completion candidates");
    result->completion_candidates[result->completion_count++] =
	bu_strdup(candidate);
    result->completion_candidates[result->completion_count] = NULL;
}


static void
cmd_schema_add_named_option_candidates(const struct bu_cmd_schema *schema,
	struct bu_cmd_validate_result *result, const char * const *canonical_names)
{
    if (!schema || !result || !canonical_names)
	return;
    for (size_t i = 0; canonical_names[i]; i++) {
	const struct bu_cmd_option *option = cmd_schema_find_canonical(schema,
	    canonical_names[i]);
	if (!option || option->hidden)
	    continue;
	if (option->shortopt && strlen(option->shortopt) == 1) {
	    char spelling[3] = {'-', option->shortopt[0], '\0'};
	    cmd_schema_append_completion_candidate(result, spelling);
	}
	if (option->longopt && option->longopt[0]) {
	    struct bu_vls spelling = BU_VLS_INIT_ZERO;
	    bu_vls_printf(&spelling, "--%s", option->longopt);
	    cmd_schema_append_completion_candidate(result, bu_vls_addr(&spelling));
	    bu_vls_free(&spelling);
	}
    }
}


static void
cmd_schema_add_keyword_candidates(const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values,
	const struct bu_cmd_arg_shape *shape,
	struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;
    size_t i = 0;
    const char *element_prefix = prefix ? prefix : "";
    const char *comma = NULL;
    size_t base_len = 0;
    int negated = 0;

    if ((!keywords && !keyword_values) || !result)
	return;

    /* A comma-list is one syntactic argument, but its individual list
     * elements have their own completion seed.  Preserve the preceding
     * elements and the stat-style per-element reverse marker while replacing
     * only the active element. */
    if (shape && shape->kind == BU_CMD_ARG_SHAPE_COMMA_LIST) {
	comma = strrchr(element_prefix, ',');
	if (comma) {
	    base_len = (size_t)(comma - element_prefix + 1);
	    element_prefix = comma + 1;
	}
	if (element_prefix[0] == '!') {
	    negated = 1;
	    element_prefix++;
	}
    }

    if (keyword_values) {
	while (keyword_values[i].canonical) {
	    int match = cmd_schema_prefix_match(keyword_values[i].canonical, element_prefix);
	    if (!match && keyword_values[i].aliases) {
		for (size_t ai = 0; keyword_values[i].aliases[ai]; ai++) {
		    if (cmd_schema_prefix_match(keyword_values[i].aliases[ai], element_prefix)) {
			match = 1;
			break;
		    }
		}
	    }
	    if (match)
		count++;
	    i++;
	}
    } else {
	while (keywords[i]) {
	    if (cmd_schema_prefix_match(keywords[i], element_prefix))
		count++;
	    i++;
	}
    }
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1, sizeof(char *),
	"command schema keyword candidates");
    result->completion_count = 0;
    if (keyword_values) {
	for (i = 0; keyword_values[i].canonical; i++) {
	    int match = cmd_schema_prefix_match(keyword_values[i].canonical, element_prefix);
	    if (!match && keyword_values[i].aliases) {
		for (size_t ai = 0; keyword_values[i].aliases[ai]; ai++) {
		    if (cmd_schema_prefix_match(keyword_values[i].aliases[ai], element_prefix)) {
			match = 1;
			break;
		    }
		}
	    }
	    if (match) {
		struct bu_vls candidate = BU_VLS_INIT_ZERO;
		if (base_len)
		    bu_vls_strncpy(&candidate, prefix, base_len);
		if (negated)
		    bu_vls_putc(&candidate, '!');
		bu_vls_strcat(&candidate, keyword_values[i].canonical);
		result->completion_candidates[result->completion_count++] =
		    bu_vls_strdup(&candidate);
		bu_vls_free(&candidate);
	    }
	}
    } else {
	for (i = 0; keywords[i]; i++) {
	    if (cmd_schema_prefix_match(keywords[i], element_prefix)) {
		struct bu_vls candidate = BU_VLS_INIT_ZERO;
		if (base_len)
		    bu_vls_strncpy(&candidate, prefix, base_len);
		if (negated)
		    bu_vls_putc(&candidate, '!');
		bu_vls_strcat(&candidate, keywords[i]);
		result->completion_candidates[result->completion_count++] = bu_vls_strdup(&candidate);
		bu_vls_free(&candidate);
	    }
	}
    }
}


const struct bu_cmd_operand *
bu_cmd_schema_operand(const struct bu_cmd_schema *schema, size_t operand_index)
{
    size_t index = 0;

    if (!schema)
	return NULL;
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    if (operand->max_count == BU_CMD_COUNT_UNLIMITED ||
		(operand_index >= index && operand_index - index < operand->max_count))
		return operand;
	    index += operand->max_count;
	}
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    size_t width = 0;
	    size_t offset;
	    size_t capacity;

	    if (!group->roles)
		continue;
	    while (group->roles[width].name)
		width++;
	    if (!width)
		continue;
	    offset = operand_index >= index ? operand_index - index : 0;
	    capacity = group->max_count == BU_CMD_COUNT_UNLIMITED ?
		BU_CMD_COUNT_UNLIMITED : group->max_count * width;
	    if (operand_index >= index &&
		(capacity == BU_CMD_COUNT_UNLIMITED || offset < capacity))
		return &group->roles[offset % width];
	    index += capacity;
	}
    }
    return NULL;
}


static size_t
cmd_schema_minimum_operands(const struct bu_cmd_schema *schema)
{
    size_t minimum = 0;

    if (!schema)
	return 0;
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++)
	    minimum += schema->operands[i].min_count;
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    size_t width = 0;
	    if (schema->operand_groups[gi].roles)
		while (schema->operand_groups[gi].roles[width].name)
		    width++;
	    minimum += schema->operand_groups[gi].min_count * width;
	}
    }
    return minimum;
}


static int
cmd_schema_operand_sequence_complete(const struct bu_cmd_schema *schema,
	size_t operand_count)
{
    size_t fixed = 0;

    if (!schema)
	return 0;
    if (!schema->operand_groups)
	return operand_count >= cmd_schema_minimum_operands(schema);
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &schema->operands[i];
	    if (operand_count < fixed + operand->min_count)
		return 0;
	    if (operand->max_count == BU_CMD_COUNT_UNLIMITED)
		return 1;
	    fixed += operand->max_count;
	}
    }
    if (operand_count < fixed)
	return operand_count >= cmd_schema_minimum_operands(schema);
    if (schema->operand_groups) {
	size_t remaining = operand_count - fixed;
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    size_t width = 0;
	    size_t capacity;
	    size_t consumed;

	    if (group->roles)
		while (group->roles[width].name)
		    width++;
	    if (!width)
		return 0;
	    capacity = group->max_count == BU_CMD_COUNT_UNLIMITED ?
		BU_CMD_COUNT_UNLIMITED : group->max_count * width;
	    consumed = capacity == BU_CMD_COUNT_UNLIMITED || remaining < capacity ?
		remaining : capacity;
	    if (consumed < group->min_count * width || consumed % width)
		return 0;
	    remaining -= consumed;
	}
	return remaining == 0;
    }
    return operand_count >= cmd_schema_minimum_operands(schema);
}


static size_t
cmd_schema_option_occurrences(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char *canonical)
{
    size_t i = 0;
    size_t operand_count = 0;
    size_t occurrences = 0;
    int options_allowed = 1;

    if (!schema || !canonical)
	return 0;
    while (i < argc) {
	const char *arg = argv[i];
	const struct bu_cmd_option *option = NULL;
	const char *eq = NULL;
	const char *short_attached = NULL;
	if (!arg)
	    return occurrences;
	if (cmd_schema_is_end_marker(schema, arg, options_allowed, operand_count)) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && cmd_schema_has_options(schema) &&
	    arg[0] == '-' && arg[1]) {
	    option = cmd_schema_lookup_token(schema, arg);
	    if (!option)
		option = cmd_schema_attached_short_option(schema, arg,
		    &short_attached);
	    eq = strchr(arg, '=');
	    if (option) {
		if (BU_STR_EQUAL(bu_cmd_option_canonical(option), canonical))
		    occurrences++;
		if (option->arg_requirement == BU_CMD_ARG_NONE || eq || short_attached) {
		    i++;
		    continue;
		}
		{
		    size_t consume = cmd_schema_option_argument_count(option, argc - i - 1,
			argv + i + 1);
		    i += consume + 1;
		    continue;
		}
	    }
	    if (cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0) > 0) {
		for (size_t ci = 1; arg[ci]; ci++) {
		    char short_name[2] = {arg[ci], '\0'};
		    option = cmd_schema_find_option(schema, short_name, 0);
		    if (option && BU_STR_EQUAL(bu_cmd_option_canonical(option), canonical))
			occurrences++;
		}
		i++;
		continue;
	    }
	}
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
	operand_count++;
	i++;
    }

    return occurrences;
}


int
bu_cmd_schema_option_present(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char *canonical)
{
    return cmd_schema_option_occurrences(schema, argc, argv, canonical) > 0;
}


size_t
bu_cmd_schema_operand_count(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv)
{
    size_t i = 0;
    size_t count = 0;
    int options_allowed = 1;

    if (!schema)
	return 0;
    while (i < argc) {
	const char *arg = argv[i];
	const struct bu_cmd_option *option = NULL;
	const char *eq = NULL;
	const char *short_attached = NULL;
	if (!arg)
	    return count;
	if (cmd_schema_is_end_marker(schema, arg, options_allowed, count)) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && cmd_schema_has_options(schema) &&
	    arg[0] == '-' && arg[1]) {
	    option = cmd_schema_lookup_token(schema, arg);
	    if (!option)
		option = cmd_schema_attached_short_option(schema, arg,
		    &short_attached);
	    eq = strchr(arg, '=');
	    if (option) {
		if (option->arg_requirement == BU_CMD_ARG_NONE || eq || short_attached) {
		    i++;
		    continue;
		}
		{
		    size_t consume = cmd_schema_option_argument_count(option, argc - i - 1,
			argv + i + 1);
		    i += consume + 1;
		    continue;
		}
	    }
	    if (cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0) > 0) {
		i++;
		continue;
	    }
	}
	count++;
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
	i++;
    }

    return count;
}


/* Collect positional words and their original argv indexes using the same
 * option scanner as bu_cmd_schema_operand_count.  This lets shape validation
 * operate on a contiguous positional view without losing editor token spans. */
static size_t
cmd_schema_collect_operands(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char ***values_out, size_t **indexes_out,
	int *options_allowed_out)
{
    const char **values = NULL;
    size_t *indexes = NULL;
    size_t count = 0;
    size_t i = 0;
    int options_allowed = 1;

    if (values_out)
	*values_out = NULL;
    if (indexes_out)
	*indexes_out = NULL;
    if (options_allowed_out)
	*options_allowed_out = 0;
    if (!schema || (argc && !argv))
	return 0;
    if (argc && values_out) {
	values = (const char **)bu_calloc(argc, sizeof(char *),
	    "command schema positional values");
    }
    if (argc && indexes_out) {
	indexes = (size_t *)bu_calloc(argc, sizeof(size_t),
	    "command schema positional indexes");
    }
    while (i < argc) {
	const char *arg = argv[i];
	const struct bu_cmd_option *option = NULL;
	const char *short_attached = NULL;
	const char *eq = NULL;

	if (!arg)
	    break;
	if (cmd_schema_is_end_marker(schema, arg, options_allowed, count)) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && cmd_schema_has_options(schema) &&
	    arg[0] == '-' && arg[1]) {
	    option = cmd_schema_lookup_token(schema, arg);
	    if (!option)
		option = cmd_schema_attached_short_option(schema, arg,
		    &short_attached);
	    eq = strchr(arg, '=');
	    if (option) {
		if (option->arg_requirement == BU_CMD_ARG_NONE || eq || short_attached) {
		    i++;
		    continue;
		}
		i += cmd_schema_option_argument_count(option, argc - i - 1,
		    argv + i + 1) + 1;
		continue;
	    }
	    if (cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0) > 0) {
		i++;
		continue;
	    }
	}
	if (values)
	    values[count] = arg;
	if (indexes)
	    indexes[count] = i;
	count++;
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
	i++;
    }
    if (values_out)
	*values_out = values;
    if (indexes_out)
	*indexes_out = indexes;
    if (options_allowed_out)
	*options_allowed_out = options_allowed;
    return count;
}


int
bu_cmd_schema_option_span(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv)
{
    const char *arg = NULL;
    const char *name = NULL;
    const char *eq = NULL;
    const char *short_attached = NULL;
    const struct bu_cmd_option *option = NULL;
    int longopt = 0;

    if (!schema || !argc || !argv || !argv[0])
	return 0;
    arg = argv[0];
    if (BU_STR_EQUAL(arg, "--"))
	return 1;
    if (arg[0] != '-' || !arg[1])
	return 0;

    longopt = arg[1] == '-';
    name = arg + (longopt ? 2 : 1);
    eq = strchr(name, '=');
    if (eq) {
	size_t name_len = (size_t)(eq - name);
	char *name_copy = (char *)bu_malloc(name_len + 1, "command schema option name");
	memcpy(name_copy, name, name_len);
	name_copy[name_len] = '\0';
	option = cmd_schema_find_option(schema, name_copy, longopt);
	bu_free(name_copy, "command schema option name");
    } else {
	option = cmd_schema_find_option(schema, name, longopt);
    }
    if (!option && !longopt && !eq)
	option = cmd_schema_attached_short_option(schema, arg, &short_attached);
    if (!option) {
	int cluster = cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0);
	return cluster > 0 ? 1 : 0;
    }
    if (option->arg_requirement == BU_CMD_ARG_NONE)
	return eq ? -1 : 1;
    if (short_attached) {
	if (!short_attached[0] ||
	    !cmd_schema_option_arguments_valid(option, 1, &short_attached))
	    return -1;
	return 1;
    }
    if (eq) {
	const char *value = eq + 1;
	if (!cmd_schema_option_attached_allowed(option) ||
	    !cmd_schema_option_arguments_valid(option, 1, &value))
	    return -1;
	return 1;
    }

    size_t available = argc - 1;
    size_t consume = cmd_schema_option_argument_count(option, available, argv + 1);
    if (consume < cmd_schema_option_min_tokens(option) ||
	!cmd_schema_option_arguments_valid(option, consume, argv + 1))
	return -1;
    return (int)(consume + 1);
}


static size_t
cmd_schema_selected_option_count(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char * const *options)
{
    size_t count = 0;

    if (!options)
	return 0;
    for (size_t i = 0; options[i]; i++) {
	if (bu_cmd_schema_option_present(schema, argc, argv, options[i]))
	    count++;
    }
    return count;
}


static size_t
cmd_schema_selected_option_occurrences(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, const char * const *options)
{
    size_t count = 0;

    if (!options)
	return 0;
    for (size_t i = 0; options[i]; i++)
	count += cmd_schema_option_occurrences(schema, argc, argv, options[i]);
    return count;
}


static int
cmd_schema_condition_applies(bu_cmd_constraint_condition_t condition,
	size_t selected_count, size_t option_count)
{
    switch (condition) {
	case BU_CMD_CONDITION_ALWAYS:
	    return 1;
	case BU_CMD_CONDITION_ANY_OPTION_PRESENT:
	    return selected_count > 0;
	case BU_CMD_CONDITION_NO_OPTION_PRESENT:
	    return selected_count == 0;
	case BU_CMD_CONDITION_ALL_OPTIONS_PRESENT:
	    return option_count > 0 && selected_count == option_count;
	default:
	    break;
    }
    return 0;
}


static int
cmd_schema_constraint_applies(const struct bu_cmd_constraint *constraint,
	size_t selected_count, size_t option_count)
{
    return constraint ? cmd_schema_condition_applies(constraint->condition,
	selected_count, option_count) : 0;
}


static const struct bu_cmd_schema_case *
cmd_schema_select_case(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv)
{
    if (!schema || !schema->validation.cases)
	return NULL;

    for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	const struct bu_cmd_schema_case *cmd_case = &schema->validation.cases[ci];
	size_t option_count = 0;
	size_t selected_count = 0;

	if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS)
	    return cmd_case;
	if (!cmd_case->options)
	    continue;
	while (cmd_case->options[option_count])
	    option_count++;
	selected_count = cmd_schema_selected_option_count(schema, argc, argv,
	    cmd_case->options);
	if (cmd_schema_condition_applies(cmd_case->condition, selected_count,
		option_count))
	    return cmd_case;
    }
    return NULL;
}


static const struct bu_cmd_schema *
cmd_schema_case_view(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, struct bu_cmd_schema *view)
{
    const struct bu_cmd_schema_case *cmd_case;

    if (!schema || !view)
	return schema;
    cmd_case = cmd_schema_select_case(schema, argc, argv);
    if (!cmd_case)
	return schema;
    *view = *schema;
    view->operands = cmd_case->operands;
    view->operand_groups = cmd_case->operand_groups;
    view->validation.cases = NULL;
    return view;
}


const struct bu_cmd_operand *
bu_cmd_schema_active_operand(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t operand_index)
{
    struct bu_cmd_schema view;
    const struct bu_cmd_schema *active = cmd_schema_case_view(schema, argc,
	argv, &view);

    return bu_cmd_schema_operand(active, operand_index);
}


static int
cmd_schema_apply_constraints(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, struct bu_cmd_validate_result *result)
{
    int prior_incomplete;

    if (!schema || !schema->validation.constraints || !result)
	return 0;
    /* A definite conflict or excess remains invalid even when a local value
     * is unfinished.  Missing higher-level requirements, however, must not
     * replace the more immediate option-argument or operand-group prompt. */
    prior_incomplete = result->state == BU_CMD_VALIDATE_INCOMPLETE;

    for (size_t ci = 0; schema->validation.constraints[ci].options; ci++) {
	const struct bu_cmd_constraint *constraint = &schema->validation.constraints[ci];
	size_t option_count = 0;
	size_t selected_count = 0;
	size_t actual_count = 0;
	const struct bu_cmd_operand *operand = NULL;
	bu_cmd_validate_state_t state = BU_CMD_VALIDATE_INVALID;
	unsigned int expected = BU_CMD_EXPECT_NONE;
	bu_cmd_value_t value_type = BU_CMD_VALUE_STRING;
	const char *provider = NULL;

	if (constraint->min_count > constraint->max_count)
	    continue;
	if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES ||
	    constraint->kind == BU_CMD_CONSTRAINT_OPTION_CONFLICTS) {
	    int violated = 0;
	    int options_allowed = 0;
	    const char **missing = NULL;
	    size_t missing_count = 0;
	    size_t missing_capacity = 0;

	    if (constraint->condition != BU_CMD_CONDITION_ALWAYS ||
		constraint->min_count || constraint->max_count ||
		!constraint->options[0] || !constraint->options[1] ||
		!bu_cmd_schema_option_present(schema, argc, argv,
		    constraint->options[0]))
		continue;
	    if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES) {
		for (size_t oi = 1; constraint->options[oi]; oi++)
		    if (!bu_cmd_schema_option_present(schema, argc, argv,
			    constraint->options[oi]))
			missing_capacity++;
		if (missing_capacity)
		    missing = (const char **)bu_calloc(missing_capacity + 1,
			sizeof(char *), "missing required command options");
	    }
	    for (size_t oi = 1; constraint->options[oi]; oi++) {
		int present = bu_cmd_schema_option_present(schema, argc, argv,
		    constraint->options[oi]);
		if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES && !present)
		    missing[missing_count++] = constraint->options[oi];
		if ((constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES && !present) ||
		    (constraint->kind == BU_CMD_CONSTRAINT_OPTION_CONFLICTS && present)) {
		    violated = 1;
		    if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_CONFLICTS)
			break;
		}
	    }
	    if (!violated) {
		if (missing)
		    bu_free((void *)missing, "missing required command options");
		continue;
	    }
	    if (prior_incomplete &&
		constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES) {
		if (missing)
		    bu_free((void *)missing, "missing required command options");
		continue;
	    }
	    if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES) {
		(void)cmd_schema_collect_operands(schema, argc, argv, NULL, NULL,
		    &options_allowed);
	    }
	    cmd_schema_set_result(result,
		constraint->kind == BU_CMD_CONSTRAINT_OPTION_REQUIRES ?
		(options_allowed ? BU_CMD_VALIDATE_INCOMPLETE :
		 BU_CMD_VALIDATE_INVALID) : BU_CMD_VALIDATE_INVALID,
		argc, BU_CMD_EXPECT_OPTION, BU_CMD_VALUE_FLAG,
		constraint->hint ? constraint->hint : "option relationship",
		NULL);
	    if (options_allowed && missing)
		cmd_schema_add_named_option_candidates(schema, result, missing);
	    if (missing)
		bu_free((void *)missing, "missing required command options");
	    return 0;
	}
	while (constraint->options[option_count])
	    option_count++;
	selected_count = cmd_schema_selected_option_count(schema, argc, argv,
	    constraint->options);
	if (!cmd_schema_constraint_applies(constraint, selected_count, option_count))
	    continue;

	if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT)
	    actual_count = selected_count;
	else if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT)
	    actual_count = cmd_schema_selected_option_occurrences(schema, argc, argv,
		constraint->options);
	else
	    actual_count = bu_cmd_schema_operand_count(schema, argc, argv);
	if (actual_count >= constraint->min_count && actual_count <= constraint->max_count)
	    continue;
	if (prior_incomplete && actual_count < constraint->min_count)
	    continue;

	state = actual_count < constraint->min_count ?
	    BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT ||
	    constraint->kind == BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT) {
	    expected = BU_CMD_EXPECT_OPTION;
	    value_type = BU_CMD_VALUE_FLAG;
	} else {
	    expected = BU_CMD_EXPECT_OPERAND;
	    operand = bu_cmd_schema_operand(schema, actual_count);
	    if (operand) {
		value_type = operand->value_type;
		provider = operand->semantic_provider;
	    }
	}
	cmd_schema_set_result(result, state, argc, expected, value_type,
	    constraint->hint ? constraint->hint : "command constraint", provider);
	if ((constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT ||
	    constraint->kind == BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT) &&
	    state == BU_CMD_VALIDATE_INCOMPLETE) {
	    int options_allowed = 0;
	    const char **missing = NULL;
	    size_t missing_count = 0;
	    (void)cmd_schema_collect_operands(schema, argc, argv, NULL, NULL,
		&options_allowed);
	    if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT) {
		missing = (const char **)bu_calloc(option_count + 1,
		    sizeof(char *), "missing command options");
		for (size_t oi = 0; constraint->options[oi]; oi++)
		    if (!bu_cmd_schema_option_present(schema, argc, argv,
			    constraint->options[oi]))
			missing[missing_count++] = constraint->options[oi];
	    }
	    if (options_allowed)
		cmd_schema_add_named_option_candidates(schema, result,
		    constraint->kind == BU_CMD_CONSTRAINT_OPTION_OCCURRENCE_COUNT ?
		    constraint->options : missing);
	    if (missing)
		bu_free((void *)missing, "missing command options");
	}
	return 0;
    }

    return 0;
}


static int
cmd_schema_terminal_option_present(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv)
{
    if (!schema)
	return 0;
    if ((schema->validation.terminal_flags & BU_CMD_TERMINAL_HELP) &&
	bu_cmd_schema_option_present(schema, argc, argv, "help"))
	return 1;
    if ((schema->validation.terminal_flags & BU_CMD_TERMINAL_VERSION) &&
	bu_cmd_schema_option_present(schema, argc, argv, "version"))
	return 1;
    for (size_t i = 0; schema->validation.terminal_options &&
	schema->validation.terminal_options[i]; i++) {
	if (bu_cmd_schema_option_present(schema, argc, argv,
		schema->validation.terminal_options[i]))
	    return 1;
    }
    return 0;
}


int
bu_cmd_operand_validate(const struct bu_cmd_operand *operand, const char *arg)
{
    struct bu_cmd_option option = BU_CMD_OPTION_NULL;

    if (!operand)
	return 0;
    option.value_type = operand->value_type;
    option.validate = operand->validate;
    option.value_keywords = operand->value_keywords;
    option.keyword_values = operand->keyword_values;
    option.range = operand->range;
    return cmd_schema_value_valid(&option, arg);
}


int
_bu_cmd_schema_validate_structure(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result,
	const struct bu_cmd_option **active_option)
{
    size_t i = 0;
    size_t operand_count = 0;
    int options_allowed = 1;

    if (!schema || !result || cursor_arg > argc)
	return -1;

    if (active_option)
	*active_option = NULL;

    bu_cmd_validate_result_clear(result);
    while (i < cursor_arg) {
	const char *arg = argv ? argv[i] : NULL;
	if (!arg) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_NONE,
		BU_CMD_VALUE_STRING, "null argument", NULL);
	    return 0;
	}
	/* The end marker is structural even for an optionless schema: callers use
	 * it to make an otherwise ambiguous leading-dash operand explicit. */
	if (cmd_schema_is_end_marker(schema, arg, options_allowed, operand_count)) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && cmd_schema_has_options(schema) &&
	    arg[0] == '-' && arg[1]) {
	    const struct bu_cmd_option *option = cmd_schema_lookup_token(schema, arg);
	const char *eq = strchr(arg, '=');
	const char *short_attached = NULL;
	size_t min_tokens = 0;
	size_t available = 0;
	    size_t consume = 0;
	    if (!option)
		option = cmd_schema_attached_short_option(schema, arg,
		    &short_attached);
	    if (!option && cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0) > 0) {
		i++;
		continue;
	    }
	    if (!option) {
		if (cmd_schema_dash_numeric_operand_valid(schema, operand_count, arg))
		    goto validate_operand;
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_OPTION,
		    BU_CMD_VALUE_STRING, "unknown option", NULL);
		cmd_schema_add_option_candidates(schema, result, arg);
		return 0;
	    }
	    if (option->arg_requirement == BU_CMD_ARG_NONE && eq) {
		if (active_option)
		    *active_option = option;
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_OPTION,
		    BU_CMD_VALUE_FLAG, "option does not take an argument", option->semantic_provider);
		return 0;
	    }
	    if (option->arg_requirement == BU_CMD_ARG_NONE) {
		i++;
		continue;
	    }
	    min_tokens = cmd_schema_option_min_tokens(option);
	    if (short_attached) {
		if (!short_attached[0] ||
		    !cmd_schema_option_arguments_valid(option, 1,
			&short_attached)) {
		    if (active_option)
			*active_option = option;
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type,
			"invalid option argument", option->semantic_provider);
		    return 0;
		}
		i++;
		continue;
	    }
	    if (eq) {
		const char *value = eq + 1;
		if (!cmd_schema_option_attached_allowed(option) ||
		    !cmd_schema_option_arguments_valid(option, 1, &value)) {
		    if (active_option)
			*active_option = option;
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_OPTION_ARG,
			option->value_type, "invalid option argument", option->semantic_provider);
		    return 0;
		}
		i++;
		continue;
	    }

	    available = argc - i - 1;
	    consume = cmd_schema_option_argument_count(option, available, argv + i + 1);

	    /* A current token selected as part of this option's shape is an
	     * option argument, not an operand.  This is what makes an optional
	     * argument such as -s [num] retain its greedy execution meaning. */
	    if (cursor_arg >= i + 1 && cursor_arg < i + 1 + consume) {
		int valid = cmd_schema_option_arguments_valid(option, consume, argv + i + 1);
		if (active_option)
		    *active_option = option;
		cmd_schema_set_result(result, valid ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID,
		    cursor_arg, BU_CMD_EXPECT_OPTION_ARG, option->value_type,
		    valid ? "option argument" : "invalid option argument", option->semantic_provider);
		result->candidate_validate = option->validate;
		cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		    cmd_schema_option_arg_shape(option), result, argv[cursor_arg]);
		return 0;
	    }

	    if (consume < min_tokens) {
		if (cmd_schema_option_arg_shape(option) && cmd_schema_option_arg_shape(option)->kind == BU_CMD_ARG_SHAPE_RGB &&
		    available && !cmd_schema_rgb_partial(option, available, argv + i + 1)) {
		    if (active_option)
			*active_option = option;
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid RGB color",
			option->semantic_provider);
		    return 0;
		}
		if (cmd_schema_option_arg_shape(option) && cmd_schema_option_arg_shape(option)->kind == BU_CMD_ARG_SHAPE_COLOR &&
		    available && !cmd_schema_color_partial(option, available, argv + i + 1)) {
		    if (active_option)
			*active_option = option;
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid color",
			option->semantic_provider);
		    return 0;
		}
		if (cmd_schema_option_arg_shape(option) && cmd_schema_option_arg_shape(option)->kind == BU_CMD_ARG_SHAPE_VECTOR3 &&
		    available && !cmd_schema_vector3_partial(option, available, argv + i + 1)) {
		    if (active_option)
			*active_option = option;
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid XYZ vector",
			option->semantic_provider);
		    return 0;
		}
		if (!option->consume) {
		    for (size_t ai = 0; ai < consume; ai++) {
			if (!cmd_schema_value_valid(option, argv[i + 1 + ai])) {
			    if (active_option)
				*active_option = option;
			    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1 + ai,
				BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid option argument",
				option->semantic_provider);
			    return 0;
			}
		    }
		}
		if (active_option)
		    *active_option = option;
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
		    BU_CMD_EXPECT_OPTION_ARG, option->value_type, "option argument expected",
		    option->semantic_provider);
		result->candidate_validate = option->validate;
		cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		    cmd_schema_option_arg_shape(option), result, "");
		return 0;
	    }
	    if (!cmd_schema_option_arguments_valid(option, consume, argv + i + 1)) {
		if (active_option)
		    *active_option = option;
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
		    BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid option argument",
		    option->semantic_provider);
		return 0;
	    }
	    i += consume + 1;
	    continue;
	}
validate_operand:
	;
	const struct bu_cmd_operand *operand = bu_cmd_schema_operand(schema, operand_count);
	if (!operand) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_NONE,
		BU_CMD_VALUE_STRING, "too many operands", NULL);
	    return 0;
	}
	if (!bu_cmd_operand_validate(operand, arg)) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_OPERAND,
		operand->value_type, "invalid operand", operand->semantic_provider);
	    return 0;
	}
	operand_count++;
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
	i++;
    }

    if (cursor_arg < argc && argv && argv[cursor_arg] &&
	cmd_schema_is_end_marker(schema, argv[cursor_arg], options_allowed,
	    operand_count) && !cmd_schema_has_long_options(schema)) {
	const struct bu_cmd_operand *operand = bu_cmd_schema_operand(schema, operand_count);
	bu_cmd_validate_state_t state = !cmd_schema_operand_sequence_complete(schema, operand_count) ?
	    BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_VALID;
	cmd_schema_set_result(result, state, cursor_arg,
	    operand ? BU_CMD_EXPECT_OPERAND : BU_CMD_EXPECT_NONE,
	    operand ? operand->value_type : BU_CMD_VALUE_STRING,
	    "end of options", operand ? operand->semantic_provider : NULL);
	cmd_schema_add_keyword_candidates(operand ? operand->value_keywords : NULL,
	    operand ? operand->keyword_values : NULL, operand ? operand->shape : NULL, result, "");
	return 0;
    }

    if (cursor_arg < argc && argv && argv[cursor_arg] && options_allowed &&
	cmd_schema_has_options(schema) &&
	argv[cursor_arg][0] == '-' && !cmd_schema_dash_numeric_operand_valid(schema,
	    operand_count, argv[cursor_arg])) {
	const char *current = argv[cursor_arg];
	int option_prefix = !current[1] || BU_STR_EQUAL(current, "--");
	const char *eq = strchr(current, '=');
	const struct bu_cmd_option *option = cmd_schema_lookup_token(schema, current);
	const char *short_attached = NULL;
	if (!option)
	    option = cmd_schema_attached_short_option(schema, current,
		&short_attached);
	int cluster = !option && cmd_schema_short_flag_cluster(schema, current, NULL, NULL, 0) > 0;
	const char *value = eq ? eq + 1 : short_attached;
	int value_valid = option && option->arg_requirement != BU_CMD_ARG_NONE && value ?
	    cmd_schema_option_attached_allowed(option) && cmd_schema_option_arguments_valid(option, 1, &value) : 1;
	int flag_has_value = option && option->arg_requirement == BU_CMD_ARG_NONE && eq;
	/* A lone '-', or '--' when long options are available, is an editable
	 * option prefix.  A completed command may still use '-' as an ordinary
	 * operand, but while the cursor is on that token the editor needs the
	 * option candidates rather than an immediate operand classification. */
	cmd_schema_set_result(result, option_prefix ? BU_CMD_VALIDATE_INCOMPLETE :
	    ((option && value_valid && !flag_has_value) || cluster ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID),
	    cursor_arg, BU_CMD_EXPECT_OPTION, BU_CMD_VALUE_STRING,
	    option_prefix ? "option expected" :
	    (cluster ? "short option flags" : (option ? (flag_has_value ? "option does not take an argument" : (value_valid ? "option" : "invalid option argument")) : "unknown option")),
	    option ? option->semantic_provider : NULL);
	cmd_schema_add_option_candidates(schema, result, current);
	if (option && option->arg_requirement != BU_CMD_ARG_NONE && value) {
	    cmd_schema_set_result(result, value_valid ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID,
		cursor_arg, BU_CMD_EXPECT_OPTION_ARG, option->value_type,
		value_valid ? "option argument" : "invalid option argument",
		option->semantic_provider);
	    result->candidate_validate = option->validate;
	    cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		cmd_schema_option_arg_shape(option), result, value);
	} else if (option && option->arg_requirement != BU_CMD_ARG_NONE && !value) {
	    bu_cmd_validate_state_t state = option->arg_requirement == BU_CMD_ARG_OPTIONAL ?
		BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE;
	    cmd_schema_set_result(result, state, cursor_arg, BU_CMD_EXPECT_OPTION_ARG,
		option->value_type, option->arg_requirement == BU_CMD_ARG_OPTIONAL ?
		"optional option argument" : "option argument expected", option->semantic_provider);
	    result->candidate_validate = option->validate;
	    cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		cmd_schema_option_arg_shape(option), result, "");
	}
	if (active_option)
	    *active_option = option;
	return 0;
    }

    const struct bu_cmd_operand *operand = bu_cmd_schema_operand(schema, operand_count);
    if (cursor_arg < argc && argv && argv[cursor_arg]) {
	if (!operand) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_EXPECT_NONE,
		BU_CMD_VALUE_STRING, "too many operands", NULL);
	} else if (!bu_cmd_operand_validate(operand, argv[cursor_arg])) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_EXPECT_OPERAND,
		operand->value_type, "invalid operand", operand->semantic_provider);
	    cmd_schema_add_keyword_candidates(operand->value_keywords, operand->keyword_values,
		operand->shape, result, argv[cursor_arg]);
	} else {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_VALID, cursor_arg, BU_CMD_EXPECT_OPERAND,
		operand->value_type, operand->name, operand->semantic_provider);
	    cmd_schema_add_keyword_candidates(operand->value_keywords, operand->keyword_values,
		operand->shape, result, argv[cursor_arg]);
	}
	if (operand)
	    result->candidate_validate = operand->validate;
	return 0;
    }

    bu_cmd_validate_state_t state = !cmd_schema_operand_sequence_complete(schema, operand_count) ?
	BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_VALID;
    int offer_options = options_allowed && cmd_schema_has_options(schema);
    cmd_schema_set_result(result, state, cursor_arg,
	(offer_options ? BU_CMD_EXPECT_OPTION : BU_CMD_EXPECT_NONE) |
	(operand ? BU_CMD_EXPECT_OPERAND : BU_CMD_EXPECT_NONE),
	operand ? operand->value_type : BU_CMD_VALUE_STRING,
	operand ? operand->name : (operand_count ? "operand" : "option or operand expected"),
	operand ? operand->semantic_provider : NULL);
    if (operand)
	result->candidate_validate = operand->validate;
    if (offer_options)
	cmd_schema_add_option_candidates(schema, result, "");
    if (!result->completion_count && operand)
	cmd_schema_add_keyword_candidates(operand->value_keywords, operand->keyword_values,
	    operand->shape, result, "");
    return 0;
}


static int
cmd_schema_validate_known_shape(const struct bu_cmd_operand *operand,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    if (!operand || !operand->shape || !result)
	return 0;
    switch (operand->shape->kind) {
	case BU_CMD_ARG_SHAPE_RGB:
	    return bu_cmd_rgb_optional_validate(argc, argv, cursor_arg, result);
	case BU_CMD_ARG_SHAPE_COLOR:
	    return bu_cmd_color_optional_validate(argc, argv, cursor_arg, result);
	case BU_CMD_ARG_SHAPE_VECTOR3:
	    return bu_cmd_vector3_optional_validate(argc, argv, cursor_arg, result);
	default:
	    break;
    }
    return 0;
}


static int
cmd_schema_has_known_operand_shape(const struct bu_cmd_schema *schema)
{
    if (!schema)
	return 0;
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++) {
	    const struct bu_cmd_arg_shape *shape = schema->operands[i].shape;
	    if (shape && (shape->kind == BU_CMD_ARG_SHAPE_RGB ||
		    shape->kind == BU_CMD_ARG_SHAPE_COLOR ||
		    shape->kind == BU_CMD_ARG_SHAPE_VECTOR3))
		return 1;
	}
    }
    if (schema->operand_groups) {
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand *roles = schema->operand_groups[gi].roles;
	    for (size_t ri = 0; roles && roles[ri].name; ri++) {
		const struct bu_cmd_arg_shape *shape = roles[ri].shape;
		if (shape && (shape->kind == BU_CMD_ARG_SHAPE_RGB ||
			shape->kind == BU_CMD_ARG_SHAPE_COLOR ||
			shape->kind == BU_CMD_ARG_SHAPE_VECTOR3))
		    return 1;
	    }
	}
    }
    return 0;
}


static int
cmd_schema_validate_operand_shapes(const struct bu_cmd_schema *schema,
	size_t argc, const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    const char **values = NULL;
    size_t *indexes = NULL;
    size_t count;
    size_t start = 0;
    int ret = 0;

    if (!schema || !result)
	return -1;
    /* Most schemas have no standard composite positional type.  Avoid two
     * argv-sized allocations on every interactive validation in that common
     * case. */
    if (!cmd_schema_has_known_operand_shape(schema))
	return 0;
    count = cmd_schema_collect_operands(schema, argc, argv, &values, &indexes,
	NULL);
    while (start < count) {
	const struct bu_cmd_operand *operand = bu_cmd_schema_operand(schema, start);
	size_t end = start + 1;
	size_t shape_cursor = end - start;
	int cursor_relevant = 0;
	struct bu_cmd_validate_result shaped = BU_CMD_VALIDATE_RESULT_NULL;

	while (end < count && bu_cmd_schema_operand(schema, end) == operand)
	    end++;
	if (!operand || !operand->shape) {
	    start = end;
	    continue;
	}
	for (size_t i = start; i < end; i++) {
	    if (indexes[i] == cursor_arg) {
		shape_cursor = i - start;
		cursor_relevant = 1;
		break;
	    }
	}
	if (cursor_arg >= argc && end == count) {
	    shape_cursor = end - start;
	    cursor_relevant = 1;
	}
	ret = cmd_schema_validate_known_shape(operand, end - start,
	    values + start, shape_cursor, &shaped);
	if (ret)
	    goto done;
	if (shaped.state == BU_CMD_VALIDATE_INVALID ||
	    shaped.state == BU_CMD_VALIDATE_INCOMPLETE || cursor_relevant) {
	    size_t relative_start = shaped.token_start;
	    size_t relative_end = shaped.token_end;

	    if (shaped.state == BU_CMD_VALIDATE_UNKNOWN) {
		bu_cmd_validate_result_clear(&shaped);
		start = end;
		continue;
	    }
	    shaped.token_start = relative_start < end - start ?
		indexes[start + relative_start] : cursor_arg;
	    shaped.token_end = relative_end < end - start ?
		indexes[start + relative_end] : shaped.token_start;
	    if (!shaped.semantic_provider)
		shaped.semantic_provider = operand->semantic_provider;
	    shaped.candidate_validate = operand->validate;
	    bu_cmd_validate_result_clear(result);
	    *result = shaped;
	    shaped.completion_candidates = NULL;
	    shaped.completion_count = 0;
	    if (result->state != BU_CMD_VALIDATE_VALID || cursor_relevant)
		goto done;
	}
	bu_cmd_validate_result_clear(&shaped);
	start = end;
    }

done:
    if (values)
	bu_free((void *)values, "command schema positional values");
    if (indexes)
	bu_free(indexes, "command schema positional indexes");
    return ret;
}


int
bu_cmd_schema_validate_syntax(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    static const struct bu_cmd_operand terminal_operands[] = {
	BU_CMD_OPERAND("argument", BU_CMD_VALUE_RAW, 0, BU_CMD_COUNT_UNLIMITED,
	    "ignored after a terminal option", NULL),
	BU_CMD_OPERAND_NULL
    };
    struct bu_cmd_schema view;
    struct bu_cmd_schema terminal_view;
    const struct bu_cmd_schema *effective;
    int terminal;
    int ret;

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (!schema || cursor_arg > argc || (argc && !argv))
	return -1;
    effective = cmd_schema_case_view(schema, argc, argv, &view);
    terminal = cmd_schema_terminal_option_present(schema, argc, argv);
    if (terminal) {
	terminal_view = *effective;
	terminal_view.operands = terminal_operands;
	terminal_view.operand_groups = NULL;
	terminal_view.validation.constraints = NULL;
	terminal_view.validation.cases = NULL;
	effective = &terminal_view;
    }
    ret = _bu_cmd_schema_validate_structure(effective, argc, argv, cursor_arg,
	result, NULL);

    if (ret) {
	bu_cmd_validate_result_clear(result);
	return ret;
    }
    if (result->state == BU_CMD_VALIDATE_INVALID)
	return ret;
    if (!terminal) {
	ret = cmd_schema_validate_operand_shapes(effective, argc, argv, cursor_arg,
	    result);
	if (ret) {
	    bu_cmd_validate_result_clear(result);
	    return ret;
	}
	if (result->state == BU_CMD_VALIDATE_INVALID)
	    return ret;
    }
    if (cursor_arg >= argc) {
	ret = cmd_schema_apply_constraints(effective, argc, argv, result);
	if (ret) {
	    bu_cmd_validate_result_clear(result);
	    return ret;
	}
	if (terminal && result->state == BU_CMD_VALIDATE_VALID)
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_VALID, cursor_arg,
		BU_CMD_EXPECT_NONE, BU_CMD_VALUE_FLAG, "terminal option", NULL);
    }
    return 0;
}


int
bu_cmd_schema_validate(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg,
	struct bu_cmd_validate_result *result)
{
    int ret = bu_cmd_schema_validate_syntax(schema, argc, argv, cursor_arg,
	result);

    if (ret || result->state == BU_CMD_VALIDATE_INVALID ||
	cmd_schema_terminal_option_present(schema, argc, argv))
	return ret;
    if (schema->validation.custom_validate) {
	ret = schema->validation.custom_validate(schema, argc, argv, cursor_arg,
	    result);
	if (ret)
	    bu_cmd_validate_result_clear(result);
	return ret;
    }
    return 0;
}


int
bu_cmd_schema_validate_ctx(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    int ret;

    if (!result)
	return -1;
    if (!schema || cursor_arg > argc || (argc && !argv)) {
	bu_cmd_validate_result_clear(result);
	return -1;
    }
    ret = bu_cmd_schema_validate(schema, argc, argv, cursor_arg, result);
    if (ret || result->state == BU_CMD_VALIDATE_INVALID ||
	cmd_schema_terminal_option_present(schema, argc, argv))
	return ret;
    if (context && schema->validation.context_validate) {
	ret = schema->validation.context_validate(schema, argc, argv,
	    cursor_arg, context, result);
	if (ret)
	    bu_cmd_validate_result_clear(result);
	return ret;
    }
    return 0;
}


static const char *
cmd_tree_phase_name(bu_cmd_tree_child_phase_t phase)
{
    switch (phase) {
	case BU_CMD_TREE_CHILD_AFTER_OPTIONS: return "after_options";
	case BU_CMD_TREE_CHILD_FIRST: return "first_argument";
	case BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS: return "after_fixed_operands";
	default: break;
    }
    return "unknown";
}


static int
cmd_tree_node_matches(const struct bu_cmd_tree_node *node, const char *name)
{
    if (!node || !node->schema || !node->schema->name || !name)
	return 0;
    if (BU_STR_EQUAL(node->schema->name, name))
	return 1;
    if (node->aliases) {
	for (size_t i = 0; node->aliases[i]; i++) {
	    if (BU_STR_EQUAL(node->aliases[i], name))
		return 1;
	}
    }
    return 0;
}


static int
cmd_tree_nodes_overlap(const struct bu_cmd_tree_node *a,
	const struct bu_cmd_tree_node *b)
{
    if (!a || !b || !a->schema || !b->schema)
	return 0;
    if (cmd_tree_node_matches(a, b->schema->name) ||
	cmd_tree_node_matches(b, a->schema->name))
	return 1;
    if (a->aliases) {
	for (size_t i = 0; a->aliases[i]; i++) {
	    if (cmd_tree_node_matches(b, a->aliases[i]))
		return 1;
	}
    }
    return 0;
}


const struct bu_cmd_tree_node *
bu_cmd_tree_find_subcommand(const struct bu_cmd_tree *tree, const char *name)
{
    if (!tree || !tree->subcommands || !name)
	return NULL;
    for (size_t i = 0; tree->subcommands[i].schema; i++) {
	if (cmd_tree_node_matches(&tree->subcommands[i], name))
	    return &tree->subcommands[i];
    }
    return NULL;
}


static size_t cmd_tree_subcommand_index(const struct bu_cmd_tree *, size_t,
	const char **);


int
bu_cmd_tree_dispatch(const struct bu_cmd_tree *tree, void *context,
	int argc, const char *argv[], int *result)
{
    const struct bu_cmd_tree_node *node;
    const char **canonical_argv = NULL;
    int command_result;

    size_t subcommand_index;
    struct bu_cmd_validate_result root = BU_CMD_VALIDATE_RESULT_NULL;

    if (!tree || !tree->root_schema || argc < 1 || !argv)
	return -1;
    subcommand_index = cmd_tree_subcommand_index(tree, (size_t)argc, argv);
    if (subcommand_index >= (size_t)argc ||
	bu_cmd_schema_validate_ctx(tree->root_schema, subcommand_index, argv,
	    subcommand_index, context, &root) != 0 ||
	root.state != BU_CMD_VALIDATE_VALID) {
	bu_cmd_validate_result_clear(&root);
	return -1;
    }
    bu_cmd_validate_result_clear(&root);
    argc -= (int)subcommand_index;
    argv += subcommand_index;
    node = bu_cmd_tree_find_subcommand(tree, argv[0]);
    if (!node)
	return -1;
    if (!node->execute && node->subcommands && argc > 1) {
	const struct bu_cmd_tree child_tree = {
	    node->schema, node->subcommands, node->child_phase
	};
	return bu_cmd_tree_dispatch(&child_tree, context, argc - 1, argv + 1,
	    result);
    }
    if (!node->execute)
	return -1;

    /* Executors never need to know which spelling matched their node. */
    if (!BU_STR_EQUAL(node->schema->name, argv[0])) {
	canonical_argv = (const char **)bu_malloc((size_t)argc * sizeof(const char *),
		"native command tree canonical argv");
	memcpy(canonical_argv, argv, (size_t)argc * sizeof(const char *));
	canonical_argv[0] = node->schema->name;
	argv = canonical_argv;
    }

    command_result = node->execute(context, argc, argv);
    if (canonical_argv)
	bu_free(canonical_argv, "native command tree canonical argv");
    if (result)
	*result = command_result;
    return 0;
}


static size_t
cmd_tree_subcommand_index(const struct bu_cmd_tree *tree, size_t argc,
	const char **argv)
{
    size_t required_operands = 0;
    size_t operand_count = 0;

    if (!tree || !tree->root_schema)
	return argc;
    if (tree->child_phase == BU_CMD_TREE_CHILD_FIRST)
	return argc ? 0 : argc;
    if (tree->child_phase == BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS) {
	if (!tree->root_schema->operands)
	    return argc;
	for (size_t oi = 0; tree->root_schema->operands[oi].name; oi++) {
	    const struct bu_cmd_operand *operand = &tree->root_schema->operands[oi];
	    if (operand->min_count != operand->max_count ||
		operand->max_count == BU_CMD_COUNT_UNLIMITED ||
		(operand->shape && (operand->shape->min_tokens != 1 ||
		    operand->shape->max_tokens != 1 || operand->shape->token_count)))
		return argc;
	    required_operands += operand->min_count;
	}
	if (!required_operands)
	    return argc;
	for (size_t i = 0; i < argc; i++) {
	    int span;
	    if (operand_count >= required_operands)
		return i;
	    span = bu_cmd_schema_option_span(tree->root_schema, argc - i, argv + i);
	    if (span > 0) {
		i += (size_t)span - 1;
		continue;
	    }
	    if (span < 0 || (argv[i][0] == '-' && argv[i][1]))
		return argc;
	    operand_count++;
	}
	return argc;
    }
    for (size_t i = 0; i < argc; i++) {
	int span = bu_cmd_schema_option_span(tree->root_schema, argc - i, argv + i);
	if (span > 0) {
	    i += (size_t)span - 1;
	    continue;
	}
	/* Leave an unrecognized or malformed option in the root phase, where
	 * the root schema can report the precise diagnostic. */
	if (span < 0 || (argv[i][0] == '-' && argv[i][1]))
	    return argc;
	return i;
    }
    return argc;
}


static void
cmd_tree_subcommand_candidates(const struct bu_cmd_tree *tree,
	struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;
    size_t prefix_len = prefix ? strlen(prefix) : 0;

    if (!result)
	return;
    cmd_schema_clear_completion_candidates(result);
    if (!tree || !tree->subcommands)
	return;
    for (size_t i = 0; tree->subcommands[i].schema; i++) {
	const char *name = tree->subcommands[i].schema->name;
	if (name && (!prefix_len || bu_strncmp(name, prefix, prefix_len) == 0))
	    count++;
    }
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "native command tree candidates");
    for (size_t i = 0; tree->subcommands[i].schema; i++) {
	const char *name = tree->subcommands[i].schema->name;
	if (name && (!prefix_len || bu_strncmp(name, prefix, prefix_len) == 0))
	    result->completion_candidates[result->completion_count++] = bu_strdup(name);
    }
}


static int
cmd_tree_validate_argv_ctx(const struct bu_cmd_tree *tree, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    const struct bu_cmd_tree_node *node;
    struct bu_cmd_validate_result root_result = BU_CMD_VALIDATE_RESULT_NULL;
    size_t subcommand_index;
    const char *prefix = "";
    int ret;

    if (!tree || !tree->root_schema || !result || cursor_arg > argc ||
	(argc && !argv))
	return -1;
    subcommand_index = cmd_tree_subcommand_index(tree, argc, argv);
    if (cursor_arg < subcommand_index)
	return bu_cmd_schema_validate_ctx(tree->root_schema, subcommand_index,
	    argv, cursor_arg, context, result);
    ret = bu_cmd_schema_validate_ctx(tree->root_schema, subcommand_index, argv,
	subcommand_index, context, &root_result);
    if (ret || root_result.state == BU_CMD_VALIDATE_INVALID ||
	root_result.state == BU_CMD_VALIDATE_INCOMPLETE) {
	if (!ret) {
	    bu_cmd_validate_result_clear(result);
	    *result = root_result;
	    root_result.completion_count = 0;
	    root_result.completion_candidates = NULL;
	}
	bu_cmd_validate_result_clear(&root_result);
	return ret;
    }
    bu_cmd_validate_result_clear(&root_result);

    if (subcommand_index >= argc) {
	cmd_schema_set_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
	    BU_CMD_EXPECT_SUBCOMMAND, BU_CMD_VALUE_KEYWORD, "subcommand expected", NULL);
	cmd_tree_subcommand_candidates(tree, result, "");
	return 0;
    }
    prefix = argv[subcommand_index] ? argv[subcommand_index] : "";
    if (cursor_arg == subcommand_index) {
	int exact = bu_cmd_tree_find_subcommand(tree, prefix) != NULL;
	cmd_schema_set_result(result, exact ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE,
	    subcommand_index, BU_CMD_EXPECT_SUBCOMMAND, BU_CMD_VALUE_KEYWORD,
	    exact ? "subcommand" : "subcommand expected", NULL);
	cmd_tree_subcommand_candidates(tree, result, prefix);
	if (!exact && !result->completion_count) {
	    result->state = BU_CMD_VALIDATE_INVALID;
	    result->hint = "unknown subcommand";
	}
	return 0;
    }
    node = bu_cmd_tree_find_subcommand(tree, prefix);
    if (!node) {
	cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, subcommand_index,
	    BU_CMD_EXPECT_SUBCOMMAND, BU_CMD_VALUE_KEYWORD, "unknown subcommand", NULL);
	cmd_tree_subcommand_candidates(tree, result, prefix);
	return 0;
    }
    if (node->subcommands) {
	const struct bu_cmd_tree child_tree = {
	    node->schema, node->subcommands, node->child_phase
	};
	ret = cmd_tree_validate_argv_ctx(&child_tree,
	    argc - subcommand_index - 1, argv + subcommand_index + 1,
	    cursor_arg - subcommand_index - 1, context, result);
	if (!ret) {
	    result->token_start += subcommand_index + 1;
	    result->token_end += subcommand_index + 1;
	}
	return ret;
    }
    ret = bu_cmd_schema_validate_ctx(node->schema,
	argc - subcommand_index - 1, argv + subcommand_index + 1,
	cursor_arg - subcommand_index - 1, context, result);
    if (!ret) {
	result->token_start += subcommand_index + 1;
	result->token_end += subcommand_index + 1;
    }
    return ret;
}


int
bu_cmd_tree_validate_argv_ctx(const struct bu_cmd_tree *tree, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    int ret;

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (!tree || !tree->root_schema || cursor_arg > argc || (argc && !argv))
	return -1;
    ret = cmd_tree_validate_argv_ctx(tree, argc, argv, cursor_arg, context,
	result);
    if (ret)
	bu_cmd_validate_result_clear(result);
    return ret;
}


int
bu_cmd_tree_validate_argv(const struct bu_cmd_tree *tree, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    return bu_cmd_tree_validate_argv_ctx(tree, argc, argv, cursor_arg, NULL,
	result);
}


static void
cmd_tree_describe_children(struct bu_vls *out, const struct bu_cmd_tree_node *nodes,
	int depth)
{
    if (!out || !nodes || depth > 64)
	return;
    for (size_t i = 0; nodes[i].schema; i++) {
	const struct bu_cmd_tree_node *node = &nodes[i];
	const struct bu_cmd_schema *schema = node->schema;
	struct bu_vls names = BU_VLS_INIT_ZERO;
	bu_vls_strcat(&names, schema->name);
	if (node->aliases) {
	    for (size_t ai = 0; node->aliases[ai]; ai++)
		bu_vls_printf(&names, ",%s", node->aliases[ai]);
	}
	bu_vls_printf(out, "%*s%-12s %s\n", depth * 2, "", bu_vls_cstr(&names),
	    schema->help ? schema->help : "");
	bu_vls_free(&names);
	if (node->subcommands)
	    cmd_tree_describe_children(out, node->subcommands, depth + 1);
    }
}


char *
bu_cmd_tree_help(const struct bu_cmd_tree *tree, const char *invocation)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *result = NULL;
    char *option_help = NULL;
    int have_options = 0;

    if (!tree || !tree->root_schema || !tree->root_schema->name)
	return NULL;

    if (BU_STR_EMPTY(invocation))
	invocation = tree->root_schema->name;
    if (tree->root_schema->options) {
	for (size_t i = 0;
		bu_cmd_option_is_valid(&tree->root_schema->options[i]); i++) {
	    if (!tree->root_schema->options[i].alias_of &&
		    !tree->root_schema->options[i].hidden) {
		have_options = 1;
		break;
	    }
	}
    }
    bu_vls_printf(&out, "Usage: %s%s", invocation,
	have_options ? " [options]" : "");
    if (tree->child_phase == BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS &&
	    tree->root_schema->operands) {
	for (size_t i = 0; tree->root_schema->operands[i].name; i++) {
	    const struct bu_cmd_operand *operand = &tree->root_schema->operands[i];
	    cmd_schema_usage_atom(&out, operand->name, operand->min_count,
		operand->max_count);
	}
    }
    bu_vls_strcat(&out, " subcommand [args ...]\n");
    if (!BU_STR_EMPTY(tree->root_schema->help))
	bu_vls_printf(&out, "\n%s\n", tree->root_schema->help);
    option_help = bu_cmd_schema_describe(tree->root_schema);
    if (option_help && option_help[0])
	bu_vls_printf(&out, "\nOptions:\n%s", option_help);
    if (option_help)
	bu_free(option_help, "native tree option help");
    if (tree->subcommands) {
	bu_vls_strcat(&out, "\nSubcommands:\n");
	cmd_tree_describe_children(&out, tree->subcommands, 1);
    }
    result = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return result;
}


int
bu_cmd_tree_help_append(struct bu_vls *output,
	const struct bu_cmd_tree *tree, const char *invocation)
{
    char *help;

    if (!output)
	return -1;
    help = bu_cmd_tree_help(tree, invocation);
    if (!help)
	return -1;
    bu_vls_strcat(output, help);
    bu_free(help, "command tree help");
    return 0;
}


char *
bu_cmd_tree_help_path(const struct bu_cmd_tree *tree, const char *invocation,
	size_t path_argc, const char * const *path_argv)
{
    const struct bu_cmd_tree *current = tree;
    struct bu_cmd_tree child_tree;
    struct bu_vls qualified = BU_VLS_INIT_ZERO;
    const struct bu_cmd_tree_node *node = NULL;
    char *result = NULL;

    if (!tree || !tree->root_schema || (path_argc && !path_argv))
	return NULL;
    if (BU_STR_EMPTY(invocation))
	invocation = tree->root_schema->name;
    if (!path_argc)
	return bu_cmd_tree_help(tree, invocation);
    bu_vls_strcat(&qualified, invocation ? invocation : "");

    for (size_t i = 0; i < path_argc; i++) {
	node = bu_cmd_tree_find_subcommand(current, path_argv[i]);
	if (!node)
	    goto done;
	bu_vls_putc(&qualified, ' ');
	bu_vls_strcat(&qualified, path_argv[i]);
	if (i + 1 < path_argc) {
	    if (!node->subcommands)
		goto done;
	    child_tree.root_schema = node->schema;
	    child_tree.subcommands = node->subcommands;
	    child_tree.child_phase = node->child_phase;
	    current = &child_tree;
	}
    }
    if (node && node->subcommands) {
	child_tree.root_schema = node->schema;
	child_tree.subcommands = node->subcommands;
	child_tree.child_phase = node->child_phase;
	result = bu_cmd_tree_help(&child_tree, bu_vls_cstr(&qualified));
    } else if (node) {
	result = bu_cmd_schema_help(node->schema, bu_vls_cstr(&qualified));
    }

done:
    bu_vls_free(&qualified);
    return result;
}


char *
bu_cmd_tree_describe(const struct bu_cmd_tree *tree)
{
    return bu_cmd_tree_help(tree, NULL);
}


static void
cmd_tree_node_describe_json(struct bu_vls *out, const struct bu_cmd_tree_node *node,
	int depth)
{
    char *schema_json = NULL;

    if (!out || !node || !node->schema || depth > 64) {
	if (out)
	    bu_vls_strcat(out, "null");
	return;
    }
    schema_json = bu_cmd_schema_describe_json(node->schema);
    bu_vls_strcat(out, "{\"schema\":");
    bu_vls_strcat(out, schema_json ? schema_json : "{}");
    bu_vls_strcat(out, ",\"aliases\":[");
    if (node->aliases) {
	for (size_t i = 0; node->aliases[i]; i++) {
	    if (i)
		bu_vls_putc(out, ',');
	    bu_cmd_json_string(out, node->aliases[i]);
	}
    }
    bu_vls_strcat(out, "],\"child_phase\":");
    bu_cmd_json_string(out, cmd_tree_phase_name(node->child_phase));
    bu_vls_strcat(out, ",\"subcommands\":[");
    if (node->subcommands) {
	for (size_t i = 0; node->subcommands[i].schema; i++) {
	    if (i)
		bu_vls_putc(out, ',');
	    cmd_tree_node_describe_json(out, &node->subcommands[i], depth + 1);
	}
    }
    bu_vls_strcat(out, "]}");
    if (schema_json)
	bu_free(schema_json, "native tree node schema JSON");
}


char *
bu_cmd_tree_describe_json(const struct bu_cmd_tree *tree)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *result = NULL;
    char *root_json = NULL;

    if (!tree || !tree->root_schema)
	return NULL;
    root_json = bu_cmd_schema_describe_json(tree->root_schema);
    bu_vls_strcat(&out, "{\"kind\":\"native_tree\",\"root\":");
    bu_vls_strcat(&out, root_json ? root_json : "{}");
    bu_vls_strcat(&out, ",\"child_phase\":");
    bu_cmd_json_string(&out, cmd_tree_phase_name(tree->child_phase));
    bu_vls_strcat(&out, ",\"subcommands\":[");
    if (tree->subcommands) {
	for (size_t i = 0; tree->subcommands[i].schema; i++) {
	    if (i)
		bu_vls_putc(&out, ',');
	    cmd_tree_node_describe_json(&out, &tree->subcommands[i], 0);
	}
    }
    bu_vls_strcat(&out, "]}");
    if (root_json)
	bu_free(root_json, "native tree root schema JSON");
    result = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return result;
}


static int
cmd_schema_lint_range(const struct bu_cmd_schema *schema,
	const struct bu_cmd_value_range *range, bu_cmd_value_t type,
	const char *kind, const char *name, struct bu_vls *msgs)
{
    const char *path = schema && schema->name ? schema->name : "(unnamed)";
    int valid_type;

    if (!range || range->kind == BU_CMD_RANGE_NONE)
	return 0;
    valid_type = range->kind == BU_CMD_RANGE_INTEGER ?
	(type == BU_CMD_VALUE_INTEGER || type == BU_CMD_VALUE_HEX_INTEGER ||
	 type == BU_CMD_VALUE_LONG || type == BU_CMD_VALUE_HEX_LONG) :
	(range->kind == BU_CMD_RANGE_NUMBER && type == BU_CMD_VALUE_NUMBER);
    if (!valid_type) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has a range incompatible with its type\n",
		path, kind, name ? name : "");
	return 1;
    }
    if (range->kind == BU_CMD_RANGE_INTEGER && range->has_minimum &&
	range->has_maximum &&
	(range->integer_minimum > range->integer_maximum ||
	 (range->integer_minimum == range->integer_maximum &&
	  (!range->minimum_inclusive || !range->maximum_inclusive)))) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has an empty integer range\n",
		path, kind, name ? name : "");
	return 1;
    }
    if (range->kind == BU_CMD_RANGE_NUMBER &&
	((range->has_minimum && !isfinite((double)range->number_minimum)) ||
	 (range->has_maximum && !isfinite((double)range->number_maximum)) ||
	 (range->has_minimum && range->has_maximum &&
	  (range->number_minimum > range->number_maximum ||
	   (!(range->number_minimum < range->number_maximum) &&
	    (!range->minimum_inclusive || !range->maximum_inclusive)))))) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has an invalid number range\n",
		path, kind, name ? name : "");
	return 1;
    }
    return 0;
}


static int
cmd_schema_lint_arg_shape(const struct bu_cmd_schema *schema,
	const struct bu_cmd_arg_shape *shape, const char *kind,
	const char *name, struct bu_vls *msgs)
{
    const char *path = schema && schema->name ? schema->name : "(unnamed)";
    int failures = 0;
    int have_minimum = 0;
    int have_maximum = 0;

    if (!shape)
	return 0;
    if (shape->kind < BU_CMD_ARG_SHAPE_SCALAR ||
	shape->kind > BU_CMD_ARG_SHAPE_CUSTOM ||
	(shape->max_tokens != BU_CMD_COUNT_UNLIMITED &&
	 shape->min_tokens > shape->max_tokens)) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has an invalid argument shape\n",
		path, kind, name ? name : "");
	failures++;
    }
    if (!shape->variants)
	return failures;

    for (size_t i = 0; shape->variants[i].name; i++) {
	const struct bu_cmd_arg_variant *variant = &shape->variants[i];
	if (!variant->name[0] ||
	    (variant->token_count && BU_STR_EMPTY(variant->syntax)) ||
	    variant->token_count < shape->min_tokens ||
	    (shape->max_tokens != BU_CMD_COUNT_UNLIMITED &&
	     variant->token_count > shape->max_tokens)) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s: %s \"%s\" has invalid argument form \"%s\"\n",
		    path, kind, name ? name : "", variant->name);
	    failures++;
	}
	if (variant->token_count == shape->min_tokens)
	    have_minimum = 1;
	if (shape->max_tokens != BU_CMD_COUNT_UNLIMITED &&
	    variant->token_count == shape->max_tokens)
	    have_maximum = 1;
	for (size_t j = 0; j < i; j++) {
	    if (BU_STR_EQUAL(variant->name, shape->variants[j].name) ||
		(!BU_STR_EMPTY(variant->syntax) &&
		 BU_STR_EQUAL(variant->syntax, shape->variants[j].syntax))) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: %s \"%s\" has duplicate argument form \"%s\"\n",
			path, kind, name ? name : "", variant->name);
		failures++;
		break;
	    }
	}
    }
    {
	const struct bu_cmd_arg_variant *sentinel = shape->variants;
	while (sentinel->name)
	    sentinel++;
	if (sentinel->syntax || sentinel->token_count || sentinel->help) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s: %s \"%s\" has a malformed argument-form terminator\n",
		    path, kind, name ? name : "");
	    failures++;
	}
    }
    if (!have_minimum ||
	(shape->max_tokens != BU_CMD_COUNT_UNLIMITED && !have_maximum)) {
	if (msgs)
	    bu_vls_printf(msgs,
		"%s: %s \"%s\" argument forms do not describe the shape bounds\n",
		path, kind, name ? name : "");
	failures++;
    }
    return failures;
}


static int
cmd_schema_lint_operand(const struct bu_cmd_schema *schema,
	const struct bu_cmd_operand *operand, const char *kind,
	struct bu_vls *msgs)
{
    const char *path = schema && schema->name ? schema->name : "(unnamed)";
    int failures = 0;

    if (!operand || !operand->name || !operand->name[0]) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s has no name\n", path, kind);
	return 1;
    }
    if (operand->max_count != BU_CMD_COUNT_UNLIMITED &&
	operand->min_count > operand->max_count) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has an invalid count range\n",
		path, kind, operand->name);
	failures++;
    }
    if (operand->value_type < BU_CMD_VALUE_FLAG ||
	operand->value_type > BU_CMD_VALUE_UNKNOWN) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" has an invalid value type\n",
		path, kind, operand->name);
	failures++;
    }
    if (operand->value_type == BU_CMD_VALUE_CUSTOM) {
	if (msgs)
	    bu_vls_printf(msgs,
		"%s: %s \"%s\" cannot use an option-only custom parser\n",
		path, kind, operand->name);
	failures++;
    }
    failures += cmd_schema_lint_arg_shape(schema, operand->shape, kind,
	operand->name, msgs);
    failures += cmd_schema_lint_range(schema, &operand->range,
	operand->value_type, kind, operand->name, msgs);
    return failures;
}


static int
cmd_schema_lint_keywords(const struct bu_cmd_schema *schema, const char *kind,
	const char *name, const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values, struct bu_vls *msgs)
{
    const char *path = schema && schema->name ? schema->name : "(unnamed)";
    int failures = 0;

    if (keywords && keyword_values) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: %s \"%s\" declares both simple and rich keywords\n",
		path, kind, name ? name : "");
	return 1;
    }
    if (keywords) {
	for (size_t i = 0; keywords[i]; i++) {
	    if (!keywords[i][0]) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: %s \"%s\" has an empty keyword\n",
			path, kind, name ? name : "");
		failures++;
	    }
	    for (size_t j = 0; j < i; j++) {
		if (BU_STR_EQUAL(keywords[i], keywords[j])) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: %s \"%s\" has duplicate keyword \"%s\"\n",
			    path, kind, name ? name : "", keywords[i]);
		    failures++;
		    break;
		}
	    }
	}
	return failures;
    }
    if (!keyword_values)
	return 0;
    for (size_t i = 0; keyword_values[i].canonical; i++) {
	const struct bu_cmd_value_keyword *keyword = &keyword_values[i];
	if (!keyword->canonical[0])
	    failures++;
	for (size_t ai = 0; keyword->aliases && keyword->aliases[ai]; ai++) {
	    const char *spelling = keyword->aliases[ai];
	    if (!spelling[0] || BU_STR_EQUAL(spelling, keyword->canonical)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: %s \"%s\" has an invalid keyword alias \"%s\"\n",
			path, kind, name ? name : "", spelling);
		failures++;
	    }
	}
	for (size_t j = 0; j < i; j++) {
	    const struct bu_cmd_value_keyword *previous = &keyword_values[j];
	    if (BU_STR_EQUAL(keyword->canonical, previous->canonical))
		failures++;
	    for (size_t aj = 0; previous->aliases && previous->aliases[aj]; aj++)
		if (BU_STR_EQUAL(keyword->canonical, previous->aliases[aj]))
		    failures++;
	    for (size_t ai = 0; keyword->aliases && keyword->aliases[ai]; ai++) {
		if (BU_STR_EQUAL(keyword->aliases[ai], previous->canonical))
		    failures++;
		for (size_t aj = 0; previous->aliases && previous->aliases[aj]; aj++)
		    if (BU_STR_EQUAL(keyword->aliases[ai], previous->aliases[aj]))
			failures++;
	    }
	}
	for (size_t ai = 0; keyword->aliases && keyword->aliases[ai]; ai++)
	    for (size_t aj = 0; aj < ai; aj++)
		if (BU_STR_EQUAL(keyword->aliases[ai], keyword->aliases[aj]))
		    failures++;
    }
    if (failures && msgs)
	bu_vls_printf(msgs, "%s: %s \"%s\" has invalid rich keyword declarations\n",
	    path, kind, name ? name : "");
    return failures;
}


static int
cmd_schema_case_same_predicate(const struct bu_cmd_schema_case *left,
	const struct bu_cmd_schema_case *right)
{
    size_t left_count = 0;
    size_t right_count = 0;

    if (!left || !right || left->condition != right->condition)
	return 0;
    if (left->condition == BU_CMD_CONDITION_ALWAYS)
	return 1;
    while (left->options && left->options[left_count])
	left_count++;
    while (right->options && right->options[right_count])
	right_count++;
    if (left_count != right_count)
	return 0;
    for (size_t li = 0; li < left_count; li++) {
	int found = 0;
	for (size_t ri = 0; ri < right_count; ri++) {
	    if (BU_STR_EQUAL(left->options[li], right->options[ri])) {
		found = 1;
		break;
	    }
	}
	if (!found)
	    return 0;
    }
    for (size_t ri = 0; ri < right_count; ri++) {
	int found = 0;
	for (size_t li = 0; li < left_count; li++) {
	    if (BU_STR_EQUAL(right->options[ri], left->options[li])) {
		found = 1;
		break;
	    }
	}
	if (!found)
	    return 0;
    }
    return 1;
}


int
bu_cmd_schema_lint(const struct bu_cmd_schema *schema, struct bu_vls *msgs)
{
    int failures = 0;
    const char *path;

    if (!schema) {
	if (msgs)
	    bu_vls_strcat(msgs, "null native schema\n");
	return 1;
    }
    path = schema->name && schema->name[0] ? schema->name : "(unnamed)";
    if (!schema->name || !schema->name[0]) {
	if (msgs)
	    bu_vls_strcat(msgs, "native schema has no name\n");
	failures++;
    }
    if (schema->parse_policy < BU_CMD_PARSE_INTERSPERSED ||
	schema->parse_policy > BU_CMD_PARSE_STOP_AT_FIRST_OPERAND) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: invalid parse policy\n", path);
	failures++;
    }
    if (schema->validation.terminal_flags & ~BU_CMD_TERMINAL_MASK) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: invalid terminal-action flags\n", path);
	failures++;
    }
    if (schema->options) {
	for (size_t i = 0; bu_cmd_option_is_valid(&schema->options[i]); i++) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    const struct bu_cmd_arg_shape *shape =
		cmd_schema_option_arg_shape(option);
	    const char *canonical = bu_cmd_option_canonical(option);
	    if (!canonical || !canonical[0]) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: option %lu has no canonical name\n",
			path, (unsigned long)i);
		failures++;
	    }
	    if (option->value_type < BU_CMD_VALUE_FLAG ||
		option->value_type > BU_CMD_VALUE_CUSTOM ||
		option->arg_requirement < BU_CMD_ARG_REQUIRED ||
		option->arg_requirement > BU_CMD_ARG_NONE) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: option \"%s\" has invalid type metadata\n",
			path, canonical ? canonical : "");
		failures++;
	    }
	    if (option->value_type >= BU_CMD_VALUE_FLAG &&
		option->value_type <= BU_CMD_VALUE_CUSTOM &&
		option->arg_requirement >= BU_CMD_ARG_REQUIRED &&
		option->arg_requirement <= BU_CMD_ARG_NONE) {
		/* Most typed values consume an argument.  A repeatable typed
		 * no-argument option is the counting-helper exception. */
		if ((option->value_type == BU_CMD_VALUE_FLAG &&
			option->arg_requirement != BU_CMD_ARG_NONE) ||
		    (option->value_type != BU_CMD_VALUE_FLAG &&
			option->value_type != BU_CMD_VALUE_CUSTOM &&
			option->arg_requirement == BU_CMD_ARG_NONE &&
			!option->alias_of && !option->repeat)) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: option \"%s\" has incompatible value and argument requirements\n",
			    path, canonical ? canonical : "");
		    failures++;
		}
	    }
	    if (shape && option->arg_requirement == BU_CMD_ARG_NONE &&
		shape->max_tokens) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: flag \"%s\" declares an argument shape\n",
			path, canonical ? canonical : "");
		failures++;
	    }
	    if (shape && shape->max_tokens != 1 && !option->consume &&
		!schema->validation.external_execution) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: option \"%s\" needs an argument-shape consumer\n",
			path, canonical ? canonical : "");
		failures++;
	    }
	    if (option->value_type == BU_CMD_VALUE_CUSTOM &&
		!option->custom_parse && !option->consume && !option->alias_of &&
		!schema->validation.external_execution) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: custom option \"%s\" has no parser or consumer\n",
			path, canonical ? canonical : "");
		failures++;
	    }
	    failures += cmd_schema_lint_arg_shape(schema, shape, "option",
		canonical, msgs);
	    failures += cmd_schema_lint_range(schema, &option->range,
		option->value_type, "option", canonical, msgs);
	    failures += cmd_schema_lint_keywords(schema, "option", canonical,
		option->value_keywords, option->keyword_values, msgs);
	    if (!option->alias_of && option->storage_offset == BU_CMD_STORAGE_NONE &&
		!schema->validation.external_execution) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: option \"%s\" has no execution binding; declare an external schema\n",
			path, canonical ? canonical : "");
		failures++;
	    }
	    if (option->alias_of) {
		int found = 0;
		for (size_t j = 0; bu_cmd_option_is_valid(&schema->options[j]); j++) {
		    if (!schema->options[j].alias_of &&
			BU_STR_EQUAL(bu_cmd_option_canonical(&schema->options[j]),
			    option->alias_of)) {
			found = 1;
			break;
		    }
		}
		if (!found) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: option alias \"%s\" has no target\n",
			    path, canonical ? canonical : "");
		    failures++;
		}
	    }
	    for (size_t j = 0; j < i; j++) {
		if ((option->shortopt && option->shortopt[0] &&
		     schema->options[j].shortopt && schema->options[j].shortopt[0] &&
		     BU_STR_EQUAL(option->shortopt, schema->options[j].shortopt)) ||
		    (option->longopt && option->longopt[0] &&
		     schema->options[j].longopt && schema->options[j].longopt[0] &&
		     BU_STR_EQUAL(option->longopt, schema->options[j].longopt))) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: duplicate option spelling for \"%s\"\n",
			    path, canonical ? canonical : "");
		    failures++;
		    break;
		}
	    }
	}
	if ((schema->validation.terminal_flags & BU_CMD_TERMINAL_HELP) &&
	    !cmd_schema_find_canonical(schema, "help")) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s: help terminal action requires a canonical help option\n",
		    path);
	    failures++;
	}
	if ((schema->validation.terminal_flags & BU_CMD_TERMINAL_VERSION) &&
	    !cmd_schema_find_canonical(schema, "version")) {
	    if (msgs)
		bu_vls_printf(msgs,
		    "%s: version terminal action requires a canonical version option\n",
		    path);
	    failures++;
	}
	if (schema->validation.terminal_options) {
	    for (size_t ti = 0; schema->validation.terminal_options[ti]; ti++) {
		int found = 0;
		if (!schema->validation.terminal_options[ti][0] ||
		    ((schema->validation.terminal_flags & BU_CMD_TERMINAL_HELP) &&
		     BU_STR_EQUAL(schema->validation.terminal_options[ti], "help")) ||
		    ((schema->validation.terminal_flags & BU_CMD_TERMINAL_VERSION) &&
		     BU_STR_EQUAL(schema->validation.terminal_options[ti], "version"))) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: duplicate or empty terminal action \"%s\"\n",
			    path, schema->validation.terminal_options[ti]);
		    failures++;
		}
		for (size_t pi = 0; pi < ti; pi++) {
		    if (BU_STR_EQUAL(schema->validation.terminal_options[pi],
			    schema->validation.terminal_options[ti])) {
			if (msgs)
			    bu_vls_printf(msgs,
				"%s: duplicate terminal action \"%s\"\n", path,
				schema->validation.terminal_options[ti]);
			failures++;
			break;
		    }
		}
		for (size_t oi = 0; bu_cmd_option_is_valid(&schema->options[oi]); oi++) {
		    if (!schema->options[oi].alias_of &&
			BU_STR_EQUAL(bu_cmd_option_canonical(&schema->options[oi]),
			    schema->validation.terminal_options[ti])) {
			found = 1;
			break;
		    }
		}
		if (!found) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: terminal action references unknown option \"%s\"\n",
			    path, schema->validation.terminal_options[ti]);
		    failures++;
		}
	    }
	}
	} else if (schema->validation.terminal_flags ||
	    schema->validation.terminal_options) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: terminal actions require an option table\n", path);
	failures++;
    }
    if (schema->operands) {
	for (size_t i = 0; schema->operands[i].name; i++)
	    failures += cmd_schema_lint_operand(schema, &schema->operands[i],
		"operand", msgs);
	for (size_t i = 0; schema->operands[i].name; i++)
	    failures += cmd_schema_lint_keywords(schema, "operand", schema->operands[i].name,
		schema->operands[i].value_keywords, schema->operands[i].keyword_values, msgs);
    }
    if (schema->operand_groups && schema->operand_groups[0].name) {
	if (schema->operands) {
	    for (size_t i = 0; schema->operands[i].name; i++) {
		const struct bu_cmd_operand *operand = &schema->operands[i];
		if (operand->min_count != operand->max_count ||
		    operand->max_count == BU_CMD_COUNT_UNLIMITED) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: operands before repeated groups must have fixed cardinality\n",
			    path);
		    failures++;
		    break;
		}
	    }
	}
	for (size_t gi = 0; schema->operand_groups[gi].name; gi++) {
	    const struct bu_cmd_operand_group *group = &schema->operand_groups[gi];
	    size_t width = 0;
	    if (!group->name[0] || !group->roles ||
		(group->max_count != BU_CMD_COUNT_UNLIMITED &&
		 group->min_count > group->max_count)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: malformed repeated operand group\n", path);
		failures++;
		continue;
	    }
	    while (group->roles[width].name) {
		const struct bu_cmd_operand *role = &group->roles[width];
		failures += cmd_schema_lint_operand(schema, role, "group role", msgs);
		if (role->min_count != 1 || role->max_count != 1 ||
		    (role->shape && (role->shape->min_tokens != 1 ||
			role->shape->max_tokens != 1))) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: repeated group role \"%s\" must be one scalar token\n",
			    path, role->name);
		    failures++;
		}
		width++;
	    }
	    if (!width) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: repeated operand group \"%s\" is empty\n",
			path, group->name);
		failures++;
	    }
	    if (group->max_count == BU_CMD_COUNT_UNLIMITED &&
		schema->operand_groups[gi + 1].name) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: an unlimited repeated operand group must be last\n", path);
		failures++;
	    }
	}
    }
    if (schema->validation.constraints) {
	for (size_t ci = 0; schema->validation.constraints[ci].options; ci++) {
	    const struct bu_cmd_constraint *constraint =
		&schema->validation.constraints[ci];
	    if (constraint->kind < BU_CMD_CONSTRAINT_OPTION_COUNT ||
		constraint->kind > BU_CMD_CONSTRAINT_OPTION_CONFLICTS ||
		constraint->condition < BU_CMD_CONDITION_ALWAYS ||
		constraint->condition > BU_CMD_CONDITION_ALL_OPTIONS_PRESENT ||
		constraint->min_count > constraint->max_count) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: malformed command constraint\n", path);
		failures++;
	    }
	    if (constraint->kind >= BU_CMD_CONSTRAINT_OPTION_REQUIRES &&
		(!constraint->options[0] || !constraint->options[1])) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: option relationship needs a trigger and target\n", path);
		failures++;
	    }
	    if (constraint->kind >= BU_CMD_CONSTRAINT_OPTION_REQUIRES &&
		(constraint->condition != BU_CMD_CONDITION_ALWAYS ||
		 constraint->min_count || constraint->max_count)) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: option relationships do not use a condition or count bounds\n",
			path);
		failures++;
	    }
	    for (size_t oi = 0; constraint->options[oi]; oi++) {
		int found = 0;
		for (size_t pi = 0; pi < oi; pi++) {
		    if (BU_STR_EQUAL(constraint->options[pi],
			    constraint->options[oi])) {
			if (msgs)
			    bu_vls_printf(msgs,
				"%s: constraint repeats option \"%s\"\n", path,
				constraint->options[oi]);
			failures++;
			break;
		    }
		}
		if (schema->options) {
		    for (size_t si = 0; bu_cmd_option_is_valid(&schema->options[si]); si++) {
			if (!schema->options[si].alias_of &&
			    BU_STR_EQUAL(bu_cmd_option_canonical(&schema->options[si]),
				constraint->options[oi])) {
			    found = 1;
			    break;
			}
		    }
		}
		if (!found) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: constraint references unknown option \"%s\"\n",
			    path, constraint->options[oi]);
		    failures++;
		}
	    }
	}
    }
    if (schema->validation.cases) {
	int saw_default = 0;
	for (size_t ci = 0; schema->validation.cases[ci].name; ci++) {
	    const struct bu_cmd_schema_case *cmd_case = &schema->validation.cases[ci];
	    struct bu_cmd_schema view = *schema;

	    if (!cmd_case->name[0] ||
		cmd_case->condition < BU_CMD_CONDITION_ALWAYS ||
		cmd_case->condition > BU_CMD_CONDITION_ALL_OPTIONS_PRESENT) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: malformed operand case\n", path);
		failures++;
	    }
	    for (size_t pj = 0; pj < ci; pj++) {
		if (BU_STR_EQUAL(cmd_case->name, schema->validation.cases[pj].name)) {
		    if (msgs)
			bu_vls_printf(msgs, "%s: duplicate operand case \"%s\"\n",
			    path, cmd_case->name);
		    failures++;
		    break;
		}
	    }
	    if (cmd_case->condition != BU_CMD_CONDITION_ALWAYS) {
		for (size_t pj = 0; pj < ci; pj++) {
		    if (cmd_schema_case_same_predicate(cmd_case,
			    &schema->validation.cases[pj])) {
			if (msgs)
			    bu_vls_printf(msgs,
				"%s: operand case \"%s\" repeats an earlier condition\n",
				path, cmd_case->name);
			failures++;
			break;
		    }
		}
	    }
	    if (cmd_case->condition == BU_CMD_CONDITION_ALWAYS) {
		if (saw_default || schema->validation.cases[ci + 1].name) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: the default operand case must be unique and last\n",
			    path);
		    failures++;
		}
		saw_default = 1;
	    } else if (!cmd_case->options || !cmd_case->options[0]) {
		if (msgs)
		    bu_vls_printf(msgs,
			"%s: operand case \"%s\" has no condition options\n",
			path, cmd_case->name);
		failures++;
	    }
	    for (size_t oi = 0; cmd_case->options && cmd_case->options[oi]; oi++) {
		int found = 0;
		for (size_t oj = 0; oj < oi; oj++) {
		    if (BU_STR_EQUAL(cmd_case->options[oi], cmd_case->options[oj])) {
			if (msgs)
			    bu_vls_printf(msgs,
				"%s: operand case \"%s\" repeats option \"%s\"\n",
				path, cmd_case->name, cmd_case->options[oi]);
			failures++;
			break;
		    }
		}
		for (size_t si = 0; schema->options &&
			bu_cmd_option_is_valid(&schema->options[si]); si++) {
		    if (!schema->options[si].alias_of &&
			BU_STR_EQUAL(bu_cmd_option_canonical(&schema->options[si]),
			    cmd_case->options[oi])) {
			found = 1;
			break;
		    }
		}
		if (!found) {
		    if (msgs)
			bu_vls_printf(msgs,
			    "%s: operand case \"%s\" references unknown option \"%s\"\n",
			    path, cmd_case->name, cmd_case->options[oi]);
		    failures++;
		}
	    }
	    view.operands = cmd_case->operands;
	    view.operand_groups = cmd_case->operand_groups;
	    view.validation.cases = NULL;
	    failures += bu_cmd_schema_lint(&view, msgs);
	}
	if (!saw_default) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: operand cases have no default\n", path);
	    failures++;
	}
    }
    return failures;
}


static int
cmd_tree_lint_nodes(const struct bu_cmd_tree_node *nodes, struct bu_vls *msgs,
	int depth)
{
    int failures = 0;

    if (!nodes)
	return 0;
    if (depth > 64) {
	if (msgs)
	    bu_vls_strcat(msgs, "native command tree exceeds nesting limit\n");
	return 1;
    }
    for (size_t i = 0; nodes[i].schema; i++) {
	const struct bu_cmd_tree_node *node = &nodes[i];
	failures += bu_cmd_schema_lint(node->schema, msgs);
	if (!node->schema->name || !node->schema->name[0]) {
	    if (msgs)
		bu_vls_strcat(msgs, "native command tree contains an unnamed child\n");
	    failures++;
	}
	if (node->child_phase != BU_CMD_TREE_CHILD_AFTER_OPTIONS &&
	    node->child_phase != BU_CMD_TREE_CHILD_FIRST &&
	    node->child_phase != BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS) {
	    if (msgs)
		bu_vls_printf(msgs, "native command tree child \"%s\" has an invalid child phase\n",
		    node->schema->name ? node->schema->name : "");
	    failures++;
	}
	for (size_t j = 0; j < i; j++) {
	    if (cmd_tree_nodes_overlap(&nodes[j], node)) {
		if (msgs)
		    bu_vls_printf(msgs, "native command tree has duplicate child \"%s\"\n",
			node->schema->name ? node->schema->name : "");
		failures++;
		break;
	    }
	}
	if (node->aliases) {
	    for (size_t ai = 0; node->aliases[ai]; ai++) {
		if (!node->aliases[ai][0] || BU_STR_EQUAL(node->aliases[ai], node->schema->name)) {
		    if (msgs)
			bu_vls_printf(msgs, "native command tree has an invalid alias for \"%s\"\n",
			    node->schema->name ? node->schema->name : "");
		    failures++;
		}
		for (size_t aj = 0; aj < ai; aj++) {
		    if (BU_STR_EQUAL(node->aliases[ai], node->aliases[aj])) {
			if (msgs)
			    bu_vls_printf(msgs, "native command tree has duplicate alias \"%s\"\n",
				node->aliases[ai]);
			failures++;
			break;
		    }
		}
	    }
	}
	failures += cmd_tree_lint_nodes(node->subcommands, msgs, depth + 1);
    }
    return failures;
}


int
bu_cmd_tree_lint(const struct bu_cmd_tree *tree, struct bu_vls *msgs)
{
    int failures;

    if (!tree || !tree->root_schema || !tree->root_schema->name ||
	!tree->root_schema->name[0]) {
	if (msgs)
	    bu_vls_strcat(msgs, "native command tree has no named root schema\n");
	return 1;
    }
    if (tree->child_phase != BU_CMD_TREE_CHILD_AFTER_OPTIONS &&
	tree->child_phase != BU_CMD_TREE_CHILD_FIRST &&
	tree->child_phase != BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS) {
	if (msgs)
	    bu_vls_strcat(msgs, "native command tree has an invalid root child phase\n");
	return 1;
    }
    if (tree->child_phase == BU_CMD_TREE_CHILD_AFTER_FIXED_OPERANDS) {
	size_t count = 0;
	if (!tree->root_schema->operands ||
	    tree->root_schema->parse_policy == BU_CMD_PARSE_INTERSPERSED) {
	    if (msgs)
		bu_vls_strcat(msgs, "fixed-operand child phase requires a non-interspersed root operand prefix\n");
	    return 1;
	}
	for (size_t oi = 0; tree->root_schema->operands[oi].name; oi++) {
	    const struct bu_cmd_operand *operand = &tree->root_schema->operands[oi];
	    if (operand->min_count != operand->max_count ||
		operand->max_count == BU_CMD_COUNT_UNLIMITED ||
		(operand->shape && (operand->shape->min_tokens != 1 ||
		    operand->shape->max_tokens != 1 || operand->shape->token_count))) {
		if (msgs)
		    bu_vls_strcat(msgs, "fixed-operand child phase requires fixed scalar root operands\n");
		return 1;
	    }
	    count += operand->min_count;
	}
	if (!count) {
	    if (msgs)
		bu_vls_strcat(msgs, "fixed-operand child phase requires at least one root operand\n");
	    return 1;
	}
    }
    failures = bu_cmd_schema_lint(tree->root_schema, msgs);
    failures += cmd_tree_lint_nodes(tree->subcommands, msgs, 0);
    return failures;
}


static int
cmd_form_valid(const struct bu_cmd_form *form)
{
    return form && form->name && form->name[0] &&
	((form->schema && !form->tree) || (form->tree && !form->schema));
}


static int
cmd_form_member(const struct bu_cmd_forms *forms,
	const struct bu_cmd_form *selected)
{
    if (!forms || !forms->forms || !selected)
	return 0;
    for (size_t i = 0; forms->forms[i].name; i++)
	if (&forms->forms[i] == selected)
	    return cmd_form_valid(selected);
    return 0;
}


static int
cmd_form_validate(const struct bu_cmd_form *form, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    if (!cmd_form_valid(form) || !result || cursor_arg > argc)
	return -1;
    if (form->tree)
	return bu_cmd_tree_validate_argv_ctx(form->tree, argc, argv, cursor_arg,
	    context, result);
    return bu_cmd_schema_validate_ctx(form->schema, argc, argv, cursor_arg,
	context, result);
}


static int
cmd_form_state_rank(bu_cmd_validate_state_t state)
{
    switch (state) {
	case BU_CMD_VALIDATE_VALID: return 3;
	case BU_CMD_VALIDATE_INCOMPLETE: return 2;
	case BU_CMD_VALIDATE_INVALID: return 1;
	case BU_CMD_VALIDATE_UNKNOWN: return 0;
    }
    return 0;
}


static const struct bu_cmd_form *
cmd_forms_auto_select(const struct bu_cmd_forms *forms, size_t argc,
	const char **argv, size_t cursor_arg, void *context)
{
    const struct bu_cmd_form *best = NULL;
    int best_rank = -1;
    int ambiguous = 0;

    if (!forms || !forms->forms || cursor_arg > argc)
	return NULL;
    for (size_t i = 0; forms->forms[i].name; i++) {
	struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
	int rank;
	if (!cmd_form_valid(&forms->forms[i]) ||
		cmd_form_validate(&forms->forms[i], argc, argv, cursor_arg,
		    context, &result) != 0) {
	    bu_cmd_validate_result_clear(&result);
	    continue;
	}
	rank = cmd_form_state_rank(result.state);
	bu_cmd_validate_result_clear(&result);
	if (rank > best_rank) {
	    best = &forms->forms[i];
	    best_rank = rank;
	    ambiguous = 0;
	} else if (rank == best_rank) {
	    ambiguous = 1;
	}
    }
    return ambiguous ? NULL : best;
}


static void
cmd_forms_merge_candidates(struct bu_cmd_validate_result *target,
	const struct bu_cmd_validate_result *source)
{
    if (!target || !source || !source->completion_candidates)
	return;
    for (size_t si = 0; si < source->completion_count; si++) {
	int duplicate = 0;
	for (size_t ti = 0; ti < target->completion_count; ti++) {
	    if (BU_STR_EQUAL(target->completion_candidates[ti],
		    source->completion_candidates[si])) {
		duplicate = 1;
		break;
	    }
	}
	if (duplicate)
	    continue;
	target->completion_candidates = (const char **)bu_realloc(
	    (void *)target->completion_candidates,
	    (target->completion_count + 2) * sizeof(const char *),
	    "merged command form candidates");
	target->completion_candidates[target->completion_count++] =
	    bu_strdup(source->completion_candidates[si]);
	target->completion_candidates[target->completion_count] = NULL;
    }
}


static int
cmd_forms_validate_alternatives(const struct bu_cmd_forms *forms, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    int best_rank = -1;
    size_t best_count = 0;
    size_t valid_count = 0;
    size_t incomplete_count = 0;

    bu_cmd_validate_result_clear(result);
    for (size_t i = 0; forms->forms[i].name; i++) {
	struct bu_cmd_validate_result candidate = BU_CMD_VALIDATE_RESULT_NULL;
	int rank;

	if (!cmd_form_valid(&forms->forms[i]) ||
		cmd_form_validate(&forms->forms[i], argc, argv, cursor_arg,
		    context, &candidate) != 0) {
	    bu_cmd_validate_result_clear(&candidate);
	    continue;
	}
	rank = cmd_form_state_rank(candidate.state);
	if (candidate.state == BU_CMD_VALIDATE_VALID)
	    valid_count++;
	else if (candidate.state == BU_CMD_VALIDATE_INCOMPLETE)
	    incomplete_count++;
	/* At an appendable cursor, a valid short form and an incomplete longer
	 * form are both useful.  Execution still selects only complete forms. */
	if (cursor_arg == argc && candidate.state == BU_CMD_VALIDATE_INCOMPLETE)
	    rank = cmd_form_state_rank(BU_CMD_VALIDATE_VALID);
	if (rank > best_rank) {
	    bu_cmd_validate_result_clear(result);
	    *result = candidate;
	    candidate = (struct bu_cmd_validate_result)BU_CMD_VALIDATE_RESULT_NULL;
	    best_rank = rank;
	    best_count = 1;
	} else if (rank == best_rank) {
	    best_count++;
	    result->expected |= candidate.expected;
	    if (candidate.token_start < result->token_start)
		result->token_start = candidate.token_start;
	    if (candidate.token_end > result->token_end)
		result->token_end = candidate.token_end;
	    if (!result->completion_count && candidate.completion_count) {
		cmd_schema_clear_completion_candidates(result);
		result->completion_type = candidate.completion_type;
		result->semantic_provider = candidate.semantic_provider;
		result->candidate_validate = candidate.candidate_validate;
		cmd_forms_merge_candidates(result, &candidate);
	    } else if (!candidate.completion_count && !candidate.semantic_provider) {
		/* A complete shorter form has nothing to add, but must not erase
		 * continuation metadata supplied by a longer viable form. */
		if (result->completion_type != candidate.completion_type)
		    result->completion_type = BU_CMD_VALUE_UNKNOWN;
	    } else if (result->completion_type == candidate.completion_type) {
		cmd_forms_merge_candidates(result, &candidate);
	    } else {
		cmd_schema_clear_completion_candidates(result);
		result->completion_type = BU_CMD_VALUE_UNKNOWN;
		result->semantic_provider = NULL;
		result->candidate_validate = NULL;
	    }
	    if ((!result->semantic_provider != !candidate.semantic_provider) ||
		(result->semantic_provider &&
		 !BU_STR_EQUAL(result->semantic_provider,
		     candidate.semantic_provider)))
		result->semantic_provider = NULL;
	    if (result->candidate_validate != candidate.candidate_validate)
		result->candidate_validate = NULL;
	}
	bu_cmd_validate_result_clear(&candidate);
    }
    if (best_rank < 0)
	return -1;
    if (valid_count > 1) {
	bu_cmd_validate_result_clear(result);
	result->state = BU_CMD_VALIDATE_INVALID;
	result->token_start = cursor_arg;
	result->token_end = cursor_arg;
	result->hint = "ambiguous command form";
    } else if (valid_count == 1) {
	result->state = BU_CMD_VALIDATE_VALID;
	if (incomplete_count)
	    result->hint = "valid command with additional form alternatives";
    } else if (best_count > 1 && result->state == BU_CMD_VALIDATE_INCOMPLETE) {
	result->hint = "command form alternatives";
    }
    return 0;
}


const struct bu_cmd_form *
bu_cmd_forms_select(const struct bu_cmd_forms *forms, size_t argc,
	const char * const *argv, void *context)
{
    const struct bu_cmd_form *selected = NULL;

    if (!forms || !forms->forms || (argc && !argv))
	return NULL;
    if (forms->select)
	selected = forms->select(forms, argc, argv, context);
    if (selected)
	return cmd_form_member(forms, selected) ? selected : NULL;

    /* Form schemas describe arguments after the command word. */
    return cmd_forms_auto_select(forms, argc ? argc - 1 : 0,
	argc ? (const char **)(argv + 1) : NULL, argc ? argc - 1 : 0, context);
}


int
bu_cmd_forms_validate(const struct bu_cmd_forms *forms, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    const struct bu_cmd_form *selected = NULL;
    size_t form_argc;
    size_t form_cursor;

    if (!result)
	return -1;
    bu_cmd_validate_result_clear(result);
    if (!forms || !forms->forms || cursor_arg > argc || (argc && !argv))
	return -1;
    if (cursor_arg == 0) {
	result->state = BU_CMD_VALIDATE_VALID;
	result->hint = "command";
	return 0;
    }
    if (forms->select)
	selected = forms->select(forms, argc,
	    (const char * const *)argv, context);
    if (selected && !cmd_form_member(forms, selected))
	return -1;
    form_argc = argc ? argc - 1 : 0;
    form_cursor = cursor_arg - 1;
    if (!selected) {
	if (cmd_forms_validate_alternatives(forms, form_argc,
		argc ? argv + 1 : NULL, form_cursor, context, result) != 0) {
	    bu_cmd_validate_result_clear(result);
	    return -1;
	}
	result->token_start++;
	result->token_end++;
	return 0;
    }
    if (cmd_form_validate(selected, form_argc, argc ? argv + 1 : NULL,
	    form_cursor, context, result) != 0) {
	bu_cmd_validate_result_clear(result);
	return -1;
	}
    result->token_start++;
    result->token_end++;
    return 0;
}


int
bu_cmd_forms_parse(const struct bu_cmd_forms *forms, void *data,
	struct bu_vls *msg, int argc, const char *argv[], void *context,
	const struct bu_cmd_form **selected)
{
    const struct bu_cmd_form *form;
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
    int ret;

    if (selected)
	*selected = NULL;
    if (!forms || argc < 1 || !argv)
	return -1;
    form = bu_cmd_forms_select(forms, (size_t)argc,
	(const char * const *)argv, context);
    if (!form || !form->schema)
	return -1;
    if (bu_cmd_schema_validate_ctx(form->schema, (size_t)argc - 1, argv + 1,
	    (size_t)argc - 1, context, &validation) != 0 ||
	validation.state != BU_CMD_VALIDATE_VALID) {
	if (msg && validation.hint)
	    bu_vls_printf(msg, "%s\n", validation.hint);
	bu_cmd_validate_result_clear(&validation);
	return -1;
    }
    bu_cmd_validate_result_clear(&validation);
    ret = bu_cmd_schema_parse(form->schema, data, msg, argc - 1, argv + 1);
    if (ret >= 0 && selected)
	*selected = form;
    return ret;
}


int
bu_cmd_forms_dispatch(const struct bu_cmd_forms *forms, void *context,
	int argc, const char *argv[], int *result,
	const struct bu_cmd_form **selected)
{
    const struct bu_cmd_form *form;
    int ret;

    if (selected)
	*selected = NULL;
    if (!forms || argc < 1 || !argv)
	return -1;
    form = bu_cmd_forms_select(forms, (size_t)argc,
	(const char * const *)argv, context);
    if (!form || !form->tree)
	return -1;
    ret = bu_cmd_tree_dispatch(form->tree, context, argc - 1, argv + 1,
	result);
    if (!ret && selected)
	*selected = form;
    return ret;
}


char *
bu_cmd_forms_help(const struct bu_cmd_forms *forms, const char *invocation)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    size_t count = 0;

    if (!forms || BU_STR_EMPTY(forms->name) || !forms->forms)
	return NULL;
    if (BU_STR_EMPTY(invocation))
	invocation = forms->name;
    while (forms->forms[count].name)
	count++;
    if (!count)
	return NULL;
    if (count == 1) {
	return forms->forms[0].tree ?
	    bu_cmd_tree_help(forms->forms[0].tree, invocation) :
	    bu_cmd_schema_help(forms->forms[0].schema, invocation);
    }
    if (!BU_STR_EMPTY(forms->help))
	bu_vls_printf(&out, "%s\n", forms->help);
    for (size_t i = 0; i < count; i++) {
	char *help = forms->forms[i].tree ?
	    bu_cmd_tree_help(forms->forms[i].tree, invocation) :
	    bu_cmd_schema_help(forms->forms[i].schema, invocation);
	if (i || bu_vls_strlen(&out))
	    bu_vls_putc(&out, '\n');
	bu_vls_printf(&out, "%s form", forms->forms[i].name);
	if (!BU_STR_EMPTY(forms->forms[i].help))
	    bu_vls_printf(&out, " - %s", forms->forms[i].help);
	bu_vls_strcat(&out, ":\n");
	if (help)
	    bu_vls_strcat(&out, help);
	if (help)
	    bu_free(help, "command form help");
    }
    char *result = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return result;
}


int
bu_cmd_forms_help_append(struct bu_vls *output,
	const struct bu_cmd_forms *forms, const char *invocation)
{
    char *help;

    if (!output)
	return -1;
    help = bu_cmd_forms_help(forms, invocation);
    if (!help)
	return -1;
    bu_vls_strcat(output, help);
    bu_free(help, "command forms help");
    return 0;
}


char *
bu_cmd_forms_describe_json(const struct bu_cmd_forms *forms)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;

    if (!forms || BU_STR_EMPTY(forms->name) || !forms->forms)
	return NULL;
    bu_vls_strcat(&out, "{\"kind\":\"native_forms\",\"name\":");
    bu_cmd_json_string(&out, forms->name);
    bu_vls_strcat(&out, ",\"help\":");
    bu_cmd_json_string(&out, forms->help);
    bu_vls_strcat(&out, ",\"forms\":[");
    for (size_t i = 0; forms->forms[i].name; i++) {
	char *grammar = forms->forms[i].tree ?
	    bu_cmd_tree_describe_json(forms->forms[i].tree) :
	    bu_cmd_schema_describe_json(forms->forms[i].schema);
	if (i)
	    bu_vls_putc(&out, ',');
	bu_vls_strcat(&out, "{\"name\":");
	bu_cmd_json_string(&out, forms->forms[i].name);
	bu_vls_strcat(&out, ",\"help\":");
	bu_cmd_json_string(&out, forms->forms[i].help);
	bu_vls_strcat(&out, ",\"grammar\":");
	bu_vls_strcat(&out, grammar ? grammar : "null");
	bu_vls_putc(&out, '}');
	if (grammar)
	    bu_free(grammar, "command form JSON");
    }
    bu_vls_strcat(&out, "]}");
    char *result = bu_vls_strdup(&out);
    bu_vls_free(&out);
    return result;
}


int
bu_cmd_forms_lint(const struct bu_cmd_forms *forms, struct bu_vls *msgs)
{
    int failures = 0;

    if (!forms || BU_STR_EMPTY(forms->name) || !forms->forms) {
	if (msgs)
	    bu_vls_strcat(msgs, "native command form set is incomplete\n");
	return 1;
    }
    if (!forms->forms[0].name) {
	if (msgs)
	    bu_vls_printf(msgs, "%s: command form set is empty\n", forms->name);
	return 1;
    }
    for (size_t i = 0; forms->forms[i].name; i++) {
	const struct bu_cmd_form *form = &forms->forms[i];
	if (!cmd_form_valid(form)) {
	    if (msgs)
		bu_vls_printf(msgs, "%s: malformed command form \"%s\"\n",
		    forms->name, form->name ? form->name : "");
	    failures++;
	    continue;
	}
	for (size_t j = 0; j < i; j++) {
	    if (BU_STR_EQUAL(forms->forms[j].name, form->name)) {
		if (msgs)
		    bu_vls_printf(msgs, "%s: duplicate command form \"%s\"\n",
			forms->name, form->name);
		failures++;
		break;
	    }
	}
	failures += form->tree ? bu_cmd_tree_lint(form->tree, msgs) :
	    bu_cmd_schema_lint(form->schema, msgs);
    }
    return failures;
}
