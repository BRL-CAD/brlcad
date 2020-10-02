#ifndef PPMCMAP_INCLUDED
#define PPMCMAP_INCLUDED
/* ppmcmap.h - header file for colormap routines in libppm
*/

#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif


/* Color histogram stuff. */

typedef struct colorhist_item* colorhist_vector;
struct colorhist_item {
    pixel color;
    int value;
};

typedef struct colorhist_list_item* colorhist_list;
struct colorhist_list_item {
    struct colorhist_item ch;
    colorhist_list next;
};

colorhist_vector
ppm_computecolorhist( pixel ** const pixels, 
                      const int cols, const int rows, const int maxcolors, 
                      int * const colorsP );
colorhist_vector
ppm_computecolorhist2(FILE * const ifp,
                      const int cols, const int rows, 
                      const pixval maxval, const int format, 
                      const int maxcolors, int * const colorsP );

void
ppm_addtocolorhist( colorhist_vector chv, 
                    int * const colorsP, const int maxcolors, 
                    const pixel * const colorP, 
                    const int value, const int position );

void
ppm_freecolorhist(colorhist_vector const chv);


/* Color hash table stuff. */

typedef colorhist_list* colorhash_table;

colorhash_table
ppm_computecolorhash( pixel ** const pixels, 
                      const int cols, const int rows, 
                      const int maxcolors, int * const colorsP );

colorhash_table
ppm_computecolorhash2(FILE * const ifp,
                      const int cols, const int rows, 
                      const pixval maxval, const int format, 
                      const int maxcolors, int * const colorsP);

int
ppm_lookupcolor(colorhash_table const cht, 
                const pixel *   const colorP );

colorhist_vector
ppm_colorhashtocolorhist(colorhash_table const cht, 
                         int             const maxcolors);

colorhash_table
ppm_colorhisttocolorhash(colorhist_vector const chv, 
                         int              const colors);

int
ppm_addtocolorhash(colorhash_table const cht, 
                   const pixel *   const colorP, 
                   int             const value);

void
ppm_delfromcolorhash(colorhash_table const cht, 
                     const pixel *   const colorP);


colorhash_table
ppm_alloccolorhash(void);

void
ppm_freecolorhash(colorhash_table const cht);


colorhash_table ppm_colorrowtocolorhash ARGS((pixel *colorrow, int ncolors));
pixel * ppm_computecolorrow ARGS((pixel **pixels, int cols, int rows, int maxcolors, int *ncolorsP));
pixel * ppm_mapfiletocolorrow ARGS((FILE *file, int maxcolors, int *ncolorsP, pixval *maxvalP));
void    ppm_colorrowtomapfile ARGS((FILE *ofp, pixel *colormap, int ncolors, pixval maxval));
void    ppm_sortcolorrow (pixel * const colorrow, const int ncolors, 
                          int (*cmpfunc)(pixel *, pixel *) );
int     ppm_addtocolorrow ARGS((pixel *colorrow, int *ncolorsP, int maxcolors, pixel *pixelP));

int
ppm_findclosestcolor(const pixel * const colormap, 
                     int           const ncolors, 
                     const pixel * const pP);

/* standard sort function for ppm_sortcolorrow() */
#define PPM_STDSORT     (int (*)(pixel *, pixel *))0
#endif

#ifdef __cplusplus
}
#endif
