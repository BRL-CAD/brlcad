/* These declarations were supposed to be in the libfloyd.h file in the ilbm
   package, but that file was missing, so I made them up myself.  
   - Bryan 01.03.10.
*/


#ifdef __cplusplus
extern "C" {
#endif
#if 0
} /* to fake out automatic code indenters */
#endif

struct ppm_fs_info {
    /* thisXerr and nextXerr are dynamically allocated arrays each of whose
       dimension is the width of the image plus 2
       */
    long * thisrederr;
    long * thisgreenerr;
    long * thisblueerr;
    long * nextrederr;
    long * nextgreenerr;
    long * nextblueerr;
    int lefttoright;
    int cols;
    pixval maxval;
    int flags;
    pixel * pixrow;
    int col_end;
    pixval red, green, blue;
};

typedef struct ppm_fs_info ppm_fs_info;

/* Bitmasks for ppm_fs_info.flags */
#define FS_RANDOMINIT 0x01
#define FS_ALTERNATE  0x02

ppm_fs_info *
ppm_fs_init(unsigned int const cols,
            pixval       const maxval,
            unsigned int const flags);

void
ppm_fs_free(ppm_fs_info *fi);

int
ppm_fs_startrow(ppm_fs_info *fi, pixel *pixrow);

int
ppm_fs_next(ppm_fs_info *fi, int col);

void
ppm_fs_endrow(ppm_fs_info *fi);

void
ppm_fs_update(    ppm_fs_info *fi, int col, pixel *pP);


void
ppm_fs_update3(ppm_fs_info * const fi, 
               int           const col, 
               pixval        const r, 
               pixval        const g, 
               pixval        const b);

#ifdef __cplusplus
}
#endif
