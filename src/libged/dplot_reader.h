#ifndef DPLOT_PARSER
#define DPLOT_PARSER

#define PERPLEX_ON_ENTER struct dplot_data *data = (struct dplot_data *)yyextra;

#include "bu.h"
#include "dplot_parser.h"
#include "dplot_scanner.h"

typedef struct {
    int n;
} token_t;

struct isocsx {
    struct bu_list l;
    int events;
};

struct ssx {
    struct bu_list l;
    int brep1_surface;
    int brep2_surface;
    int final_curve_events;
    int face1_clipped_curves;
    int face2_clipped_curves;
    int intersecting_brep1_isocurves;
    int intersecting_isocurves;
    struct bu_list isocsx_list; /* struct isocsx list */
    int *isocsx_events;
};

struct dplot_data {
    token_t token_data;
    int brep1_surface_count;
    int brep2_surface_count;
    int ssx_count;
    struct bu_list ssx_list; /* struct ssx list */
    struct ssx *ssx;
    struct bu_list isocsx_list; /* struct ssx list */
};

/* lemon prototypes */
void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void *));
void Parse(void *yyp, int yymajor, token_t tokenData, struct dplot_data *data);
void ParseTrace(FILE *fp, char *s);

#endif
