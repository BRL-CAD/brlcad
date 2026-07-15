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


static const char *
cmd_schema_keyword_canonical(const char * const *keywords,
	const struct bu_cmd_value_keyword *keyword_values, const char *arg);
static const struct bu_cmd_operand *cmd_schema_operand_at(
	const struct bu_cmd_schema *schema, size_t operand_index);
static int cmd_schema_operand_valid(const struct bu_cmd_operand *operand,
	const char *arg);


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


const struct bu_cmd_arg_shape bu_cmd_rgb_arg_shape = {
    BU_CMD_ARG_SHAPE_RGB, 1, 3,
    "packed r/g/b, r,g,b, or r;g;b; or three RGB channels", NULL
};

const struct bu_cmd_arg_shape bu_cmd_color_arg_shape = {
    BU_CMD_ARG_SHAPE_COLOR, 1, 3,
    "packed color or three RGB components", NULL
};

const struct bu_cmd_arg_shape bu_cmd_vector3_arg_shape = {
    BU_CMD_ARG_SHAPE_VECTOR3, 1, 3,
    "packed x/y/z, x,y,z, or x;y;z; quoted x y z; or three numeric components", NULL
};

const struct bu_cmd_arg_shape bu_cmd_counted_vector3_arg_shape = {
    BU_CMD_ARG_SHAPE_TOKEN_SEQUENCE, 2, 4,
    "integer count plus packed or three-component XYZ vector",
    cmd_schema_counted_vector3_token_count
};

const struct bu_cmd_arg_shape bu_cmd_optional_scalar_arg_shape = {
    BU_CMD_ARG_SHAPE_CUSTOM, 0, 1,
    "one optional scalar token, excluding the next option",
    cmd_schema_optional_scalar_token_count
};


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

    if (!result || cursor_arg > argc)
	return -1;
    bu_cmd_validate_result_clear(result);
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

    if (!result || cursor_arg > argc)
	return -1;
    bu_cmd_validate_result_clear(result);
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

    if (!result || cursor_arg > argc)
	return -1;
    bu_cmd_validate_result_clear(result);
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

    if (!result || cursor_arg > argc)
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

    if (!result || cursor_arg > argc)
	return -1;
    bu_cmd_validate_result_clear(result);
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
    parsed = bu_units_conversion(arg);
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
	"xh", "yi", "yo", "zu", NULL
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

    while (schema->options[i].canonical) {
	const struct bu_cmd_option *option = &schema->options[i];
	if (!option->alias_of && BU_STR_EQUAL(option->canonical, canonical))
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

    while (schema->options[i].canonical) {
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
    return option->longopt ? option->longopt :
	(option->canonical ? option->canonical : "option");
}


static int
cmd_schema_has_options(const struct bu_cmd_schema *schema)
{
    return schema && schema->options && schema->options[0].canonical;
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

    if (option->storage_offset == BU_CMD_STORAGE_NONE) {
	if (msg)
	    bu_vls_printf(msg, "--%s is syntax-only and has no execution storage binding\n",
		option->longopt ? option->longopt : option->canonical);
	return -1;
    }
	storage = (char *)data + option->storage_offset;

    if (option->value_type == BU_CMD_VALUE_KEYWORD &&
	!cmd_schema_keyword_canonical(option->value_keywords, option->keyword_values, arg)) {
	if (msg)
	    bu_vls_printf(msg, "invalid keyword for --%s: %s\n",
		option->longopt ? option->longopt : option->canonical, arg);
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
		cmd_schema_keyword_canonical(option->value_keywords, option->keyword_values, arg) : arg;
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
    if (!option || option->arg_requirement == BU_CMD_ARG_NONE)
	return 0;
    if (option->arg_shape)
	return option->arg_shape->min_tokens;
    return option->arg_requirement == BU_CMD_ARG_OPTIONAL ? 0 : 1;
}


static size_t
cmd_schema_option_max_tokens(const struct bu_cmd_option *option)
{
    if (!option || option->arg_requirement == BU_CMD_ARG_NONE)
	return 0;
    if (option->arg_shape)
	return option->arg_shape->max_tokens;
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

    if (max_tokens != BU_CMD_COUNT_UNLIMITED && count > max_tokens)
	count = max_tokens;
    if (option && option->arg_shape && option->arg_shape->token_count) {
	size_t selected = option->arg_shape->token_count(count, argv);
	return selected <= count ? selected : 0;
    }
    if (option && option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_RGB) {
	unsigned char rgb[3] = {0, 0, 0};
	return (size_t)bu_rgb_from_argv(rgb, count, (const char * const *)argv);
    }
    if (option && option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_COLOR) {
	struct bu_color color = BU_COLOR_INIT_ZERO;
	return (size_t)bu_cmd_color_from_argv(&color, count, (const char * const *)argv);
    }
    if (option && option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_VECTOR3) {
	fastf_t xyz[3] = {0.0, 0.0, 0.0};
	return (size_t)bu_cmd_vector3_from_argv(xyz, count, (const char * const *)argv);
    }
    return count;
}


static int
cmd_schema_rgb_partial(const struct bu_cmd_option *option, size_t available,
	const char **argv)
{
    if (!option || !option->arg_shape || option->arg_shape->kind != BU_CMD_ARG_SHAPE_RGB ||
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
    if (!option || !option->arg_shape || option->arg_shape->kind != BU_CMD_ARG_SHAPE_COLOR ||
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
    if (!option || !option->arg_shape || option->arg_shape->kind != BU_CMD_ARG_SHAPE_VECTOR3 ||
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
    return cmd_schema_option_max_tokens(option) == 1 ||
	(option && option->arg_shape && (option->arg_shape->kind == BU_CMD_ARG_SHAPE_RGB ||
	    option->arg_shape->kind == BU_CMD_ARG_SHAPE_COLOR ||
	    option->arg_shape->kind == BU_CMD_ARG_SHAPE_VECTOR3));
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
		option->longopt ? option->longopt : option->canonical);
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
		option->longopt ? option->longopt : option->canonical, (unsigned long)argc);
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
	    int value = bu_str_true(arg);
	    valid = value == 0 || value == 1;
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

    /* A string-valued role may publish keyword values as canonical completion
     * hints while a command validator accepts a wider expression grammar (for
     * example, units accepts both cm and 25cm).  Only a keyword-typed value is
     * constrained to its static value set. */
    return valid && (option->value_type != BU_CMD_VALUE_KEYWORD ||
	cmd_schema_keyword_canonical(option->value_keywords, option->keyword_values, arg)) &&
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
    operand = cmd_schema_operand_at(schema, operand_index);
    if (!operand)
	return 0;
    switch (operand->value_type) {
	case BU_CMD_VALUE_INTEGER:
	case BU_CMD_VALUE_HEX_INTEGER:
	case BU_CMD_VALUE_LONG:
	case BU_CMD_VALUE_HEX_LONG:
	case BU_CMD_VALUE_NUMBER:
	    return cmd_schema_operand_valid(operand, arg);
	default:
	    break;
    }
    return 0;
}


int
bu_cmd_schema_parse(const struct bu_cmd_schema *schema, void *data, struct bu_vls *msg, int argc, const char *argv[])
{
    int i = 0;
    int end_options = 0;
    int ret = -1;
    int interspersed = 0;
    size_t known_count = 0;
    size_t operand_count = 0;
    const char **known_args = NULL;
    const char **operand_args = NULL;

    if (!schema || argc < 0 || (argc > 0 && !argv))
	return -1;

    /* With no declared options, every word is an operand.  This matters for
     * raw-text commands such as echo: a literal leading '-' is not an option
     * merely because it resembles one. */
    if (!cmd_schema_has_options(schema)) {
	for (i = 0; i < argc; i++) {
	    if (BU_STR_EQUAL(argv[i], "--"))
		return i + 1;
	}
	return 0;
    }

    if (!data)
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
	const struct bu_cmd_option *option = NULL;
	int longopt = 0;

	if (!arg) {
	    if (msg)
		bu_vls_printf(msg, "null command argument\n");
	    goto done;
	}
	if (!end_options && BU_STR_EQUAL(arg, "--")) {
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
	for (size_t ai = 0; ai < known_count; ai++)
	    argv[ai] = known_args[ai];
	for (size_t ai = 0; ai < operand_count; ai++)
	    argv[known_count + ai] = operand_args[ai];
	ret = (int)known_count;
    }
    if (known_args)
	bu_free((void *)known_args, "interspersed command option arguments");
    if (operand_args)
	bu_free((void *)operand_args, "interspersed command operands");
    return ret;
}


int
bu_cmd_schema_parse_complete(const struct bu_cmd_schema *schema, void *data,
	struct bu_vls *msg, int argc, const char *argv[])
{
    struct bu_cmd_validate_result result = BU_CMD_VALIDATE_RESULT_NULL;
    int operand_index = bu_cmd_schema_parse(schema, data, msg, argc, argv);

    if (operand_index < 0)
	return -1;
    if (bu_cmd_schema_validate(schema, (size_t)argc, argv, (size_t)argc, &result) != 0 ||
	result.state != BU_CMD_VALIDATE_VALID) {
	if (msg && result.hint)
	    bu_vls_printf(msg, "%s\n", result.hint);
	bu_cmd_validate_result_clear(&result);
	return -1;
    }
    bu_cmd_validate_result_clear(&result);
    return operand_index;
}


char *
bu_cmd_schema_describe_selected(const struct bu_cmd_schema *schema, const char * const *selected)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    size_t i = 0;

    if (!schema || !schema->options)
	return NULL;

    while (schema->options[i].canonical) {
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
		if (BU_STR_EQUAL(option->canonical, selected[selected_i])) {
		    found = 1;
		    break;
		}
	    }
	    if (!found) {
		i++;
		continue;
	    }
	}
	if (option->shortopt && option->shortopt[0])
	    bu_vls_printf(&spellings, "-%s", option->shortopt);
	if (option->longopt && option->longopt[0]) {
	    if (bu_vls_strlen(&spellings) > 0)
		bu_vls_printf(&spellings, ", ");
	    bu_vls_printf(&spellings, "--%s", option->longopt);
	}
	if (option->arg_requirement != BU_CMD_ARG_NONE && option->argument && option->argument[0])
	    bu_vls_printf(&spellings, " %s", option->argument);
	bu_vls_printf(&out, "  %-34s %s\n", bu_vls_addr(&spellings),
		option->help ? option->help : "");
	bu_vls_free(&spellings);
	i++;
    }

    if (schema->validation.constraint_data.constraints) {
	for (i = 0; schema->validation.constraint_data.constraints[i].options; i++) {
	    const char *hint = schema->validation.constraint_data.constraints[i].hint;
	    if (hint && hint[0])
		bu_vls_printf(&out, "  %-34s %s\n", "Constraint:", hint);
	}
    }

    char *result = bu_strdup(bu_vls_addr(&out));
    bu_vls_free(&out);
    return result;
}


char *
bu_cmd_schema_describe(const struct bu_cmd_schema *schema)
{
    return bu_cmd_schema_describe_selected(schema, NULL);
}


static void
cmd_schema_json_string(struct bu_vls *out, const char *value)
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
	    cmd_schema_json_string(out, keyword_values[i].canonical);
	}
    } else if (keywords) {
	for (i = 0; keywords[i]; i++) {
	    if (i)
		bu_vls_putc(out, ',');
	    cmd_schema_json_string(out, keywords[i]);
	}
    }
    bu_vls_strcat(out, "],\"keyword_values\":[");
    if (keyword_values) {
	for (i = 0; keyword_values[i].canonical; i++) {
	    const struct bu_cmd_value_keyword *keyword = &keyword_values[i];
	    if (i)
		bu_vls_putc(out, ',');
	    bu_vls_strcat(out, "{\"canonical\":");
	    cmd_schema_json_string(out, keyword->canonical);
	    bu_vls_strcat(out, ",\"aliases\":[");
	    if (keyword->aliases) {
		for (size_t ai = 0; keyword->aliases[ai]; ai++) {
		    if (ai)
			bu_vls_putc(out, ',');
		    cmd_schema_json_string(out, keyword->aliases[ai]);
		}
	    }
	    bu_vls_strcat(out, "],\"description\":");
	    cmd_schema_json_string(out, keyword->description);
	    bu_vls_putc(out, '}');
	}
    }
    bu_vls_putc(out, ']');
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
	case BU_CMD_CONSTRAINT_OPERAND_COUNT: return "operand_count";
	default: break;
    }
    return "unknown";
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
    cmd_schema_json_string(&out, schema->name);
    bu_vls_strcat(&out, ",\"help\":");
    cmd_schema_json_string(&out, schema->help);
    bu_vls_strcat(&out, ",\"parse_policy\":");
    cmd_schema_json_string(&out, cmd_schema_policy_name(schema->parse_policy));
    bu_vls_strcat(&out, ",\"options\":[");
    if (schema->options) {
	while (schema->options[i].canonical) {
	    const struct bu_cmd_option *option = &schema->options[i];
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"short\":");
	    cmd_schema_json_string(&out, option->shortopt);
	    bu_vls_strcat(&out, ",\"long\":");
	    cmd_schema_json_string(&out, option->longopt);
	    bu_vls_strcat(&out, ",\"canonical\":");
	    cmd_schema_json_string(&out, option->canonical);
	    bu_vls_strcat(&out, ",\"alias_of\":");
	    cmd_schema_json_string(&out, option->alias_of);
	    bu_vls_strcat(&out, ",\"argument\":");
	    cmd_schema_json_string(&out, option->argument);
	    bu_vls_strcat(&out, ",\"argument_requirement\":");
	    cmd_schema_json_string(&out, cmd_schema_arg_requirement_name(option->arg_requirement));
	    bu_vls_strcat(&out, ",\"argument_shape\":");
	    if (option->arg_shape) {
		bu_vls_strcat(&out, "{\"kind\":");
		cmd_schema_json_string(&out, cmd_schema_arg_shape_name(option->arg_shape->kind));
		bu_vls_printf(&out, ",\"min_tokens\":%lu,\"max_tokens\":",
		    (unsigned long)option->arg_shape->min_tokens);
		if (option->arg_shape->max_tokens == BU_CMD_COUNT_UNLIMITED)
		    bu_vls_strcat(&out, "null");
		else
		    bu_vls_printf(&out, "%lu", (unsigned long)option->arg_shape->max_tokens);
		bu_vls_strcat(&out, ",\"description\":");
		cmd_schema_json_string(&out, option->arg_shape->description);
		bu_vls_putc(&out, '}');
	    } else {
		bu_vls_strcat(&out, "null");
	    }
	    bu_vls_strcat(&out, ",\"type\":");
	    cmd_schema_json_string(&out, cmd_schema_value_name(option->value_type));
	    bu_vls_strcat(&out, ",\"semantic_provider\":");
	    cmd_schema_json_string(&out, option->semantic_provider);
	    bu_vls_strcat(&out, ",");
	    cmd_schema_json_keyword_values(&out, option->value_keywords, option->keyword_values);
	    bu_vls_printf(&out, ",\"repeat\":%s,\"hidden\":%s,\"help\":",
		option->repeat ? "true" : "false", option->hidden ? "true" : "false");
	    cmd_schema_json_string(&out, option->help);
	    bu_vls_putc(&out, '}');
	    comma = 1;
	    i++;
	}
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
	    cmd_schema_json_string(&out, operand->name);
	    bu_vls_printf(&out, ",\"min\":%lu,\"max\":", (unsigned long)operand->min_count);
	    if (operand->max_count == BU_CMD_COUNT_UNLIMITED)
		bu_vls_strcat(&out, "null");
	    else
		bu_vls_printf(&out, "%lu", (unsigned long)operand->max_count);
	    bu_vls_strcat(&out, ",\"help\":");
	    cmd_schema_json_string(&out, operand->help);
	    bu_vls_strcat(&out, ",\"type\":");
	    cmd_schema_json_string(&out, cmd_schema_value_name(operand->value_type));
	    bu_vls_strcat(&out, ",\"shape\":");
	    if (operand->shape) {
		bu_vls_strcat(&out, "{\"kind\":");
		cmd_schema_json_string(&out, cmd_schema_arg_shape_name(operand->shape->kind));
		bu_vls_printf(&out, ",\"min_tokens\":%lu,\"max_tokens\":",
		    (unsigned long)operand->shape->min_tokens);
		if (operand->shape->max_tokens == BU_CMD_COUNT_UNLIMITED)
		    bu_vls_strcat(&out, "null");
		else
		    bu_vls_printf(&out, "%lu", (unsigned long)operand->shape->max_tokens);
		bu_vls_strcat(&out, ",\"description\":");
		cmd_schema_json_string(&out, operand->shape->description);
		bu_vls_putc(&out, '}');
	    } else {
		bu_vls_strcat(&out, "null");
	    }
	    bu_vls_strcat(&out, ",\"semantic_provider\":");
	    cmd_schema_json_string(&out, operand->semantic_provider);
	    bu_vls_strcat(&out, ",");
	    cmd_schema_json_keyword_values(&out, operand->value_keywords, operand->keyword_values);
	    bu_vls_putc(&out, '}');
	    comma = 1;
	    i++;
	}
    }
    bu_vls_strcat(&out, "],\"constraints\":[");
    comma = 0;
    if (schema->validation.constraint_data.constraints) {
	for (i = 0; schema->validation.constraint_data.constraints[i].options; i++) {
	    const struct bu_cmd_constraint *constraint = &schema->validation.constraint_data.constraints[i];
	    if (comma)
		bu_vls_putc(&out, ',');
	    bu_vls_strcat(&out, "{\"kind\":");
	    cmd_schema_json_string(&out, cmd_schema_constraint_kind_name(constraint->kind));
	    bu_vls_strcat(&out, ",\"condition\":");
	    cmd_schema_json_string(&out, cmd_schema_constraint_condition_name(constraint->condition));
	    bu_vls_strcat(&out, ",\"options\":[");
	    for (size_t oi = 0; constraint->options[oi]; oi++) {
		if (oi)
		    bu_vls_putc(&out, ',');
		cmd_schema_json_string(&out, constraint->options[oi]);
	    }
	    bu_vls_printf(&out, "],\"min\":%lu,\"max\":", (unsigned long)constraint->min_count);
	    if (constraint->max_count == BU_CMD_COUNT_UNLIMITED)
		bu_vls_strcat(&out, "null");
	    else
		bu_vls_printf(&out, "%lu", (unsigned long)constraint->max_count);
	    bu_vls_strcat(&out, ",\"hint\":");
	    cmd_schema_json_string(&out, constraint->hint);
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
bu_cmd_validate_result_clear(struct bu_cmd_validate_result *result)
{
    if (!result)
	return;
    cmd_schema_clear_completion_candidates(result);
    *result = (struct bu_cmd_validate_result)BU_CMD_VALIDATE_RESULT_NULL;
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
	if (!prefix_len || strncmp(values[i], prefix, prefix_len) == 0)
	    count++;
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "command schema keyword candidates");
    for (size_t i = 0; values[i]; i++)
	if (!prefix_len || strncmp(values[i], prefix, prefix_len) == 0)
	    result->completion_candidates[result->completion_count++] = bu_strdup(values[i]);
}


static void
cmd_schema_set_result(struct bu_cmd_validate_result *result,
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
    return !prefix || !prefix[0] || strncmp(candidate, prefix, strlen(prefix)) == 0;
}


static void
cmd_schema_add_option_candidates(const struct bu_cmd_schema *schema,
	struct bu_cmd_validate_result *result, const char *prefix)
{
    size_t count = 0;
    size_t i = 0;

    if (!schema || !schema->options)
	return;
    while (schema->options[i].canonical) {
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
    while (schema->options[i].canonical) {
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


static const struct bu_cmd_operand *
cmd_schema_operand_at(const struct bu_cmd_schema *schema, size_t operand_index)
{
    size_t index = 0;

    if (!schema || !schema->operands)
	return NULL;
    for (size_t i = 0; schema->operands[i].name; i++) {
	const struct bu_cmd_operand *operand = &schema->operands[i];
	if (operand->max_count == BU_CMD_COUNT_UNLIMITED ||
	    (operand_index >= index && operand_index - index < operand->max_count))
	    return operand;
	index += operand->max_count;
    }
    return NULL;
}


static size_t
cmd_schema_minimum_operands(const struct bu_cmd_schema *schema)
{
    size_t minimum = 0;

    if (!schema || !schema->operands)
	return 0;
    for (size_t i = 0; schema->operands[i].name; i++)
	minimum += schema->operands[i].min_count;
    return minimum;
}


int
bu_cmd_schema_option_present(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, const char *canonical)
{
    size_t i = 0;
    int options_allowed = cmd_schema_has_options(schema);

    if (!schema || !canonical)
	return 0;
    while (i < argc) {
	const char *arg = argv[i];
	const struct bu_cmd_option *option = NULL;
	const char *eq = NULL;
	if (!arg)
	    return 0;
	if (BU_STR_EQUAL(arg, "--")) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && arg[0] == '-' && arg[1]) {
	    option = cmd_schema_lookup_token(schema, arg);
	    eq = strchr(arg, '=');
	    if (option) {
		if (BU_STR_EQUAL(option->canonical, canonical))
		    return 1;
		if (option->arg_requirement == BU_CMD_ARG_NONE || eq) {
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
		    if (option && BU_STR_EQUAL(option->canonical, canonical))
			return 1;
		}
		i++;
		continue;
	    }
	}
	if (schema->parse_policy != BU_CMD_PARSE_INTERSPERSED)
	    options_allowed = 0;
	i++;
    }

    return 0;
}


size_t
bu_cmd_schema_operand_count(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv)
{
    size_t i = 0;
    size_t count = 0;
    int options_allowed = cmd_schema_has_options(schema);

    if (!schema)
	return 0;
    while (i < argc) {
	const char *arg = argv[i];
	const struct bu_cmd_option *option = NULL;
	const char *eq = NULL;
	if (!arg)
	    return count;
	if (BU_STR_EQUAL(arg, "--")) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && arg[0] == '-' && arg[1]) {
	    option = cmd_schema_lookup_token(schema, arg);
	    eq = strchr(arg, '=');
	    if (option) {
		if (option->arg_requirement == BU_CMD_ARG_NONE || eq) {
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


int
bu_cmd_schema_option_span(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv)
{
    const char *arg = NULL;
    const char *name = NULL;
    const char *eq = NULL;
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
    if (!option) {
	int cluster = cmd_schema_short_flag_cluster(schema, arg, NULL, NULL, 0);
	return cluster > 0 ? 1 : 0;
    }
    if (option->arg_requirement == BU_CMD_ARG_NONE)
	return eq ? -1 : 1;
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


static int
cmd_schema_constraint_applies(const struct bu_cmd_constraint *constraint,
	size_t selected_count, size_t option_count)
{
    if (!constraint)
	return 0;
    switch (constraint->condition) {
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
cmd_schema_apply_constraints(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, struct bu_cmd_validate_result *result)
{
    if (!schema || !schema->validation.constraint_data.constraints || !result)
	return 0;

    for (size_t ci = 0; schema->validation.constraint_data.constraints[ci].options; ci++) {
	const struct bu_cmd_constraint *constraint = &schema->validation.constraint_data.constraints[ci];
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
	while (constraint->options[option_count])
	    option_count++;
	selected_count = cmd_schema_selected_option_count(schema, argc, argv,
	    constraint->options);
	if (!cmd_schema_constraint_applies(constraint, selected_count, option_count))
	    continue;

	actual_count = constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT ?
	    selected_count : bu_cmd_schema_operand_count(schema, argc, argv);
	if (actual_count >= constraint->min_count && actual_count <= constraint->max_count)
	    continue;

	state = actual_count < constraint->min_count ?
	    BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_INVALID;
	if (constraint->kind == BU_CMD_CONSTRAINT_OPTION_COUNT) {
	    expected = BU_CMD_EXPECT_OPTION;
	    value_type = BU_CMD_VALUE_FLAG;
	} else {
	    expected = BU_CMD_EXPECT_OPERAND;
	    operand = cmd_schema_operand_at(schema, actual_count);
	    if (operand) {
		value_type = operand->value_type;
		provider = operand->semantic_provider;
	    }
	}
	cmd_schema_set_result(result, state, argc, expected, value_type,
	    constraint->hint ? constraint->hint : "command constraint", provider);
	return 0;
    }

    return 0;
}


static int
cmd_schema_operand_valid(const struct bu_cmd_operand *operand, const char *arg)
{
    struct bu_cmd_option option = BU_CMD_OPTION_NULL;

    if (!operand)
	return 0;
    option.value_type = operand->value_type;
    option.validate = operand->validate;
    option.value_keywords = operand->value_keywords;
    option.keyword_values = operand->keyword_values;
    return cmd_schema_value_valid(&option, arg);
}


int
bu_cmd_schema_validate(const struct bu_cmd_schema *schema, size_t argc, const char **argv,
	size_t cursor_arg, struct bu_cmd_validate_result *result)
{
    size_t i = 0;
    size_t operand_count = 0;
    int options_allowed = cmd_schema_has_options(schema);

    if (!schema || !result || cursor_arg > argc)
	return -1;

    if (schema->validation.custom_validate)
	return schema->validation.custom_validate(schema, argc, argv, cursor_arg, result);

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
	if (BU_STR_EQUAL(arg, "--")) {
	    options_allowed = 0;
	    i++;
	    continue;
	}
	if (options_allowed && arg[0] == '-' && arg[1]) {
	    const struct bu_cmd_option *option = cmd_schema_lookup_token(schema, arg);
	const char *eq = strchr(arg, '=');
	size_t min_tokens = 0;
	size_t available = 0;
	    size_t consume = 0;
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
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_OPTION,
		    BU_CMD_VALUE_FLAG, "option does not take an argument", option->semantic_provider);
		return 0;
	    }
	    if (option->arg_requirement == BU_CMD_ARG_NONE) {
		i++;
		continue;
	    }
	    min_tokens = cmd_schema_option_min_tokens(option);
	    if (eq) {
		const char *value = eq + 1;
		if (!cmd_schema_option_attached_allowed(option) ||
		    !cmd_schema_option_arguments_valid(option, 1, &value)) {
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
		cmd_schema_set_result(result, valid ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID,
		    cursor_arg, BU_CMD_EXPECT_OPTION_ARG, option->value_type,
		    valid ? "option argument" : "invalid option argument", option->semantic_provider);
		cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		    option->arg_shape, result, argv[cursor_arg]);
		return 0;
	    }

	    if (consume < min_tokens) {
		if (option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_RGB &&
		    available && !cmd_schema_rgb_partial(option, available, argv + i + 1)) {
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid RGB color",
			option->semantic_provider);
		    return 0;
		}
		if (option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_COLOR &&
		    available && !cmd_schema_color_partial(option, available, argv + i + 1)) {
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid color",
			option->semantic_provider);
		    return 0;
		}
		if (option->arg_shape && option->arg_shape->kind == BU_CMD_ARG_SHAPE_VECTOR3 &&
		    available && !cmd_schema_vector3_partial(option, available, argv + i + 1)) {
		    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1,
			BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid XYZ vector",
			option->semantic_provider);
		    return 0;
		}
		if (!option->consume) {
		    for (size_t ai = 0; ai < consume; ai++) {
			if (!cmd_schema_value_valid(option, argv[i + 1 + ai])) {
			    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i + 1 + ai,
				BU_CMD_EXPECT_OPTION_ARG, option->value_type, "invalid option argument",
				option->semantic_provider);
			    return 0;
			}
		    }
		}
		cmd_schema_set_result(result, BU_CMD_VALIDATE_INCOMPLETE, cursor_arg,
		    BU_CMD_EXPECT_OPTION_ARG, option->value_type, "option argument expected",
		    option->semantic_provider);
		cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		    option->arg_shape, result, "");
		return 0;
	    }
	    if (!cmd_schema_option_arguments_valid(option, consume, argv + i + 1)) {
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
	const struct bu_cmd_operand *operand = cmd_schema_operand_at(schema, operand_count);
	if (!operand) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, i, BU_CMD_EXPECT_NONE,
		BU_CMD_VALUE_STRING, "too many operands", NULL);
	    return 0;
	}
	if (!cmd_schema_operand_valid(operand, arg)) {
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
	BU_STR_EQUAL(argv[cursor_arg], "--")) {
	const struct bu_cmd_operand *operand = cmd_schema_operand_at(schema, operand_count);
	bu_cmd_validate_state_t state = operand_count < cmd_schema_minimum_operands(schema) ?
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
	argv[cursor_arg][0] == '-' && !cmd_schema_dash_numeric_operand_valid(schema,
	    operand_count, argv[cursor_arg])) {
	const char *current = argv[cursor_arg];
	int option_prefix = !current[1];
	const char *eq = strchr(current, '=');
	const struct bu_cmd_option *option = cmd_schema_lookup_token(schema, current);
	int cluster = !option && cmd_schema_short_flag_cluster(schema, current, NULL, NULL, 0) > 0;
	const char *value = eq ? eq + 1 : NULL;
	int value_valid = option && option->arg_requirement != BU_CMD_ARG_NONE && value ?
	    cmd_schema_option_attached_allowed(option) && cmd_schema_option_arguments_valid(option, 1, &value) : 1;
	int flag_has_value = option && option->arg_requirement == BU_CMD_ARG_NONE && eq;
	/* A lone '-' is an editable option prefix.  A completed command may still
	 * use it as an ordinary operand, but while the cursor is on that token the
	 * editor needs the option candidates rather than an immediate operand
	 * classification. */
	cmd_schema_set_result(result, option_prefix ? BU_CMD_VALIDATE_INCOMPLETE :
	    ((option && value_valid && !flag_has_value) || cluster ? BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INVALID),
	    cursor_arg, BU_CMD_EXPECT_OPTION, BU_CMD_VALUE_STRING,
	    option_prefix ? "option expected" :
	    (cluster ? "short option flags" : (option ? (flag_has_value ? "option does not take an argument" : (value_valid ? "option" : "invalid option argument")) : "unknown option")),
	    option ? option->semantic_provider : NULL);
	cmd_schema_add_option_candidates(schema, result, current);
	if (option && option->arg_requirement != BU_CMD_ARG_NONE && !eq) {
	    bu_cmd_validate_state_t state = option->arg_requirement == BU_CMD_ARG_OPTIONAL ?
		BU_CMD_VALIDATE_VALID : BU_CMD_VALIDATE_INCOMPLETE;
	    cmd_schema_set_result(result, state, cursor_arg, BU_CMD_EXPECT_OPTION_ARG,
		option->value_type, option->arg_requirement == BU_CMD_ARG_OPTIONAL ?
		"optional option argument" : "option argument expected", option->semantic_provider);
	    cmd_schema_add_keyword_candidates(option->value_keywords, option->keyword_values,
		option->arg_shape, result, "");
	}
	return 0;
    }

    const struct bu_cmd_operand *operand = cmd_schema_operand_at(schema, operand_count);
    if (cursor_arg < argc && argv && argv[cursor_arg]) {
	if (!operand) {
	    cmd_schema_set_result(result, BU_CMD_VALIDATE_INVALID, cursor_arg, BU_CMD_EXPECT_NONE,
		BU_CMD_VALUE_STRING, "too many operands", NULL);
	} else if (!cmd_schema_operand_valid(operand, argv[cursor_arg])) {
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
	return 0;
    }

    bu_cmd_validate_state_t state = operand_count < cmd_schema_minimum_operands(schema) ?
	BU_CMD_VALIDATE_INCOMPLETE : BU_CMD_VALIDATE_VALID;
    cmd_schema_set_result(result, state, cursor_arg,
	(options_allowed ? BU_CMD_EXPECT_OPTION : BU_CMD_EXPECT_NONE) |
	(operand ? BU_CMD_EXPECT_OPERAND : BU_CMD_EXPECT_NONE),
	operand ? operand->value_type : BU_CMD_VALUE_STRING,
	operand ? operand->name : (operand_count ? "operand" : "option or operand expected"),
	operand ? operand->semantic_provider : NULL);
    if (options_allowed)
	cmd_schema_add_option_candidates(schema, result, "");
    if (!result->completion_count && operand)
	cmd_schema_add_keyword_candidates(operand->value_keywords, operand->keyword_values,
	    operand->shape, result, "");
    return cmd_schema_apply_constraints(schema, argc, argv, result);
}


int
bu_cmd_schema_validate_ctx(const struct bu_cmd_schema *schema, size_t argc,
	const char **argv, size_t cursor_arg, void *context,
	struct bu_cmd_validate_result *result)
{
    if (!schema || !result || cursor_arg > argc)
	return -1;
    if (context && schema->validation.constraint_data.context_validate)
	return schema->validation.constraint_data.context_validate(schema, argc, argv,
	    cursor_arg, context, result);
    return bu_cmd_schema_validate(schema, argc, argv, cursor_arg, result);
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


int
bu_cmd_tree_dispatch(const struct bu_cmd_tree *tree, void *context,
	int argc, const char *argv[], int *result)
{
    const struct bu_cmd_tree_node *node;
    const char **canonical_argv = NULL;
    int command_result;

    if (!tree || argc < 1 || !argv || !argv[0])
	return -1;
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
	if (name && (!prefix_len || strncmp(name, prefix, prefix_len) == 0))
	    count++;
    }
    if (!count)
	return;
    result->completion_candidates = (const char **)bu_calloc(count + 1,
	sizeof(char *), "native command tree candidates");
    for (size_t i = 0; tree->subcommands[i].schema; i++) {
	const char *name = tree->subcommands[i].schema->name;
	if (name && (!prefix_len || strncmp(name, prefix, prefix_len) == 0))
	    result->completion_candidates[result->completion_count++] = bu_strdup(name);
    }
}


int
bu_cmd_tree_validate_argv(const struct bu_cmd_tree *tree, size_t argc,
	const char **argv, size_t cursor_arg, struct bu_cmd_validate_result *result)
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
	return bu_cmd_schema_validate(tree->root_schema, subcommand_index, argv,
	    cursor_arg, result);
    ret = bu_cmd_schema_validate(tree->root_schema, subcommand_index, argv,
	subcommand_index, &root_result);
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
	ret = bu_cmd_tree_validate_argv(&child_tree, argc - subcommand_index - 1,
	    argv + subcommand_index + 1, cursor_arg - subcommand_index - 1, result);
	if (!ret) {
	    result->token_start += subcommand_index + 1;
	    result->token_end += subcommand_index + 1;
	}
	return ret;
    }
    ret = bu_cmd_schema_validate(node->schema, argc - subcommand_index - 1,
	argv + subcommand_index + 1, cursor_arg - subcommand_index - 1, result);
    if (!ret) {
	result->token_start += subcommand_index + 1;
	result->token_end += subcommand_index + 1;
    }
    return ret;
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
bu_cmd_tree_describe(const struct bu_cmd_tree *tree)
{
    struct bu_vls out = BU_VLS_INIT_ZERO;
    char *option_help = NULL;

    if (!tree || !tree->root_schema || !tree->root_schema->name)
	return NULL;
    bu_vls_printf(&out, "Usage: %s [options] subcommand [args]\n",
	tree->root_schema->name);
    option_help = bu_cmd_schema_describe(tree->root_schema);
    if (option_help && option_help[0])
	bu_vls_printf(&out, "\nOptions:\n%s", option_help);
    if (option_help)
	bu_free(option_help, "native tree option help");
    if (tree->subcommands) {
	bu_vls_strcat(&out, "\nSubcommands:\n");
	cmd_tree_describe_children(&out, tree->subcommands, 1);
    }
    return bu_vls_strdup(&out);
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
	    cmd_schema_json_string(out, node->aliases[i]);
	}
    }
    bu_vls_strcat(out, "],\"child_phase\":");
    cmd_schema_json_string(out, cmd_tree_phase_name(node->child_phase));
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
    char *root_json = NULL;

    if (!tree || !tree->root_schema)
	return NULL;
    root_json = bu_cmd_schema_describe_json(tree->root_schema);
    bu_vls_strcat(&out, "{\"kind\":\"native_tree\",\"root\":");
    bu_vls_strcat(&out, root_json ? root_json : "{}");
    bu_vls_strcat(&out, ",\"child_phase\":");
    cmd_schema_json_string(&out, cmd_tree_phase_name(tree->child_phase));
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
    return bu_vls_strdup(&out);
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
    return cmd_tree_lint_nodes(tree->subcommands, msgs, 0);
}
