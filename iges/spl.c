#include <stdio.h>
#include "machine.h"
#include "vmath.h"
#include "./b_spline.h"

struct b_spline *
spl_new(u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, evp)
int u_order, v_order, n_u_knots, n_v_knots, n_rows, n_cols, evp;
{
	struct b_spline *srf;

	srf = (struct b_spline *) malloc(sizeof(struct b_spline));

	srf->next = (struct b_spline *)0;
	srf->order[0] = u_order;
	srf->order[1] = v_order;

	srf->u_kv = (struct knot_vec *) malloc(sizeof(struct knot_vec));
	srf->v_kv = (struct knot_vec *) malloc(sizeof(struct knot_vec));

	srf->u_kv->k_size = n_u_knots;
	srf->v_kv->k_size = n_v_knots;

	srf->u_kv->knots = (fastf_t *) calloc(n_u_knots, sizeof(fastf_t));
	srf->v_kv->knots = (fastf_t *) calloc(n_v_knots, sizeof(fastf_t));

	srf->ctl_mesh = (struct b_mesh *) malloc(sizeof(struct b_mesh));

	srf->ctl_mesh->mesh = (fastf_t *) calloc(n_rows * n_cols * evp,
		sizeof (fastf_t));

	srf->ctl_mesh->pt_type = evp;
	srf->ctl_mesh->mesh_size[0] = n_rows;
	srf->ctl_mesh->mesh_size[1] = n_cols;

	return srf;
}

void
spl_sfree(srf)
struct b_spline * srf;
{
	free((char *)srf->u_kv->knots);
	free((char *)srf->v_kv->knots);
	free((char *)srf->u_kv);
	free((char *)srf->v_kv);

	free((char *)srf->ctl_mesh->mesh);
	free((char *)srf->ctl_mesh);

	free((char *)srf);
}

struct knot_vec *
spl_kvknot(order, lower, upper, num)
int order, num;
fastf_t lower, upper;
{
	register int i;
	int total;
	fastf_t knot_step;
	register struct knot_vec *new_knots;

	total = order * 2 + num;

	knot_step = (upper - lower) / ( num + 1 );

	new_knots = (struct knot_vec *) malloc(sizeof(struct knot_vec));
	new_knots->k_size = total;

	new_knots->knots = (fastf_t *) calloc(total, sizeof(fastf_t));

	for (i = 0; i < order; i++)
		new_knots->knots[i] = lower;

	for (i = order; i <= (num + order -1); i++)
		new_knots->knots[i] = new_knots->knots[i-1] + knot_step;

	for (i = num + order; i < total; i++)
		new_knots->knots[i] = upper;

	return new_knots;
}
