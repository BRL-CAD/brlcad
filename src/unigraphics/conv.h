/* default indentation for a single level in the DAG */
#define LEVEL_INDENT 6
#define FLAG_SUPPRESS		0
#define FLAG_RESET_SUP		1
#define FLAG_FACETIZE		2
#define MAX_FLAGS 14 /* UG limitation */
extern int flags[MAX_FLAGS];

extern int feature_is_suppressible(tag_t feat,
				   int level,
				   double units_conv,
				   uf_list_p_t sup_l);

typedef struct ug_tol {
    double	dist;
    double	radius;
} ug_tol;

extern ug_tol ugtol;

extern int debug;
#define dprintf if (debug) printf

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
