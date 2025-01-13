/*                         P D B - G . C
 * BRL-CAD
 *
 * Copyright (c) 2012-2024 United States Government as represented by
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
/**
 * Protein Data Bank (PDB) is a text file format used for describing
 * the 3D structures of molecules, proteins, and nucleic acids.
 *
 * PDB is a fixed column format and begins with a HEADER line and is
 * followed by other lines with additional information such as name,
 * authors, and experimental conditions.  Each line in the PDB file
 * begins with a 6-char record type followed by data for that record
 * type.  All lines are 80-characters long (followed by \n or \r\n).
 *
 * The header is followed by a series of records, each of which
 * describes a single atom or group of atoms in the molecule. These
 * records include information such as the atom's 3D position chemical
 * identity, and connectivity to other atoms in the molecule.
 *
 * Here's what a water.pdb might look like:
 *
 @code
HEADER    WATER MOLECULE                                                        
ATOM      1  O   HOH     1       0.000   0.000   0.000  1.00  0.00              
ATOM      2  H1  HOH     1       0.957   0.000   0.000  1.00  0.00              
ATOM      3  H2  HOH     1       0.239   0.927   0.000  1.00  0.00              
END                                                                             
 @endcode
 *
 * NOTE: THE TRAILING WHITESPACE IS INTENTIONAL AND REQUIRED FOR PDB.
 *
 * Here's what a caffeine.pdb might look like:
 @code
HEADER    CAFFEINE (1,3,7-TRIMETHYLXANTHINE) FORMULA C8H10N4O2                  
HETATM    1  N1  CAF A   1      -1.250   1.250   0.000  1.00  0.00           N  
HETATM    2  C2  CAF A   1       0.000   1.500   0.000  1.00  0.00           C  
HETATM    3  O2  CAF A   1       1.200   1.300   0.000  1.00  0.00           O  
HETATM    4  N3  CAF A   1       1.500   0.000   0.000  1.00  0.00           N  
HETATM    5  C4  CAF A   1       1.000  -1.000   0.000  1.00  0.00           C  
HETATM    6  C5  CAF A   1       0.000  -1.500   0.000  1.00  0.00           C  
HETATM    7  C6  CAF A   1      -1.000  -1.000   0.000  1.00  0.00           C  
HETATM    8  O6  CAF A   1      -1.800  -2.000   0.000  1.00  0.00           O  
HETATM    9  N7  CAF A   1      -1.500   0.000   0.000  1.00  0.00           N  
HETATM   10  C8  CAF A   1      -2.300   1.000   0.000  1.00  0.00           C  
HETATM   11  H8  CAF A   1      -3.000   1.200   0.000  1.00  0.00           H  
HETATM   12  N9  CAF A   1      -2.900   2.100   0.000  1.00  0.00           N  
HETATM   13  CM1 CAF A   1      -1.800   2.200   0.000  1.00  0.00           C  
HETATM   14 HM1A CAF A   1      -1.000   2.500   0.000  1.00  0.00           H  
HETATM   15 HM1B CAF A   1      -2.400   2.900   0.000  1.00  0.00           H  
HETATM   16 HM1C CAF A   1      -2.200   1.300   0.000  1.00  0.00           H  
HETATM   17  CM3 CAF A   1       2.200   0.500   0.000  1.00  0.00           C  
HETATM   18 HM3A CAF A   1       2.700   1.200   0.000  1.00  0.00           H  
HETATM   19 HM3B CAF A   1       2.800  -0.200   0.000  1.00  0.00           H  
HETATM   20 HM3C CAF A   1       1.900   0.800   0.000  1.00  0.00           H  
HETATM   21  CM7 CAF A   1      -2.300  -0.500   0.000  1.00  0.00           C  
HETATM   22 HM7A CAF A   1      -3.200  -0.700   0.000  1.00  0.00           H  
HETATM   23 HM7B CAF A   1      -1.800  -1.300   0.000  1.00  0.00           H  
HETATM   24 HM7C CAF A   1      -2.800   0.100   0.000  1.00  0.00           H  
END                                                                             
 @endcode
 *
 * The format allows for multiple models of the same molecule to be
 * included in a single file, allowing for the representation of
 * dynamic or flexible structures.  Each molecule starts with the HEADER
 *
 * PDB files can also contain multiple moleclues, each separated by a
 * header record that starts with the keyword "ATOM" or "HETATM".
 * This indicates the beginning of a new molecule and provides info
 * about it such as its name and chain identifier.  The end of the
 * molecule is typically marked by the start of a new header or by
 * EOF.
 *
 * Here's a schema of the PDF file format:
 @code
 Columns | Field             | Example Value        | Required?         | Notes
-----------------------------------------------------------------------------------
   1–6   | Record name       | ATOM / HETATM        | Yes (one or other)| "ATOM " or "HETATM"
   7–11  | Atom serial num   | 1, 2, ...            | Yes               | Usually right-justified
   13–16 | Atom name         | C, CA, O, N, etc.    | Yes               | E.g. " CA ", " O  ", " H1 "
   17    | Alt. loc (altLoc) | ' ', 'A', 'B', ...   | **Optional**      | Only used for alternate conformations
   18–20 | Residue name      | ALA, HOH, CAF, etc.  | Yes               | Typically 3-char code
   22    | Chain ID          | A, B, ' '            | **Required**      | Can be blank if only one chain
   23–26 | Residue seq #     | 1, 2, 345, etc.      | Yes               | Right-justified
   27    | Insertion code    | ' ', 'A', 'B', etc.  | **Optional**      | Only used if there's an insertion
   31–38 | X coordinate      | -12.345              | Yes               | Floating point
   39–46 | Y coordinate      |  23.456              | Yes               | Floating point
   47–54 | Z coordinate      |   5.678              | Yes               | Floating point
   55–60 | Occupancy         | 1.00                 | **Required**      | Default to 1.00 if unknown
   61–66 | B-factor          | 0.00                 | **Required**      | Default to 0.00 if unknown
   77–78 | Element symbol    | C, N, O, Na, ...     | **Required** (v3) | Used to identify the element
   79–80 | Charge            | 2+, 1-, ' '          | **Optional**      | Only used for charged atoms
 @endcode
 */
#include "common.h"

#include "bu/app.h"
#include "bu/log.h"
#include "bu/str.h"
#include "bu/malloc.h"
#include "bu/file.h"
#include "bu/exit.h"

/* how many to allocate per set, not an upper limit */
#define MORE_ATOMS 100000


typedef struct {
    char record_type[7];
    int serial;
    char atom_name[5];
    char alt_loc;
    char res_name[4];
    char chain_id;
    int res_seq;
    char i_code;
    double x;
    double y;
    double z;
    double occupancy;
    double temp_factor;
    char element[3];
    char charge[3];
} pdb_atom;

typedef struct {
    char* header;
    pdb_atom* atoms;
    int num_atoms;
} pdb_data;


static pdb_data*
read_pdb(const char* filename)
{
    FILE* fp;
#define PDB_LINELEN 81
    char line[PDB_LINELEN];
    int num_atoms = 0;
    int num_alloc = 0;
    pdb_atom* atoms = NULL;
    char* header = NULL;

    if (!bu_file_exists(filename, 0)) {
	fprintf(stderr, "ERROR: pdb file [%s] does not exist\n", filename);
	return NULL;
    }

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Failed to open file");
        return NULL;
    }

    num_alloc = MORE_ATOMS;
    atoms = bu_malloc(num_alloc * sizeof(pdb_atom), "pdb_atom alloc");

    while (bu_fgets(line, PDB_LINELEN, fp) != NULL) {
        if (bu_strncasecmp(line, "HEADER", 6) == 0) {
            header = strdup(line);
        } else if (bu_strncasecmp(line, "ATOM", 4) == 0 || bu_strncasecmp(line, "HETATM", 6) == 0) {
            if (num_atoms >= num_alloc) {
		num_alloc += MORE_ATOMS;
		atoms = bu_realloc(atoms, num_alloc * sizeof(pdb_atom), "pdb_atom realloc");
	    }
	    pdb_atom* atom = &atoms[num_atoms];
	    bu_strlcpy(atom->record_type, line, sizeof(atom->record_type));
	    sscanf(&line[6], "%d", &atom->serial);
	    bu_strlcpy(atom->atom_name, &line[12], sizeof(atom->atom_name));
	    atom->alt_loc = line[16];
	    bu_strlcpy(atom->res_name, &line[17], sizeof(atom->res_name));
	    atom->chain_id = line[21];
	    sscanf(&line[22], "%d", &atom->res_seq);
	    atom->i_code = line[26];
	    sscanf(&line[30], "%lf%lf%lf", &atom->x, &atom->y, &atom->z);
	    sscanf(&line[54], "%lf%lf", &atom->occupancy, &atom->temp_factor);
	    bu_strlcpy(atom->element, &line[76], sizeof(atom->element));
	    bu_strlcpy(atom->charge, &line[78], sizeof(atom->charge));
	    num_atoms++;
	}
    }

    fclose(fp);

    pdb_data* data = bu_malloc(sizeof(pdb_data), "pdb_data alloc");

    data->header = header;
    data->atoms = atoms;
    data->num_atoms = num_atoms;

    return data;
}


static void
write_g(char *filename, struct pdb_data *pdbp)
{
    struct rt_wdb *db_fp;

    if (!pdbp || !filename) {
        fprintf(stderr, "ERROR: Invalid PDB data or filename.\n");
        return;
    }

    db_fp = wdb_fopen(filename);
    if (db_fp == NULL) {
        perror(filename);
        return;
    }

    mk_id_units(db_fp, "PDB Geometry Database", "mm");

    struct wmember wm_hd;
    BU_LIST_INIT(&wm_hd.l);

    /* iterate through atoms in our PDB structure */
    for (int i = 0; i < pdbp->num_atoms; ++i) {
        struct pdb_atom *atom = &pdbp->atoms[i];

        /* create a sphere for the atom */
        point_t center;
        VSET(center, atom->x, atom->y, atom->z);

        char sphere_name[64];
        snprintf(sphere_name, sizeof(sphere_name), "atom_%d.s", atom->serial);

        mk_sph(db_fp, sphere_name, center, 1.0); // Default radius = 1.0 mm

        /* add it to our combination list */
        (void)mk_addmember(sphere_name, &wm_hd.l, NULL, WMOP_UNION);
    }

    /* create a combination for all atoms */
    mk_lcomb(db_fp, "all_atoms.r", &wm_hd, 1, NULL, NULL, NULL, 0);

    db_close(db_fp->dbip);
}


/* format of command: pdb-g pdbfile gfile */
int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);

    if (argc < 2) {
	bu_log("Usage: %s file.pdb [file.g]\n", argv[0]);
	bu_exit(1, "ERROR: No pdb filename given.\n");
    }

    /* open PDB file */
    struct pdb_data pdb = read_pdb(argv[1]);

    write_g(argv[2], &pdb);

    if (pdb)
	bu_free(pdb->atoms, "pdb_atom free");
    bu_free(pdb, "pdb_data free");

    return 0;
}


/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
