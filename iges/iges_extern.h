
#define		CARDLEN		71 /* length of data portion in Global records */
#define		PARAMLEN	63 /* length of data portion in Parameter records */

extern int do_projection;
extern char eor; /* IGES end of record delimeter */
extern char eof; /* IGES end of field delimeter */
extern char card[256]; /* input buffer, filled by readrec */
extern fastf_t scale,inv_scale; /* IGES file scale factor and inverse */
extern fastf_t conv_factor; /* Conversion factor from IGES file units to mm */
extern mat_t *identity; /* identity matrix */
extern int units; /* IGES file units code */
extern int counter; /* keep track of where we are in the "card" buffer */
extern int pstart; /* record number where parameter section starts */
extern int dstart; /* record number where directory section starts */
extern int totentities; /* total number of entities in the IGES file */
extern int dirarraylen;	/* number of elements in the "dir" array */
extern int reclen; /* IGES file record length (in bytes) */
extern int currec; /* current record number in the "card" buffer */
extern int ntypes; /* Number of different types of IGES entities recognized by
			this code */
extern int brlcad_att_de; /* DE sequence number for BRLCAD attribute
		definition entity */
extern int do_polysolids; /* flag indicating NMG solids should be written as polysolids */
extern FILE *fd; /* file pointer for IGES file */
extern FILE *fdout; /* file pointer for BRLCAD output file */
extern char brlcad_file[]; /* name of brlcad output file (".g" file) */
extern struct iges_directory **dir; /* Directory array */
extern struct reglist *regroot; /* list of regions created from solids of revolution */
extern struct types typecount[]; /* Count of how many entities of each type actually
				appear in the IGES file */
extern char operator[]; /* characters representing operators: 'u', '+', and '-' */
extern struct iges_edge_list *edge_root;
extern struct iges_vertex_list *vertex_root;
extern struct rt_tol tol;
extern char *solid_name;
extern struct file_list *curr_file;
extern struct file_list iges_list;
extern struct name_list *name_root;
