#ifndef DPLOT_PARSER
#define DPLOT_PARSER

#define PERPLEX_ON_ENTER struct dplot_data *data = (struct dplot_data *)yyextra;

#include "bu.h"
#include "dplot_parser.h"
#include "dplot_scanner.h"

typedef struct {
    int n;
} token_t;

struct ssx {
    struct bu_list l;
    int brep1_surface;
    int brep2_surface;
    int final_curve_events;
    int intersecting_brep1_isocurves;
    int intersecting_isocurves;
};

struct dplot_data {
    token_t token_data;
    int brep1_surface_count;
    int brep2_surface_count;
    int ssx_count;
    struct bu_list ssx_list; /* struct ssx list */
    struct ssx *ssx;
};

/* lemon prototypes */
void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void *));
void Parse(void *yyp, int yymajor, token_t tokenData, struct dplot_data *data);
void ParseTrace(FILE *fp, char *s);

#endif
