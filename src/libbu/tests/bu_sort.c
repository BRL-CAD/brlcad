/*                       B U _ S O R T . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2013 United States Government as represented by
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
#include "bu.h"
#include "bn.h"
#include "string.h"


/* sort from small to big for unsigned int */
int
comp_1(const void *num1, const void *num2, void *UNUSED(arg))
{
    return (*((unsigned int *)num1) - *((unsigned int *)num2));
}


/* sort from small to big for fastf_t */
int
comp_2(const void *num1, const void *num2, void *UNUSED(arg))
{
    if (*(fastf_t *)num1 > *(fastf_t *)num2)
	return 1;
    else if ((*(fastf_t *)num1 < *(fastf_t *)num2))
	return -1;
    return 0;
}


/* sort strings based on ASCII-table */
int
comp_3(const void *str1, const void *str2, void *UNUSED(arg))
{
    return strcmp((char *)str1, (char *)str2);
}


/* sort fastf_t's by their distance to cmp */
int
comp_4(const void *num1, const void *num2, void *cmp)
{
    if (abs(*(fastf_t *)num1 - *(fastf_t *)cmp) > abs((*(fastf_t *)num2)- *(fastf_t *)cmp))
	return 1;
    else if (abs(*(fastf_t *)num1 - *(fastf_t *)cmp) < abs((*(fastf_t *)num2)- *(fastf_t *)cmp))
	return -1;
    return 0;
}


int
main()
{
    unsigned int arg_1[6] = {5, 2, 6, -15, 168, 3};
    unsigned int exp_1[6] = {-15, 2, 3, 5, 6, 168};
    unsigned int arg_2[8] = {56, 4, 7, 156, 2, 0, 23, 8};
    unsigned int exp_2[8] = {0, 2, 4, 7, 8, 23, 56, 156};
    fastf_t arg_3[5] = {5.5, 3.8, -5.5, 1, -7};
    fastf_t exp_3[5] = {-7, -5.5, 1, 3.8, 5.5};
    fastf_t arg_4[7] = {7.42, -5.2, -5.9, 7.36, 7.0, 0, 7.36};
    fastf_t exp_4[7] = {-5.9, -5.2, 0, 7.0, 7.36, 7.36, 7.42};
    char arg_5[4][256] = {"Zfg", "ZFg", "azf", "bzf"};
    char exp_5[4][256] = {"ZFg", "Zfg", "azf", "bzf"};
    char arg_6[3][256] = {"test", "BAB", "aab"};
    char exp_6[3][256] = {"BAB", "aab", "test"};
    fastf_t cmp_7 = -2;
    fastf_t arg_7[9] = {-3, 7, -9, 34, 33, -34, 0, -12, 6};
    fastf_t exp_7[9] = {-3, 0, -9, 6, 7, -12, -34, 33, 34};
    fastf_t cmp_8 = 3;
    fastf_t arg_8[5] = {-5, 23, 5.5, 0, 2};
    fastf_t exp_8[5] = {2, 5.5, 0, -5, 23};
    int i;

    bu_sort(&arg_1, 6, sizeof(int), comp_1, NULL);
    for (i = 0; i < 6; i++)
	if (arg_1[i] != exp_1[i])
	    return 1;

    bu_sort(&arg_2, 8, sizeof(int), comp_1, NULL);
    for (i = 0; i < 8; i++)
	if (arg_2[i] != exp_2[i])
	    return 1;

    bu_sort(&arg_3, 5, sizeof(fastf_t), comp_2, NULL);
    for (i = 0; i < 5; i++)
	if (!EQUAL(arg_3[i], exp_3[i]))
	    return 1;

    bu_sort(&arg_4, 7, sizeof(fastf_t), comp_2, NULL);
    for (i = 0; i < 7; i++)
	if (!EQUAL(arg_4[i], exp_4[i]))
	    return 1;

    bu_sort(&arg_5, 4, sizeof(char[256]), comp_3, NULL);
    for (i = 0; i < 4; i++)
	if (strcmp(arg_5[i], exp_5[i]) != 0)
	    return 1;

    bu_sort(&arg_6, 3, sizeof(char[256]), comp_3, NULL);
    for (i = 0; i < 3; i++)
	if (strcmp(arg_6[i], exp_6[i]) != 0)
	    return 1;

    bu_sort(&arg_7, 9, sizeof(fastf_t), comp_4, &cmp_7);
    for (i = 0; i < 9; i++)
	if (!EQUAL(arg_7[i], exp_7[i]))
	    return 1;

    bu_sort(&arg_8, 5, sizeof(fastf_t), comp_4, &cmp_8);
    for (i = 0; i < 5; i++)
	if (!EQUAL(arg_8[i], exp_8[i]))
	    return 1;
    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
