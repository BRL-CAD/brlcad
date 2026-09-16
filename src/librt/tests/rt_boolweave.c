/*                  R T _ B O O L W E A V E . C
 * BRL-CAD
 *
 * Copyright (c) 2025-2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/**
 * Small, data-driven tests for rt_boolweave() and rt_boolfinal().
 *
 * The test deliberately hand-builds only the structures these APIs need:
 * two soltabs, one A op B region tree, and two segments.  That keeps the
 * test focused on ray interval behavior rather than database preparation.
 */

#include "common.h"

#include <math.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include "bu.h"
#include "raytrace.h"
#include "rt/boolweave.h"
#include "rt/op.h"
#include "rt/tree.h"


struct interval {
    double in;
    double out;
};

struct bool_case {
    const char *name;
    struct interval a;
    struct interval b;
};

struct bool_test_context {
    struct rt_i *rtip;
    struct resource *resp;
    struct soltab stp[2];
    struct region region;
};


static int failures = 0;


static void
check(int condition, const char *fmt, ...)
{
    va_list ap;
    char message[512];

    if (condition)
	return;

    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    bu_log("FAIL: %s\n", message);
    va_end(ap);
    failures++;
}


static struct seg *
create_segment(struct bool_test_context *ctx, int solid, double in_dist, double out_dist)
{
    struct seg *segp;

    RT_GET_SEG(segp, ctx->resp);
    segp->seg_stp = &ctx->stp[solid];
    segp->seg_in.hit_dist = in_dist;
    segp->seg_out.hit_dist = out_dist;
    return segp;
}


static union tree *
create_leaf(struct soltab *stp)
{
    union tree *leaf;

    BU_ALLOC(leaf, union tree);
    RT_TREE_INIT(leaf);
    leaf->tr_a.tu_op = OP_SOLID;
    leaf->tr_a.tu_stp = stp;
    return leaf;
}


static union tree *
create_boolean_tree(struct soltab *a, int op, struct soltab *b)
{
    union tree *node;

    BU_ALLOC(node, union tree);
    RT_TREE_INIT(node);
    node->tr_b.tb_op = op;
    node->tr_b.tb_left = create_leaf(a);
    node->tr_b.tb_right = create_leaf(b);
    return node;
}


static void
destroy_boolean_tree(union tree *treep)
{
    if (!treep)
	return;

    if (treep->tr_op != OP_SOLID) {
	destroy_boolean_tree(treep->tr_b.tb_left);
	destroy_boolean_tree(treep->tr_b.tb_right);
    }
    bu_free(treep, "boolweave test tree");
}


static void
init_partition_head(struct partition *head, uint32_t magic)
{
    memset(head, 0, sizeof(struct partition));
    head->pt_magic = magic;
    head->pt_forw = head;
    head->pt_back = head;
}


static int
near_equal(double a, double b)
{
    return fabs(a - b) <= 1.0e-9;
}


static int
interval_contains(struct interval in, double value)
{
    return value > in.in && value < in.out;
}


static int
boolean_value(int op, int a, int b)
{
    switch (op) {
	case OP_UNION:     return a || b;
	case OP_INTERSECT: return a && b;
	case OP_SUBTRACT:  return a && !b;
	case OP_XOR:       return (a && !b) || (!a && b);
	default:           return 0;
    }
}


/* Generate the expected positive-length intervals from the elementary
 * intervals induced by the four input endpoints.  This intentionally
 * treats touching intervals as one continuous result. */
static int
expected_intervals(int op, struct interval a, struct interval b, struct interval *expected)
{
    double cut[4] = {a.in, a.out, b.in, b.out};
    double tmp;
    int i, j, n = 0;

    /* bool_eval() returns -1 for an overlapping XOR (the internal GUARD
     * error state), and rt_boolfinal() treats every non-zero result as a
     * claiming region.  Preserve that observable behavior here; it is an
     * important reason to keep overlapping XOR in the matrix. */
    if (op == OP_XOR && a.in < b.out && b.in < a.out &&
	 a.in < a.out && b.in < b.out)
	return expected_intervals(OP_UNION, a, b, expected);

    for (i = 0; i < 4; i++) {
	for (j = i + 1; j < 4; j++) {
	    if (cut[j] < cut[i]) {
		tmp = cut[i];
		cut[i] = cut[j];
		cut[j] = tmp;
	    }
	}
    }

    for (i = 0; i < 3; i++) {
	double midpoint;
	int selected;

	if (near_equal(cut[i], cut[i + 1]))
	    continue;

	midpoint = (cut[i] + cut[i + 1]) * 0.5;
	selected = boolean_value(op,
				 interval_contains(a, midpoint),
				 interval_contains(b, midpoint));
	if (!selected)
	    continue;

	if (n > 0 && near_equal(expected[n - 1].out, cut[i])) {
	    expected[n - 1].out = cut[i + 1];
	} else {
	    expected[n].in = cut[i];
	    expected[n].out = cut[i + 1];
	    n++;
	}
    }

    return n;
}


static int
partition_count(const struct partition *head)
{
    const struct partition *pp;
    int count = 0;

    for (pp = head->pt_forw; pp != head; pp = pp->pt_forw)
	count++;
    return count;
}


static int
check_partitions(const struct partition *head, const struct interval *expected, int expected_count, const char *label)
{
    const struct partition *pp;
    int i = 0;

    check(partition_count(head) == expected_count,
	  "%s: expected %d result partition(s), got %d",
	  label, expected_count, partition_count(head));

    for (pp = head->pt_forw; pp != head && i < expected_count; pp = pp->pt_forw, i++) {
	check(near_equal(pp->pt_inhit->hit_dist, expected[i].in) &&
	      near_equal(pp->pt_outhit->hit_dist, expected[i].out),
	      "%s: result %d is (%g, %g), expected (%g, %g)",
	      label, i, pp->pt_inhit->hit_dist, pp->pt_outhit->hit_dist,
	      expected[i].in, expected[i].out);
    }

    return failures == 0;
}


static void
check_partition_segments(const struct partition *head, const struct seg *a, const struct seg *b, const char *label)
{
    const struct partition *pp;

    for (pp = head->pt_forw; pp != head; pp = pp->pt_forw) {
	struct seg **segpp;

	check(pp->pt_inseg == a || pp->pt_inseg == b,
	      "%s: input segment pointer was not preserved", label);
	check(pp->pt_outseg == a || pp->pt_outseg == b,
	      "%s: output segment pointer was not preserved", label);
	check(pp->pt_inhit == &pp->pt_inseg->seg_in ||
	      pp->pt_inhit == &pp->pt_inseg->seg_out,
	      "%s: input hit no longer belongs to its segment", label);
	check(pp->pt_outhit == &pp->pt_outseg->seg_in ||
	      pp->pt_outhit == &pp->pt_outseg->seg_out,
	      "%s: output hit no longer belongs to its segment", label);

	for (BU_PTBL_FOR(segpp, (struct seg **), &pp->pt_seglist)) {
	    check(*segpp == a || *segpp == b,
		  "%s: partition segment list contains a replacement pointer", label);
	}
    }
}


static void
reset_region(struct bool_test_context *ctx, int op)
{
    ctx->region.reg_treetop = create_boolean_tree(&ctx->stp[0], op, &ctx->stp[1]);
}


static void
run_case_impl(struct bool_test_context *ctx, const char *case_name,
	      struct interval a_interval, struct interval b_interval,
	      int op, int reverse, const struct interval *expected,
	      int expected_count)
{
    struct seg in_hd;
    struct seg out_hd;
    struct partition input_hd;
    struct partition final_hd;
    struct bu_ptbl regiontable = BU_PTBL_INIT_ZERO;
    struct bu_bitv solidbits = BU_BITV_INIT_ZERO;
    struct application ap;
    struct seg *a;
    struct seg *b;
    char label[160];

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = ctx->rtip;
    ap.a_resource = ctx->resp;

    BU_LIST_INIT(&in_hd.l);
    BU_LIST_INIT(&out_hd.l);
    init_partition_head(&input_hd, PT_HD_MAGIC);
    init_partition_head(&final_hd, PT_HD_MAGIC);

    reset_region(ctx, op);
    a = create_segment(ctx, 0, a_interval.in, a_interval.out);
    b = create_segment(ctx, 1, b_interval.in, b_interval.out);
    if (reverse) {
	BU_LIST_APPEND(&in_hd.l, &b->l);
	BU_LIST_APPEND(&in_hd.l, &a->l);
    } else {
	BU_LIST_APPEND(&in_hd.l, &a->l);
	BU_LIST_APPEND(&in_hd.l, &b->l);
    }

    rt_boolweave(&out_hd, &in_hd, &input_hd, &ap);
    snprintf(label, sizeof(label), "%s/%s/order-%s", case_name,
	     op == OP_UNION ? "union" :
	     op == OP_INTERSECT ? "intersect" :
	     op == OP_SUBTRACT ? "subtract" : "xor",
	     reverse ? "B-A" : "A-B");

    check(BU_LIST_IS_EMPTY(&in_hd.l), "%s: weave left input segments", label);

    BU_BITSET(&solidbits, 0);
    BU_BITSET(&solidbits, 1);
    rt_boolfinal(&input_hd, &final_hd, 0.0, INFINITY,
		 &regiontable, &ap, &solidbits);
    check_partitions(&final_hd, expected, expected_count, label);
    check_partition_segments(&final_hd, a, b, label);

    RT_FREE_PT_LIST(&input_hd, ctx->resp);
    RT_FREE_PT_LIST(&final_hd, ctx->resp);
    RT_FREE_SEG_LIST(&out_hd, ctx->resp);
    bu_ptbl_free(&regiontable);
    destroy_boolean_tree(ctx->region.reg_treetop);
    ctx->region.reg_treetop = TREE_NULL;
}


static void
run_case(struct bool_test_context *ctx, const struct bool_case *tc, int op, int reverse)
{
    struct interval expected[4];
    int expected_count;

    expected_count = expected_intervals(op, tc->a, tc->b, expected);
    run_case_impl(ctx, tc->name, tc->a, tc->b, op, reverse,
		  expected, expected_count);
}


static void
run_empty_case(struct bool_test_context *ctx)
{
    struct seg in_hd;
    struct seg out_hd;
    struct partition input_hd;
    struct partition final_hd;
    struct bu_ptbl regiontable = BU_PTBL_INIT_ZERO;
    struct bu_bitv solidbits = BU_BITV_INIT_ZERO;
    struct application ap;

    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = ctx->rtip;
    ap.a_resource = ctx->resp;
    BU_LIST_INIT(&in_hd.l);
    BU_LIST_INIT(&out_hd.l);
    init_partition_head(&input_hd, PT_HD_MAGIC);
    init_partition_head(&final_hd, PT_HD_MAGIC);
    reset_region(ctx, OP_UNION);

    rt_boolweave(&out_hd, &in_hd, &input_hd, &ap);
    rt_boolfinal(&input_hd, &final_hd, 0.0, INFINITY,
		 &regiontable, &ap, &solidbits);
    check(BU_LIST_IS_EMPTY(&in_hd.l) && BU_LIST_IS_EMPTY(&out_hd.l) &&
	  partition_count(&input_hd) == 0 && partition_count(&final_hd) == 0,
	  "empty input: weave/final produced output");

    RT_FREE_PT_LIST(&input_hd, ctx->resp);
    RT_FREE_PT_LIST(&final_hd, ctx->resp);
    RT_FREE_SEG_LIST(&out_hd, ctx->resp);
    bu_ptbl_free(&regiontable);
    destroy_boolean_tree(ctx->region.reg_treetop);
    ctx->region.reg_treetop = TREE_NULL;
}


static void
run_edge_cases(struct bool_test_context *ctx)
{
    static const int ops[] = {OP_UNION, OP_INTERSECT, OP_SUBTRACT, OP_XOR};
    struct interval negative_expected[1];
    struct interval origin_expected[2];
    struct interval tolerance_union_expected[1];
    struct interval tolerance_subtract_a_first[1];
    struct interval tolerance_subtract_b_first[1];
    struct bool_case zero_a = {"zero-a", {2.0, 2.0}, {4.0, 6.0}};
    struct bool_case negative = {"behind-ray", {-5.0, -2.0}, {-4.0, -1.0}};
    struct bool_case origin = {"near-origin", {0.0001, 2.0}, {4.0, 5.0}};
    struct bool_case tolerance = {"near-touching", {1.0, 3.0}, {3.0005, 5.0}};
    size_t i;

    negative_expected[0] = (struct interval){0.0, 0.0};
    origin_expected[0] = (struct interval){0.0, 2.0};
    origin_expected[1] = (struct interval){4.0, 5.0};
    tolerance_union_expected[0] = (struct interval){1.0, 5.0};
    tolerance_subtract_a_first[0] = (struct interval){1.0, 3.0005};
    tolerance_subtract_b_first[0] = (struct interval){1.0, 3.0};

    run_empty_case(ctx);

    /* These calls are kept explicit: the expected list documents the
     * normalization performed by the API at each edge. */
    for (i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
	int op = ops[i];
	struct interval expected[3];
	int count;

	if (op == OP_UNION || op == OP_XOR) {
	    expected[0] = (struct interval){2.0, 2.0};
	    expected[1] = (struct interval){4.0, 6.0};
	    count = 2;
	} else if (op == OP_SUBTRACT) {
	    expected[0] = (struct interval){2.0, 2.0};
	    count = 1;
	} else {
	    count = 0;
	}
	run_case_impl(ctx, zero_a.name, zero_a.a, zero_a.b, op, 0,
		      expected, count);
	run_case_impl(ctx, zero_a.name, zero_a.a, zero_a.b, op, 1,
		      expected, count);

	count = 0;
	run_case_impl(ctx, negative.name, negative.a, negative.b, op, 0,
		      negative_expected, count);
	run_case_impl(ctx, negative.name, negative.a, negative.b, op, 1,
		      negative_expected, count);

	if (op == OP_UNION || op == OP_XOR) {
	    count = 2;
	} else if (op == OP_SUBTRACT) {
	    count = 1;
	} else {
	    count = 0;
	}
	run_case_impl(ctx, origin.name, origin.a, origin.b, op, 0,
		      origin_expected, count);
	run_case_impl(ctx, origin.name, origin.a, origin.b, op, 1,
		      origin_expected, count);

	if (op == OP_UNION || op == OP_XOR) {
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 0,
			  tolerance_union_expected, 1);
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 1,
			  tolerance_union_expected, 1);
	} else if (op == OP_SUBTRACT) {
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 0,
			  tolerance_subtract_a_first, 1);
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 1,
			  tolerance_subtract_b_first, 1);
	} else {
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 0,
			  tolerance_union_expected, 0);
	    run_case_impl(ctx, tolerance.name, tolerance.a, tolerance.b, op, 1,
			  tolerance_union_expected, 0);
	}
    }
}


static void
init_context(struct bool_test_context *ctx)
{
    int i;

    memset(ctx, 0, sizeof(*ctx));
    ctx->rtip = rt_dirbuild_inmem(NULL, 0, NULL, 0);
    check(ctx->rtip != NULL, "rt_dirbuild_inmem() failed");
    if (!ctx->rtip)
	return;

    ctx->rtip->rti_tol.magic = BN_TOL_MAGIC;
    ctx->rtip->rti_tol.dist = 0.001;
    ctx->rtip->rti_tol.dist_sq = ctx->rtip->rti_tol.dist * ctx->rtip->rti_tol.dist;
    ctx->rtip->rti_tol.perp = 1.0e-6;
    ctx->rtip->rti_tol.para = 1.0 - 1.0e-6;
    ctx->rtip->stats.nsolids = 2;

    rt_init_resource(&rt_uniresource, 0, ctx->rtip);
    ctx->resp = &rt_uniresource;

    ctx->region.l.magic = RT_REGION_MAGIC;
    ctx->region.reg_name = "boolweave-test.r";
    ctx->region.reg_bit = 0;
    ctx->region.reg_aircode = 0;
    ctx->region.reg_all_unions = 0;

    for (i = 0; i < 2; i++) {
	ctx->stp[i].l.magic = RT_SOLTAB_MAGIC;
	ctx->stp[i].l2.magic = RT_SOLTAB2_MAGIC;
	ctx->stp[i].st_rtip = ctx->rtip;
	ctx->stp[i].st_bit = i;
	ctx->stp[i].st_aradius = INFINITY;
	BU_PTBL_INIT(&ctx->stp[i].st_regions);
	bu_ptbl_ins_unique(&ctx->stp[i].st_regions, (long *)&ctx->region);
    }
}


static void
destroy_context(struct bool_test_context *ctx)
{
    bu_ptbl_free(&ctx->stp[0].st_regions);
    bu_ptbl_free(&ctx->stp[1].st_regions);
    if (ctx->rtip)
	rt_i_destroy(ctx->rtip);
}


int
main(int ac, char *av[])
{
    static const struct bool_case cases[] = {
	{"disjoint", {1.0, 2.0}, {4.0, 5.0}},
	{"touching", {1.0, 3.0}, {3.0, 5.0}},
	{"partial-overlap", {1.0, 4.0}, {3.0, 6.0}},
	{"a-contains-b", {1.0, 6.0}, {3.0, 4.0}},
	{"b-contains-a", {3.0, 4.0}, {1.0, 6.0}},
	{"equal", {1.0, 5.0}, {1.0, 5.0}},
	{"reversed-a", {5.0, 1.0}, {2.0, 4.0}},
	{"reversed-both", {5.0, 1.0}, {6.0, 2.0}}
    };
    const int ops[] = {OP_UNION, OP_INTERSECT, OP_SUBTRACT, OP_XOR};
    struct bool_test_context ctx;
    size_t i, j, k;

    bu_setprogname(av[0]);
    if (ac != 1) {
	bu_log("Usage: %s\n", av[0]);
	return 1;
    }

    init_context(&ctx);
    if (!ctx.rtip)
	return 1;

    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
	for (j = 0; j < sizeof(ops) / sizeof(ops[0]); j++) {
	    for (k = 0; k < 2; k++) {
		run_case(&ctx, &cases[i], ops[j], (int)k);
	    }
	}
    }
    run_edge_cases(&ctx);

    destroy_context(&ctx);
    return failures ? 1 : 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8 cino=N-s
 */
