/*                     D B _ D I R _ I N F O . C
 * BRL-CAD
 *
 * Published in 2020 by the United States Government.
 * This work is in the public domain.
 *
 */
/** @file dir_info.c
 *
 * This is a stub file, whose purpose is to provide the Docbook logic with an
 * add_executable build target that can be passed to scripts in order to decode
 * at runtime what the location of BR-CAD's build directory is.
 *
 * Unlike most other BRL-CAD logic, Docbook building may be done without any
 * software build targets needing to be processed if the necessary system
 * tools are in place.  Since it is otherwise difficult to reliably get the
 * location of the current build directory in multiconfig build tools, this
 * file is used to provide a db_dir_info build target to reference in CMake.
 */

int main(void) {
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
