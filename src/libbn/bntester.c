/*                       B N T E S T E R . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
#include <errno.h>

#include "bu.h"
#include "vmath.h"
#include "bn.h"


int
parse_case(char *buf_p, int *i, long *l, double *d, unsigned long *u, char *fmt_str, unsigned long line_num, FILE *stream)
{
    int i_idx = 0;
    int l_idx = 0;
    int d_idx = 0;
    int u_idx = 1; /* start index at 1 since function number is stored at 0 */
    int idx;
    int fmt_str_len;
    char *endp;
    long int l_tmp;

    fmt_str_len = strlen(fmt_str);

    for (idx = 0 ; idx < fmt_str_len ; idx++) {
        buf_p = strtok(NULL, ", ");

        /* The variable buf_p should never become NULL since the for loop
         * will exit just before we run out of data if all the required
         * data is supplied in the data file.
         */
        if (buf_p == NULL) {
            (void)fprintf(stream, "ERROR: Missing parameter(s) for test case on line %lu, skipping test case.\n", line_num);
            return EXIT_FAILURE;
        }

        errno = 0;
        switch (fmt_str[idx]) {
            case 'd' : /* double */
                d[d_idx] = strtod(buf_p, &endp);
                if (errno) {
                    (void)fprintf(stream, "Convert to double failed, function %lu test case on line %lu parameter %d error msg: '%s' string '%s'\n",
				  u[0], line_num, idx+2, strerror(errno), buf_p);
                    return EXIT_FAILURE;
                }
                if ((*endp != '\0') || (buf_p == endp)) {
                    (void)fprintf(stream, "Convert to double failed, function %lu test case on line %lu parameter %d string '%s'\n",
				  u[0], line_num, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                d_idx++;
                break;
            case 'l' : /* long int */
                l[l_idx] = strtol(buf_p, &endp, 10);
                if (errno) {
                    (void)fprintf(stream, "Convert to long int failed, function %lu test case on line %lu parameter %d error msg: '%s' string '%s'\n",
				  u[0], line_num, idx+2, strerror(errno), buf_p);
                    return EXIT_FAILURE;
                }
                if ((*endp != '\0') || (buf_p == endp)) {
                    (void)fprintf(stream, "Convert to long int failed, function %lu test case on line %lu parameter %d string '%s'\n",
				  u[0], line_num, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                l_idx++;
                break;
            case 'i' : /* int */
                l_tmp = strtol(buf_p, &endp, 10);
                if (errno) {
                    (void)fprintf(stream, "Convert to int failed, function %lu test case on line %lu parameter %d error msg: '%s' string '%s'\n",
				  u[0], line_num, idx+2, strerror(errno), buf_p);
                    return EXIT_FAILURE;
                }
                if ((*endp != '\0') || (buf_p == endp)) {
                    (void)fprintf(stream, "Convert to int failed, function %lu test case on line %lu parameter %d string '%s'\n",
				  u[0], line_num, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                if (l_tmp > INT_MAX || l_tmp < INT_MIN) {
                    (void)fprintf(stream, "Convert to int failed (under/over flow), function %lu test case on line %lu parameter %d string '%s'\n",
				  u[0], line_num, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                i[i_idx] = (int)l_tmp;
                i_idx++;
                break;
            case 'u' : /* unsigned long */
                u[u_idx] = strtoul(buf_p, &endp, 10);
                if (errno) {
                    (void)fprintf(stream, "Convert to unsigned long int failed, function %lu test case on line %lu parameter %d error msg: '%s' string '%s'\n",
				  u[0], line_num, idx+2, strerror(errno), buf_p);
                    return EXIT_FAILURE;
                }
                if ((*endp != '\0') || (buf_p == endp) || (strchr(buf_p, '-') != '\0')) {
                    (void)fprintf(stream, "Convert to unsigned long int failed, function %lu test case on line %lu parameter %d string '%s'\n",
				  u[0], line_num, idx+2, buf_p);
                    return EXIT_FAILURE;
                }
                u_idx++;
                break;
            default:
                (void)fprintf(stream, "INTERNAL ERROR: Unknown data type '%c' for function %lu test case on line %lu, skipping test case.\n",
			      fmt_str[idx], u[0], line_num);
                return EXIT_FAILURE;
                break;
        } /* End of data format switch */
    } /* End of for loop traversing data format string */

    return 0;
}


int
main(int argc, char **argv)
{
    static char *usage="Usage: bntester [-l test_case_line_number] [-f function_number] -i input_file [-o output_file]\n";

    /* static to prevent longjmp clobber warning */
    static FILE *stream = NULL;
    static unsigned long line_num = 0; 
    static unsigned long failed_cnt = 0;
    static unsigned long bomb_cnt = 0;
    static unsigned long success_cnt = 0;
    static int ret = 0;

    char buf[BUFSIZ];
    FILE *fp_in = NULL; 
    char *endp = NULL;
    int string_length;
    int argv_idx;
    int c;
    char dt_fmt[50];  /* data type format string */
    char *buf_p1;
    char *buf_p;
    struct bn_tol tol;

    /* command line parameters */
    static unsigned long test_case_line_num = 0; /* static due to longjmp */
    static unsigned long function_num = 0; /* static due to longjmp */
    char input_file_name[BUFSIZ] = {0};
    char output_file_name[BUFSIZ] = {0};

    /* function parameter arrays */
    int i[50] = {0};
    long l[50] = {0};
    double d[50] = {0.0};
    unsigned long u[50] = {0};

    /* boolean variables */
    static int output_file_name_defined = 0; /* static due to longjmp */
    static int process_single_test_case = 0; /* static due to longjmp */
    static int process_single_function = 0; /* static due to longjmp */
    int input_file_name_defined = 0;
    int valid_function_number = 0;
    int process_test_case = 0;
    int early_exit = 0;
    int found_eof = 0;

    /* set initial values in tol structure */
    tol.magic = BN_TOL_MAGIC;
    tol.dist = 0.0005;
    tol.dist_sq = tol.dist * tol.dist;
    tol.perp = 1e-6;
    tol.para = 1.0 - tol.perp;


    if (argc < 2) {
        bu_log("Too few parameters, %d specified, at least 1 required\n", argc - 1);
        bu_exit(EXIT_FAILURE, usage);
    }

    while ((c = bu_getopt(argc, argv, "l:f:i:o:")) != -1) {
        switch (c) {
            case 'l': /* test case line number */
                errno = 0;
                test_case_line_num = strtoul(bu_optarg, &endp, 10);
                if (errno) {
                    bu_log("Invalid test case line number '%s' '%s'\n", bu_optarg, strerror(errno));
                    bu_exit(EXIT_FAILURE, usage);
                }
                if ((*endp != '\0') || (bu_optarg == endp) || (strchr(bu_optarg, '-') != '\0')) {
                    bu_log("Invalid test case line number '%s'\n", bu_optarg);
                    bu_exit(EXIT_FAILURE, usage);
                }
                process_single_test_case = 1;
                break;
            case 'f': /* function number */
                errno = 0;
                function_num = strtoul(bu_optarg, &endp, 10);
                if (errno) {
                    bu_log("Invalid function number '%s' '%s'\n", bu_optarg, strerror(errno));
                    bu_exit(EXIT_FAILURE, usage);
                }
                if ((*endp != '\0') || (bu_optarg == endp) || (strchr(bu_optarg, '-') != '\0')) {
                    bu_log("Invalid function number '%s'\n", bu_optarg);
                    bu_exit(EXIT_FAILURE, usage);
                }
                process_single_function = 1;
                break;
            case 'i': /* input file name */
                string_length = strlen(bu_optarg);
                if (string_length >= BUFSIZ) {
                    bu_log("Input file name too long, length was %d but must be less than %d\n",
			   string_length, BUFSIZ);
                    bu_exit(EXIT_FAILURE, usage);
                }
                (void)strcpy(input_file_name, bu_optarg);
                input_file_name_defined = 1;
                break;
            case 'o': /* output file name */
                string_length = strlen(bu_optarg);
                if (string_length >= BUFSIZ) {
                    bu_log("Output file name too long, length was %d but must be less than %d\n",
			   string_length, BUFSIZ);
                    bu_exit(EXIT_FAILURE, usage);
                }
                (void)strcpy(output_file_name, bu_optarg);
                output_file_name_defined = 1;
                break;
            default:
                bu_log("Invalid option '%c'.\n", c);
                bu_exit(EXIT_FAILURE, usage);
                break;
        }
    }

    if (process_single_test_case && process_single_function) {
        bu_log("Can not specify both test case line number and function number.\n");
        early_exit = 1;
    }

    if (!input_file_name_defined) {
        bu_log("Input file name is required but was not specified.\n");
        early_exit = 1;
    }

    if (early_exit) {
        bu_exit(EXIT_FAILURE, usage);
    }

    if ((fp_in = fopen(input_file_name, "r")) == NULL) {
        bu_log("Cannot open input file (%s)\n", input_file_name);
        return EXIT_FAILURE;
    }


    if (output_file_name_defined) {
        if ((stream = fopen(output_file_name, "w")) == NULL) {
            bu_log("Cannot create output file (%s)\n", output_file_name);
            if (fclose(fp_in) != 0) {
                bu_log("Unable to close input file.\n");
            }
            return EXIT_FAILURE;
        }
    } else {
        stream = stderr;
    }

    /* all output after this point is sent to stream */

    (void)fprintf(stream, "Command line parameters: bntester ");
    for (argv_idx = 1 ; argv_idx < argc ; argv_idx++) {
        (void)fprintf(stream, "%s ", argv[argv_idx]);
    }
    (void)fprintf(stream, "\n");

    if (process_single_test_case) {
        (void)fprintf(stream, "Processing only test case on line number: %lu\n", test_case_line_num);
    }

    if (process_single_function) {
        (void)fprintf(stream, "Processing all test cases for function number: %lu\n", function_num);
    }

    if (!process_single_test_case && !process_single_function) {
        (void)fprintf(stream, "Processing all test cases.\n");
    }


    while (!found_eof) {
        if (line_num == ULONG_MAX) {
            (void)fprintf(stream, "ERROR: Input data file exceeded max %lu number of lines.\n", ULONG_MAX);
            if (fclose(fp_in) != 0) {
                (void)fprintf(stream, "Unable to close input file.\n");
            }
            if (output_file_name_defined) {
                if (fclose(stream) != 0) {
                    bu_log("Unable to close output file.\n");
                }
            }
            return EXIT_FAILURE;
        }
        line_num++;
	if (fgets(buf, BUFSIZ, fp_in) == NULL) {
	    if (feof(fp_in)) {
		found_eof = 1;
	    } else if (ferror(fp_in)) {
		perror("ERROR: Problem reading file, system error message");
		if (fclose(fp_in) != 0) {
		    (void)fprintf(stream, "Unable to close input file.\n");
		}
		return EXIT_FAILURE;
	    } else {
		perror("Oddness reading input file");
		return EXIT_FAILURE;
	    }
	} else {
            /* Skip input data file lines which start with a '#' character
             * or a new line character.
             */
            if ((buf[0] != '#') && (buf[0] != '\n')) {
                buf_p1 = strtok(buf, "\n");
                buf_p = strtok(buf_p1, ", ");

                /* The 1st parameter of the test case is alway an unsigned
                 * long int which represents the function number. This logic
                 * validates the test case function number to ensure it is
                 * an unsigned long int.
                 */
                valid_function_number = 1;
                errno = 0;
                u[0] = strtoul(buf_p, &endp, 10);
                if (errno) {
                    (void)fprintf(stream, "Read function number failed, line %lu error msg: '%s' string '%s'\n",
				  line_num, strerror(errno), buf_p);
                    valid_function_number = 0;
                } else if ((*endp != '\0') || (buf_p == endp) || (strchr(buf_p, '-') != '\0')) {
                    (void)fprintf(stream, "Read function number failed, line %lu string '%s'\n", line_num, buf_p);
                    valid_function_number = 0;
                }

                /* This logic restricts processing of the test case(s) to
                 * only those specified by the bntester input parameters.
                 */
                process_test_case = 0;
                if (valid_function_number && process_single_test_case && (test_case_line_num == line_num)) {
                    process_test_case = 1;
                } else if (valid_function_number && process_single_function && (function_num == u[0])) {
                    process_test_case = 1;
                } else if (valid_function_number && !process_single_test_case && !process_single_function) {
                    process_test_case = 1;
                }

                if (process_test_case) {
                    /* Each case within this switch corresponds to each
                     * function to be tested.
                     */
                    switch (u[0]) {
                        case 1: /* function 'bn_distsq_line3_pt3' */
                            (void)strcpy(dt_fmt, "dddddddddd"); /* defines parameter data types */
                            if (parse_case(buf_p, i, l, d, u, dt_fmt, line_num, stream)) {
                                /* Parse failed, skipping test case */
                                ret = 1;
                            } else {
                                double result;
                                if (!BU_SETJUMP) {
                                    /* try */
                                    result = bn_distsq_line3_pt3(&d[0], &d[3], &d[6]); 
                                    if (!NEAR_EQUAL(result, d[9], VUNITIZE_TOL)) {
                                        ret = 1;
                                        failed_cnt++;
                                        (void)fprintf(stream, "Failed function %lu test case on line %lu expected = %.15f result = %.15f\n",
						      u[0], line_num, d[9], result); 
                                    } else {
                                        success_cnt++;
                                    }
                                } else {
                                    /* catch */
                                    BU_UNSETJUMP;
                                    ret = 1;
                                    bomb_cnt++;
                                    (void)fprintf(stream, "Failed function %lu test case on line %lu bu_bomb encountered.\n", u[0], line_num); 
                                } BU_UNSETJUMP;
                            }
                            break;
                        case 2: /* function 'bn_2line3_colinear' */
                            (void)strcpy(dt_fmt, "ddddddddddddduddddi");
                            if (parse_case(buf_p, i, l, d, u, dt_fmt, line_num, stream)) {
                                /* Parse failed, skipping test case */
                                ret = 1;
                            } else {
                                int result;
                                if (!BU_SETJUMP) {
                                    /* try */
                                    tol.magic = u[1];
                                    tol.dist = d[13];
                                    tol.dist_sq = d[14];
                                    tol.perp = d[15];
                                    tol.para = d[16];
                                    result = bn_2line3_colinear(&d[0], &d[3], &d[6], &d[9], d[12], &tol);
                                    if (result != i[0]) {
                                        ret = 1;
                                        failed_cnt++;
                                        (void)fprintf(stream, "Failed function %lu test case on line %lu expected = %d result = %d\n",
						      u[0], line_num, i[0], result); 
                                    } else {
                                        success_cnt++;
                                    }
                                } else {
                                    /* catch */
                                    BU_UNSETJUMP;
                                    ret = 1;
                                    bomb_cnt++;
                                    (void)fprintf(stream, "Failed function %lu test case on line %lu bu_bomb encountered.\n", u[0], line_num); 
                                } BU_UNSETJUMP;
                            }
                            break;
                        case 3: /* function 'bn_isect_line3_line3' */
                            (void)strcpy(dt_fmt, "dddddddddddddduddddi");
                            if (parse_case(buf_p, i, l, d, u, dt_fmt, line_num, stream)) {
                                /* Parse failed, skipping test case */
                                ret = 1;
                            } else {
                                int result;
                                double t_out = 0.0;
                                double u_out = 0.0;
                                int t_fail = 0;
                                int u_fail = 0;

                                if (!BU_SETJUMP) {
                                    /* try */
                                    tol.magic = u[1];
                                    tol.dist = d[14];
                                    tol.dist_sq = d[15];
                                    tol.perp = d[16];
                                    tol.para = d[17];
                                    result = bn_isect_line3_line3(&t_out, &u_out, &d[2], &d[5], &d[8], &d[11], &tol);
                                    if (result != i[0]) {
                                        ret = 1;
                                        failed_cnt++;
                                        (void)fprintf(stream, "Failed function %lu test case on line %lu expected = %d result = %d\n",
						      u[0], line_num, i[0], result); 
                                    } else if (result == 0) {
                                        if (!NEAR_EQUAL(t_out, d[0], tol.dist)) {
                                            ret = 1;
                                            failed_cnt++;
                                            (void)fprintf(stream, "Failed function %lu test case on line %lu result = %d expected t = %.15f result t = %.15f\n",
							  u[0], line_num, result, d[0], t_out); 
                                        } else {
                                            success_cnt++;
                                        }
                                    } else if (result == 1) {
                                        t_fail = !NEAR_EQUAL(t_out, d[0], tol.dist);
                                        u_fail = !NEAR_EQUAL(u_out, d[1], tol.dist);
                                        if (t_fail) {
                                            (void)fprintf(stream, "Failed function %lu test case on line %lu result = %d expected t = %.15f result t = %.15f\n",
							  u[0], line_num, result, d[0], t_out); 
                                        }
                                        if (u_fail) {
                                            (void)fprintf(stream, "Failed function %lu test case on line %lu result = %d expected u = %.15f result u = %.15f\n",
							  u[0], line_num, result, d[1], u_out); 
                                        }
                                        if (t_fail || u_fail) {
                                            ret = 1;
                                            failed_cnt++;
                                        } else {
                                            /* No other output to validate when result matches expected and
                                             * result is not 0 and not 1. 
                                             */
                                            success_cnt++;
                                        }
                                    } else {
                                        success_cnt++;
                                    }
                                } else {
                                    /* catch */
                                    BU_UNSETJUMP;
                                    ret = 1;
                                    bomb_cnt++;
                                    (void)fprintf(stream, "Failed function %lu test case on line %lu bu_bomb encountered.\n", u[0], line_num); 
                                } BU_UNSETJUMP;
                            }
                            break;
                        case 4: /* function 'bn_isect_lseg3_lseg3' */
                            (void)strcpy(dt_fmt, "dddddddddddddduddddi");
                            if (parse_case(buf_p, i, l, d, u, dt_fmt, line_num, stream)) {
                                /* Parse failed, skipping test case */
                                ret = 1;
                            } else {
                                int result;
                                double dist[2] = {0.0, 0.0};
                                int d0_fail = 0;
                                int d1_fail = 0;

                                if (!BU_SETJUMP) {
                                    /* try */
                                    tol.magic = u[1];
                                    tol.dist = d[14];
                                    tol.dist_sq = d[15];
                                    tol.perp = d[16];
                                    tol.para = d[17];
                                    result = bn_isect_lseg3_lseg3(&dist[0], &d[2], &d[5], &d[8], &d[11], &tol);
                                    if (result != i[0]) {
                                        ret = 1;
                                        failed_cnt++;
                                        (void)fprintf(stream, "Failed function %lu test case on line %lu expected = %d result = %d\n",
						      u[0], line_num, i[0], result); 
                                    } else if (result == 0 || result == 1) {
                                        d0_fail = !NEAR_EQUAL(dist[0], d[0], VUNITIZE_TOL);
                                        d1_fail = !NEAR_EQUAL(dist[1], d[1], VUNITIZE_TOL);
                                        if (d0_fail) {
                                            (void)fprintf(stream, "Failed function %lu test case on line %lu result = %d expected dist[0] = %.15f result dist[0] = %.15f\n",
							  u[0], line_num, result, d[0], dist[0]); 
                                        }
                                        if (d1_fail) {
                                            (void)fprintf(stream, "Failed function %lu test case on line %lu result = %d expected dist[1] = %.15f result dist[1] = %.15f\n",
							  u[0], line_num, result, d[1], dist[1]); 
                                        }
                                        if (d0_fail || d1_fail) {
                                            ret = 1;
                                            failed_cnt++;
                                        } else {
                                            /* No other output to validate when result matches expected and
                                             * result is not 0 and not 1. 
                                             */
                                            success_cnt++;
                                        }
                                    } else {
                                        success_cnt++;
                                    }
                                } else {
                                    /* catch */
                                    BU_UNSETJUMP;
                                    ret = 1;
                                    bomb_cnt++;
                                    (void)fprintf(stream, "Failed function %lu test case on line %lu bu_bomb encountered.\n", u[0], line_num); 
                                } BU_UNSETJUMP;
                            }
                            break;
                        default:
                            (void)fprintf(stream, "ERROR: Unknown function number %lu test case on line %lu, skipping test case.\n", u[0], line_num);
                            return EXIT_FAILURE;
                            break;
                    } /* End of function number switch */
                }
            } /* End of if statement skipping lines starting with '#' or new line */    
        }
    } /* End of while loop reading lines from data file */

    (void)fprintf(stream, "Summary: %lu total test cases success.\n", success_cnt);
    (void)fprintf(stream, "Summary: %lu total test cases failed.\n", failed_cnt);
    (void)fprintf(stream, "Summary: %lu total test cases bomb.\n", bomb_cnt);
 
    if (output_file_name_defined) {
        bu_log("Summary: %lu total test cases success.\n", success_cnt);
        bu_log("Summary: %lu total test cases failed.\n", failed_cnt);
        bu_log("Summary: %lu total test cases bomb.\n", bomb_cnt);
    }

    (void)fprintf(stream, "Done.\n");

    if (output_file_name_defined) {
        bu_log("Done.\n");
    }

    if (fclose(fp_in) != 0) {
        (void)fprintf(stream, "Unable to close input file.\n");
    }

    if (output_file_name_defined) {
        if (fclose(stream) != 0) {
            bu_log("Unable to close output file.\n");
        }
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
