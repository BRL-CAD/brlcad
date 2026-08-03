/*                     T E S T _ T A B D A T A . C
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

#include "common.h"

#include "test_util.h"


static int
test_tabdata_io(void)
{
    int failures = 0;
    const char *test = "tabdata_io";
    fastf_t xs[5] = {10.0, 12.0, 14.0, 16.0, 18.0};
    fastf_t ys[4] = {1.0, 3.0, 5.0, 7.0};
    struct bn_table *table = NULL;
    struct bn_table *table_read = NULL;
    struct bn_tabdata *data = NULL;
    struct bn_tabdata *text_read = NULL;
    struct bn_tabdata *bin_read = NULL;
    char table_path[MAXPATHLEN] = {0};
    char text_path[MAXPATHLEN] = {0};
    char bin_path[MAXPATHLEN] = {0};
    FILE *fp = NULL;

    table = make_table(xs, 4);
    data = make_tabdata(table, ys);

    if (!make_temp_path(table_path) || !make_temp_path(text_path) || !make_temp_path(bin_path)) {
	report_failure(test, "failed to allocate temporary file paths");
	failures++;
	goto cleanup;
    }

    if (bn_table_write(table_path, table) != 0) {
	report_failure(test, "bn_table_write failed");
	failures++;
	goto cleanup;
    }
    table_read = bn_table_read(table_path);
    if (!table_close(table, table_read, 0.0)) {
	report_failure(test, "bn_table_read did not round-trip the written table");
	failures++;
    }

    if (bn_print_table_and_tabdata(text_path, data) != 0) {
	report_failure(test, "bn_print_table_and_tabdata failed");
	failures++;
	goto cleanup;
    }
    text_read = bn_read_table_and_tabdata(text_path);
    if (!tabdata_close(data, text_read, 0.0)) {
	report_failure(test, "bn_read_table_and_tabdata did not round-trip the written tabdata");
	failures++;
    }

    fp = fopen(bin_path, "wb");
    if (!fp) {
	report_failure(test, "failed to create binary tabdata file");
	failures++;
	goto cleanup;
    }
    if (fwrite(data, BN_SIZEOF_TABDATA(table), 1, fp) != 1) {
	report_failure(test, "failed to write binary tabdata contents");
	failures++;
	fclose(fp);
	fp = NULL;
	goto cleanup;
    }
    fclose(fp);
    fp = NULL;

    bin_read = bn_tabdata_binary_read(bin_path, 1, table);
    if (!bin_read || !tabdata_close(data, bin_read, 0.0)) {
	report_failure(test, "bn_tabdata_binary_read did not round-trip the serialized tabdata");
	failures++;
    }

    bn_tabdata_constval(data, 2.5);
    if (!scalar_close(data->y[0], 2.5, 0.0) ||
	!scalar_close(data->y[1], 2.5, 0.0) ||
	!scalar_close(data->y[2], 2.5, 0.0) ||
	!scalar_close(data->y[3], 2.5, 0.0)) {
	report_failure(test, "bn_tabdata_constval failed to set every sample value");
	failures++;
    }

cleanup:
    if (fp) {
	fclose(fp);
    }
    if (table_path[0]) bu_file_delete(table_path);
    if (text_path[0]) bu_file_delete(text_path);
    if (bin_path[0]) bu_file_delete(bin_path);
    if (table_read) bn_table_free(table_read);
    if (text_read) {
	const struct bn_table *text_table = text_read->table;
	bn_tabdata_free(text_read);
	bn_table_free((struct bn_table *)text_table);
    }
    if (bin_read) bn_tabdata_free(bin_read);
    if (data) bn_tabdata_free(data);
    if (table) bn_table_free(table);

    return failures;
}


static int
test_tabdata_ops(void)
{
    int failures = 0;
    const char *test = "tabdata_ops";
    static const fastf_t uniform_xs[] = {
	0.0, 10.0, 20.0, 30.0, 40.0, 50.0,
	60.0, 70.0, 80.0, 90.0, 100.0
    };
    static const fastf_t xs[] = {0.0, 20.0, 40.0, 60.0, 80.0, 100.0};
    static const fastf_t xs_scaled[] = {0.0, 40.0, 80.0, 120.0, 160.0, 200.0};
    static const fastf_t xs_interp[] = {0.0, 20.0, 30.0, 60.0, 80.0, 100.0};
    static const fastf_t y1[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    static const fastf_t y2[] = {8.0, 3.0, 6.0, 2.0, 9.0};
    static const fastf_t y3[] = {8.0, 6.0, 18.0, 8.0, 45.0};
    static const fastf_t yrev[] = {5.0, 4.0, 3.0, 2.0, 1.0};
    static const fastf_t yblend[] = {2.0, 4.0, 6.0, 8.0, 10.0};
    static const fastf_t yblend3[] = {3.0, 2.0, 4.0, 1.0, 5.0};
    static const fastf_t yadd[] = {9.0, 5.0, 9.0, 6.0, 14.0};
    static const fastf_t ymul[] = {8.0, 6.0, 18.0, 8.0, 45.0};
    static const fastf_t ymul3[] = {64.0, 36.0, 324.0, 64.0, 2025.0};
    static const fastf_t yincr3[] = {133.0, 76.0, 651.0, 130.0, 4051.0};
    static const fastf_t yincr2[] = {21.0, 16.0, 39.0, 18.0, 91.0};
    static const fastf_t yscale[] = {2.0, 4.0, 6.0, 8.0, 10.0};
    static const fastf_t yjoin1[] = {11.0, 10.0, 9.0, 8.0, 7.0};
    static const fastf_t yjoin2[] = {17.0, 22.0, 27.0, 32.0, 37.0};
    static const fastf_t yblend2[] = {4.5, 6.0, 7.5, 9.0, 10.5};
    static const fastf_t yblend3_expected[] = {13.5, 12.0, 19.5, 12.0, 25.5};
    static const fastf_t yconst[] = {3.0, 3.0, 3.0, 3.0, 3.0};

    {
	struct bn_table *actual = bn_table_make_uniform(10, 0.0, 100.0);
	struct bn_table *expected = make_table(uniform_xs, 10);
	if (!table_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_table_make_uniform produced the wrong uniform sample positions");
	    failures++;
	}
	bn_table_free(expected);
	bn_table_free(actual);
    }

    {
	struct bn_table *tab = make_table(xs, 5);
	struct bn_table *tab_scaled = make_table(xs, 5);
	struct bn_table *expected_scaled = make_table(xs_scaled, 5);
	struct bn_tabdata *td1 = make_tabdata(tab, y1);
	struct bn_tabdata *td2 = make_tabdata(tab, y2);
	struct bn_tabdata *td3 = make_tabdata(tab, y3);
	struct bn_tabdata *tdrev = make_tabdata(tab, yrev);
	struct bn_tabdata *tdblend = make_tabdata(tab, yblend);
	struct bn_tabdata *tdblend3 = make_tabdata(tab, yblend3);
	struct bn_tabdata *actual = NULL;
	struct bn_tabdata *expected = NULL;
	struct bn_tabdata *dup = NULL;

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yadd);
	bn_tabdata_add(actual, td1, td2);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_add produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, ymul);
	bn_tabdata_mul(actual, td1, td2);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_mul produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, ymul3);
	bn_tabdata_mul3(actual, td1, td2, td3);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_mul3 produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, yrev);
	expected = make_tabdata(tab, yincr3);
	bn_tabdata_incr_mul3_scale(actual, td1, td2, td3, 2.0);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_incr_mul3_scale produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, yrev);
	expected = make_tabdata(tab, yincr2);
	bn_tabdata_incr_mul2_scale(actual, td1, td2, 2.0);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_incr_mul2_scale produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yscale);
	bn_tabdata_scale(actual, td1, 2.0);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_scale produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	bn_table_scale(tab_scaled, 2.0);
	if (!table_close(tab_scaled, expected_scaled, 0.0)) {
	    report_failure(test, "bn_table_scale produced the wrong exact result");
	    failures++;
	}

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yjoin1);
	bn_tabdata_join1(actual, td1, 2.0, tdrev);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_join1 produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yjoin2);
	bn_tabdata_join2(actual, td1, 2.0, tdrev, 3.0, tdblend);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_join2 produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yblend2);
	bn_tabdata_blend2(actual, 2.0, td1, 0.5, tdrev);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_blend2 produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	actual = make_tabdata(tab, y1);
	expected = make_tabdata(tab, yblend3_expected);
	bn_tabdata_blend3(actual, 2.0, td1, 0.5, tdrev, 3.0, tdblend3);
	if (!tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_blend3 produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	bn_tabdata_free(actual);

	if (!scalar_close(bn_tabdata_area1(td1), 15.0, 0.0)) {
	    report_failure(test, "bn_tabdata_area1 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_tabdata_area2(td1), 300.0, 0.0)) {
	    report_failure(test, "bn_tabdata_area2 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_tabdata_mul_area1(td1, tdrev), 35.0, 0.0)) {
	    report_failure(test, "bn_tabdata_mul_area1 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_tabdata_mul_area2(td1, tdrev), 700.0, 0.0)) {
	    report_failure(test, "bn_tabdata_mul_area2 produced the wrong exact result");
	    failures++;
	}

	actual = make_tabdata(tab, yrev);
	bn_tabdata_copy(actual, td1);
	if (!tabdata_close(actual, td1, 0.0)) {
	    report_failure(test, "bn_tabdata_copy failed to duplicate the source data");
	    failures++;
	}
	bn_tabdata_free(actual);

	dup = bn_tabdata_dup(td1);
	if (!dup || !tabdata_close(dup, td1, 0.0)) {
	    report_failure(test, "bn_tabdata_dup failed to duplicate the source data");
	    failures++;
	}
	if (dup) bn_tabdata_free(dup);

	expected = make_tabdata(tab, yconst);
	actual = bn_tabdata_get_constval(3.0, tab);
	if (!actual || !tabdata_close(actual, expected, 0.0)) {
	    report_failure(test, "bn_tabdata_get_constval produced the wrong exact result");
	    failures++;
	}
	bn_tabdata_free(expected);
	if (actual) bn_tabdata_free(actual);

	bn_tabdata_free(tdblend3);
	bn_tabdata_free(tdblend);
	bn_tabdata_free(tdrev);
	bn_tabdata_free(td3);
	bn_tabdata_free(td2);
	bn_tabdata_free(td1);
	bn_table_free(expected_scaled);
	bn_table_free(tab_scaled);
	bn_table_free(tab);
    }

    {
	struct bn_table *tab = make_table(xs, 5);
	struct bn_table *tab_alt = make_table(xs_interp, 5);
	struct bn_tabdata *td = make_tabdata(tab, y1);
	struct bn_tabdata *td_alt = make_tabdata(tab_alt, y1);

	if (!scalar_close(bn_table_lin_interp(td, 30.0), 2.5, 0.0)) {
	    report_failure(test, "bn_table_lin_interp case 1 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_table_lin_interp(td_alt, 30.0), 3.0, 0.0)) {
	    report_failure(test, "bn_table_lin_interp case 2 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_table_lin_interp(td_alt, 45.0), 3.5, 0.0)) {
	    report_failure(test, "bn_table_lin_interp case 3 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_table_lin_interp(td_alt, 25.0), 2.5, 0.0)) {
	    report_failure(test, "bn_table_lin_interp case 4 produced the wrong exact result");
	    failures++;
	}
	if (!scalar_close(bn_table_lin_interp(td, 800.0), 0.0, 0.0)) {
	    report_failure(test, "bn_table_lin_interp case 5 produced the wrong exact result");
	    failures++;
	}

	bn_tabdata_free(td_alt);
	bn_tabdata_free(td);
	bn_table_free(tab_alt);
	bn_table_free(tab);
    }

    return failures;
}


static int
test_tabdata_utils(void)
{
    int failures = 0;
    const char *test = "tabdata_utils";
    fastf_t xs_a[5] = {0.0, 10.0, 20.0, 30.0, 40.0};
    fastf_t xs_b[5] = {10.0, 15.0, 25.0, 30.0, 40.0};
    fastf_t xs_expected_merge[7] = {0.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0};
    fastf_t xs_expected_delete[3] = {0.0, 30.0, 40.0};
    fastf_t xs_filter[4] = {0.0, 10.0, 20.0, 30.0};
    fastf_t expected_filter[3] = {0.5, 1.0, 0.5};
    fastf_t xs_shift[5] = {0.0, 1.0, 2.0, 3.0, 4.0};
    fastf_t ys_shift[4] = {0.0, 10.0, 20.0, 30.0};
    fastf_t expected_shift[4] = {5.0, 15.0, 30.0, 30.0};
    struct bn_table *a = NULL;
    struct bn_table *b = NULL;
    struct bn_table *merged = NULL;
    struct bn_table *expected_merge = NULL;
    struct bn_table *to_delete = NULL;
    struct bn_table *expected_delete = NULL;
    struct bn_table *filter_table = NULL;
    struct bn_table *shift_table = NULL;
    struct bn_tabdata *filter = NULL;
    struct bn_tabdata *expected_filter_td = NULL;
    struct bn_tabdata *shift_in = NULL;
    struct bn_tabdata *shift_out = NULL;
    struct bn_tabdata *expected_shift_td = NULL;
    size_t killed;

    a = make_table(xs_a, 4);
    b = make_table(xs_b, 4);
    expected_merge = make_table(xs_expected_merge, 6);
    merged = bn_table_merge2(a, b);
    if (!table_close(merged, expected_merge, 0.0)) {
	report_failure(test, "bn_table_merge2 did not merge and deduplicate sample points correctly");
	failures++;
    }

    to_delete = make_table(xs_a, 4);
    expected_delete = make_table(xs_expected_delete, 2);
    killed = bn_table_delete_sample_pnts(to_delete, 1, 2);
    if (killed != 2 || !table_close(to_delete, expected_delete, 0.0)) {
	report_failure(test, "bn_table_delete_sample_pnts did not remove the requested sample points");
	failures++;
    }

    filter_table = make_table(xs_filter, 3);
    filter = bn_tabdata_mk_linear_filter(filter_table, 5.0, 25.0);
    expected_filter_td = make_tabdata(filter_table, expected_filter);
    if (!tabdata_close(filter, expected_filter_td, 1.0e-12)) {
	report_failure(test, "bn_tabdata_mk_linear_filter produced unexpected filter weights");
	failures++;
    }

    shift_table = make_table(xs_shift, 4);
    shift_in = make_tabdata(shift_table, ys_shift);
    {
	fastf_t shift_zero[4] = {0.0, 0.0, 0.0, 0.0};
	shift_out = make_tabdata(shift_table, shift_zero);
    }
    expected_shift_td = make_tabdata(shift_table, expected_shift);
    bn_tabdata_freq_shift(shift_out, shift_in, 0.5);
    if (!tabdata_close(shift_out, expected_shift_td, 1.0e-12)) {
	report_failure(test, "bn_tabdata_freq_shift produced unexpected shifted samples");
	failures++;
    }

    if (expected_shift_td) bn_tabdata_free(expected_shift_td);
    if (shift_out) bn_tabdata_free(shift_out);
    if (shift_in) bn_tabdata_free(shift_in);
    if (shift_table) bn_table_free(shift_table);
    if (expected_filter_td) bn_tabdata_free(expected_filter_td);
    if (filter) bn_tabdata_free(filter);
    if (filter_table) bn_table_free(filter_table);
    if (expected_delete) bn_table_free(expected_delete);
    if (to_delete) bn_table_free(to_delete);
    if (expected_merge) bn_table_free(expected_merge);
    if (merged) bn_table_free(merged);
    if (b) bn_table_free(b);
    if (a) bn_table_free(a);

    return failures;
}


static const struct bn_api_case tabdata_cases[] = {
    {"io", test_tabdata_io},
    {"ops", test_tabdata_ops},
    {"utils", test_tabdata_utils},
    {NULL, NULL}
};


int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    return bn_api_dispatch(argc, argv, tabdata_cases);
}
