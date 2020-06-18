#if defined(__cplusplus)
extern "C" {
#endif

struct nc;

int nc_create(struct nc **n);
void nc_destroy(struct nc *n);

void add_number(struct nc *n, int num);
void tally(struct nc *n);

#if defined(__cplusplus)
}
#endif

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */

