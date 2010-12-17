/*                       B N T E S T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2010 United States Government as represented by
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

#include "common.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


static char *usage="Usage: bntester [-c test_case_number] [-f function_number] -i input_file -o output_file\n";

int
parse_case(char *buf_p, int *i, long *l, double *d, unsigned long *u, char *fmt_str, int line_num, int case_idx)
{
    int i_idx;
    int l_idx;
    int d_idx;
    int u_idx;
    int idx;
    int fmt_str_len;
    char *endp;

    fmt_str_len = strlen(fmt_str);

    for (idx = 0 ; idx < fmt_str_len ; idx++) {
        buf_p = strtok(NULL, ",");

        if ((buf_p == NULL) && (idx != fmt_str_len-1)) {
            bu_log("missing parameter(s) for test case %d on line %d\n", case_idx, line_num);
            return EXIT_FAILURE;
        }

        switch (fmt_str[idx]) {
            case 'd' : /* double */
                d[d_idx] = strtod(buf_p, &endp);
                if (!((buf_p != endp) && (*endp == '\0'))) {
                    /* 2 is added to idx since the user numbers the parameters
                     * starting with 1 but internally they start at 0, also idx
                     * starts a the 2nd parameter in the data file since the first
                     * parameter is always the function number, so we need to add
                     * again to the idx to correspond to the parameter in the data 
                     * file
                     */
                    bu_log("Convert to double failed, line %d test case %d parameter %d string '%s'\n",
                           line_num, case_idx, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                d_idx++;
                break;
            case 'l' : /* long int */
                l[l_idx] = strtol(buf_p, &endp, 10);
                if (!((buf_p != endp) && (*endp == '\0'))) {
                    bu_log("Convert to long int failed, line %d test case %d parameter %d string '%s'\n",
                           line_num, case_idx, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                l_idx++;
                break;
            default:
                bu_log("unknown data type '%c'\n", fmt_str[idx]);
                return EXIT_FAILURE;
                break;
        } /* end of data format switch */
    } /* end of for loop traversing data format string */

    return 0;
}

int
main(int argc, char **argv)
{
    char buf[BUFSIZ];
    char input_file_name[BUFSIZ];
    char output_file_name[BUFSIZ];
    int input_file_name_defined = 0;
    int output_file_name_defined = 0;
    FILE *fp_in; 
    FILE *fp_out;
    char *endp;
    long int case_num;
    int process_single_case = 0;
    int function_num_defined = 0;
    long int function_num;
    int string_length;
    char c;
    int early_exit = 0;
    int found_eof = 0;
    int i[50];
    long l[50];
    double d[50];
    unsigned long u[50];
    char io_fmt[50];  /* input-output format string */
    char dt_fmt[50];  /* data type format string */
    int ret = 0;

    char *buf_p1;
    char *buf_p;
    int case_idx = 0; 
    int line_num = 0; 
    struct bn_tol tol;

    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1.0 - tol.perp;


    if (argc < 3) {
        bu_log("Too few parameters, %d specified, at least 2 required\n", argc - 1);
        bu_exit(EXIT_FAILURE, usage);
    }

    while ((c = bu_getopt(argc, argv, "c:f:i:o:")) != EOF) {
        switch (c) {
            case 'c': /* test case number */
                case_num = strtol(bu_optarg, &endp, 10);
                if ((bu_optarg != endp) && (*endp == '\0')) {
                    /* convert to long integer success */
                    process_single_case = 1;
                } else {
                    /* convert to long integer failed */
                    bu_exit(EXIT_FAILURE, usage);
                }
                break;
            case 'f': /* function number */
                function_num = strtol(bu_optarg, &endp, 10);
                if ((bu_optarg != endp) && (*endp == '\0')) {
                    /* convert to long integer success */
                    function_num_defined = 1;
                } else {
                    /* convert to long integer failed */
                    bu_exit(EXIT_FAILURE, usage);
                }
                break;
            case 'i': /* input file name */
                if (string_length = strlen(bu_optarg) >= BUFSIZ) {
                    bu_log("Input file name too long, length was %d but must be less than %d\n",
                            string_length, BUFSIZ);
                    bu_exit(EXIT_FAILURE, usage);
                }
                strcpy(input_file_name,bu_optarg);
                input_file_name_defined = 1;
                break;
            case 'o': /* output file name */
                if (string_length = strlen(bu_optarg) >= BUFSIZ) {
                    bu_log("Output file name too long, length was %d but must be less than %d\n",
                            string_length, BUFSIZ);
                    bu_exit(EXIT_FAILURE, usage);
                }
                strcpy(output_file_name,bu_optarg);
                output_file_name_defined = 1;
                break;
            default:
                bu_log("Invalid option '%c'.\n", c);
                bu_exit(EXIT_FAILURE, usage);
                break;
        }
    }

    if (process_single_case && function_num_defined) {
        bu_log("Can not specify both test case number and function number.\n");
        early_exit = 1;
    }

    if (!input_file_name_defined) {
        bu_log("Input file name is required but was not specified.\n");
        early_exit = 1;
    }
    if (!output_file_name_defined) {
        bu_log("Output file name is required but was not specified.\n");
        early_exit = 1;
    }

    if (early_exit) {
        bu_exit(EXIT_FAILURE, usage);
    }

    bu_log("Using input path and file name: '%s'\n", input_file_name);
    bu_log("Using output path and file name: '%s'\n", output_file_name);

    if (process_single_case) {
        bu_log("Processing only test case: '%d'\n", case_num);
    }

    if (function_num_defined) {
        bu_log("Processing all test cases for function number: '%ld'\n", function_num);
    }

    if (!process_single_case && !function_num_defined) {
        bu_log("Processing all test cases.\n");
    }

    if ((fp_in = fopen(input_file_name, "r")) == NULL) {
        bu_log("Cannot open input file (%s)\n", input_file_name);
        return EXIT_FAILURE;
    }

    if ((fp_out = fopen(output_file_name, "w")) == NULL) {
        bu_log("Cannot create output file (%s)\n", output_file_name);
        if (fclose(fp_in) != 0) {
            bu_log("Unable to close input file.\n");
        }
        return EXIT_FAILURE;
    }

    while (!found_eof) {
        line_num++;
        (void)fgets(buf, BUFSIZ, fp_in);
        if (feof(fp_in)) {
            if (ferror(fp_in)) {
                perror("ERROR: Problem reading file, system error message");
                fclose(fp_in);
                return EXIT_FAILURE;
            } else {
                found_eof = 1;
            }
        } else {
            if ((buf[0] != '#') && (buf[0] != '\n')) {
                case_idx++;
                buf_p1 = strtok(buf, "\n");
                buf_p = strtok(buf_p1, ",");

                /* the 1st parameter is alway a long int which represents the function number */
                l[0] = strtol(buf_p, &endp, 10);
                if (!((bu_optarg != endp) && (*endp == '\0'))) {
                    /* convert to long integer failed */
                    bu_log("read function number failed, line number '%d' case number '%d' string '%s'\n",
                           line_num, case_idx, buf_p);
                    if (fclose(fp_in) != 0) {
                        bu_log("Unable to close input file.\n");
                    }
                    return EXIT_FAILURE;
                }

                    switch (l[0]) {
                        case 1: /* function number 'bn_distsq_line3_pt3' */

                            /* data type string indicating data type of
                             * function parameters
                             */
                            strcpy(dt_fmt, "dddddddddd");

                            if (parse_case(buf_p, i, l, d, u, dt_fmt, line_num, case_idx)) {
                                bu_log("skipped line %d test case %d\n", line_num, case_idx); 
                                ret = 1;
                            } else {
                                double result;

                                result = bn_distsq_line3_pt3(&d[0], &d[3], &d[6]); 
                                if (!NEAR_ZERO(result - d[9], SMALL_FASTF)) {
                                    ret = 1;
                                    bu_log("failed test case %d on line %d expected = %.15lf result = %.15lf\n",
                                           case_idx, line_num, d[9], result); 
                                } else {
                                    bu_log("success test case %d on line %d expected = %.15lf result = %.15lf\n",
                                           case_idx, line_num, d[9], result); 
                                }
                            }

                            break;
                        default:
                            bu_log("unknown function number '%ld'\n", l[0]);
                            return EXIT_FAILURE;
                            break;
                    } /* end of function number switch */
            } /* end of if statement skipping lines start with '#' */    
        }
    } /* end of while loop reading lines from data file */
 
    if (fclose(fp_in) != 0) {
        bu_log("Unable to close input file.\n");
    }
    if (fclose(fp_out) != 0) {
        bu_log("Unable to close output file.\n");
    }

    exit(ret);
}

/** @} */
/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
