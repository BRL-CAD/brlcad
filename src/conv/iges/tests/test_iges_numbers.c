/* BRL-CAD
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "../iges_struct.h"

#include <limits.h>

#include "../iges_extern.h"
#include "../iges_output.h"

/* Only the legacy record source and diagnostic sink are substituted.  The
 * numeric and Hollerith readers and their shared implementations are real. */
char card[256];
char eofd = ',';
char eord = ';';
int currec;
int counter;
fastf_t conv_factor = 2.0;

static struct bu_vls input = BU_VLS_INIT_ZERO;
static char input_section;
static size_t warnings;

int
Readrec(int record)
{
    const size_t width = input_section == 'P' ? PARAMLEN + 1 : CARDLEN + 1;
    const size_t start = (record - 1) * width;
    currec = record;
    if (start >= bu_vls_strlen(&input))
	return 1;
    memset(card, ' ', sizeof(card));
    const size_t length = bu_vls_strlen(&input) - start;
    memcpy(card, bu_vls_cstr(&input) + start, length < width ? length : width);
    card[IGES_SECTION_COL] = input_section;
    counter = 0;
    return 0;
}

void
iges_output_legacy_warning(int UNUSED(entity), const char *UNUSED(code), const char *UNUSED(message))
{
    ++warnings;
}

void
Skip_field(void)
{
    /* A successfully read Hollerith body must be followed only by its
     * delimiter.  Integration tests exercise the production field skipper. */
    char field[MAX_NUM];
    if (iges_read_number(field))
	bu_exit(1, "Hollerith reader did not consume exactly its declared length\n");
}

static void
prepare(char section, int offset, const char *field)
{
    input_section = section;
    bu_vls_sprintf(&input, "%*s%s%c7%c", offset, "", field, eofd, eord);
    if (Readrec(1))
	bu_exit(1, "Could not prepare numeric test records\n");
    counter = offset;
    warnings = 0;
}

static void
check_real(void (*reader)(double *, const char *), char section, int offset,
    const char *field, double expected, double next)
{
    prepare(section, offset, field);
    double value = 19.0;
    reader(&value, "");
    if (!isfinite(value) || !NEAR_EQUAL(value, expected, SMALL_FASTF) || warnings)
	bu_exit(1, "Incorrect real '%s' at %c/%d: %g, %zu warnings\n",
	    field, section, offset, value, warnings);
    reader(&value, "");
    if (!NEAR_EQUAL(value, next, SMALL_FASTF) || warnings)
	bu_exit(1, "Real reader lost the next field at %c/%d\n", section, offset);
    const int terminal = counter;
    reader(&value, "");
    if (counter != terminal || !NEAR_EQUAL(value, next, SMALL_FASTF) || warnings)
	bu_exit(1, "Real reader consumed the record terminator\n");
}

int
main(void)
{
    const char *reals[] = {"120", "-1.25", "+1.2D+02", "1.2d+02", " 120 ", "", "   "};
    const double values[] = {120.0, -1.25, 120.0, 120.0, 120.0, 19.0, 19.0};
    const char *integers[] = {"120", "-120", "+120", " 120 ", "", "   "};
    const int integer_values[] = {120, -120, 120, 120, 19, 19};

    for (int delimiters = 0; delimiters < 2; ++delimiters) {
	eofd = delimiters ? '^' : ',';
	eord = delimiters ? '!' : ';';
	for (int section = 0; section < 2; ++section) {
	    const char kind = section ? 'P' : 'G';
	    const int width = section ? PARAMLEN + 1 : CARDLEN + 1;
	    for (int offset = 0; offset <= width; ++offset) {
		for (size_t i = 0; i < sizeof(reals) / sizeof(reals[0]); ++i) {
		    check_real(Readflt, kind, offset, reals[i], values[i], 7.0);
		    check_real(Readdbl, kind, offset, reals[i], values[i], 7.0);
		    /* Empty parameters retain the caller's default without scaling. */
		    const int empty = strspn(reals[i], " ") == strlen(reals[i]);
		    const double converted = empty ? values[i] : values[i] * conv_factor;
		    check_real(Readcnv, kind, offset, reals[i], converted, 7.0 * conv_factor);
		}
		for (size_t i = 0; i < sizeof(integers) / sizeof(integers[0]); ++i) {
		    prepare(kind, offset, integers[i]);
		    int value = 19;
		    Readint(&value, "");
		    if (value != integer_values[i] || warnings)
			bu_exit(1, "Incorrect integer '%s' at %c/%d\n", integers[i], kind, offset);
		    Readint(&value, "");
		    const int terminal = counter;
		    Readint(&value, "");
		    if (value != 7 || counter != terminal || warnings)
			bu_exit(1, "Integer reader lost the next field or terminator\n");
		}
		prepare(kind, offset, "5Hhello");
		Readstrg("");
		int next = 0;
		Readint(&next, "");
		if (next != 7 || warnings)
		    bu_exit(1, "String reader lost the next field at %c/%d\n", kind, offset);
		const struct {
		    const char *field;
		    size_t expected_warnings;
		} timestamps[] = {{"15H20260905.000000", 0}, {"13H260905.000000", 0},
		    {"4H2026", 1}, {"15H20260905!000000", 1}};
		for (size_t i = 0; i < sizeof(timestamps) / sizeof(timestamps[0]); ++i) {
		    prepare(kind, offset, timestamps[i].field);
		    Readtime("");
		    Readint(&next, "");
		    if (next != 7 || warnings != timestamps[i].expected_warnings)
			bu_exit(1, "Timestamp reader lost the next field at %c/%d\n", kind, offset);
		}
	    }
	}
    }

    const char *invalid_reals[] = {"1junk", "NaN", "Inf", "1e9999", "1e-9999"};
    for (size_t i = 0; i < sizeof(invalid_reals) / sizeof(invalid_reals[0]); ++i) {
	prepare('P', PARAMLEN, invalid_reals[i]);
	double value = 19.0;
	Readflt(&value, "");
	if (!ZERO(value) || warnings != 1)
	    bu_exit(1, "Invalid real was accepted without a diagnostic\n");
	Readflt(&value, "");
	if (!NEAR_EQUAL(value, 7.0, SMALL_FASTF) || warnings != 1)
	    bu_exit(1, "Invalid real consumed the next field\n");
    }
    prepare('P', PARAMLEN, "1junk");
    int integer = 19;
    Readint(&integer, "");
    if (integer != INT_MIN || warnings != 1)
	bu_exit(1, "Invalid integer was accepted without a diagnostic\n");

    /* An oversized field must be discarded as a whole, not split into
     * plausible but incorrect values in subsequent parameters. */
    char excessive[MAX_NUM + 1];
    memset(excessive, '1', sizeof(excessive) - 1);
    excessive[sizeof(excessive) - 1] = '\0';
    prepare('P', 0, excessive);
    Readint(&integer, "");
    if (integer != INT_MIN || warnings != 1)
	bu_exit(1, "Excessive numeric field was not rejected\n");
    Readint(&integer, "");
    if (integer != 7 || warnings != 1)
	bu_exit(1, "Excessive numeric field was not discarded\n");

    bu_vls_strcpy(&input, "120");
    Readrec(1);
    warnings = 0;
    Readint(&integer, "");
    if (integer != INT_MIN || warnings != 1)
	bu_exit(1, "Truncated numeric field was not rejected\n");
    bu_vls_free(&input);
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
