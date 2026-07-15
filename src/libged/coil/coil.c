/*                          C O I L . C
 * BRL-CAD
 *
 * Copyright (c) 2009-2026 United States Government as represented by
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
/** @file coil.c
 *
 * Generator logic for creating coils.
 *
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "bu/cmdschema.h"
#include "vmath.h"
#include "bn.h"
#include "raytrace.h"
#include "wdb.h"
#include "ged.h"


#define D2R(x) (x * DEG2RAD)
#define DEFAULT_COIL_OBJECT "coil"
#define DEFAULT_COIL_FILENAME "coil.g"

struct coil_data_t {
    struct bu_list l;
    int nt;     /*Number of Turns*/
    fastf_t od; /*Outer Diameter*/
    fastf_t wd; /*Wire Diameter*/
    fastf_t ha; /*Helix Angle*/
    fastf_t p;  /*Pitch*/
    int lhf; /*Winding Direction - 1 is Right Handed, -1 is Left Handed*/
};


struct coil_args {
    fastf_t mean_outer_diameter;
    fastf_t wire_diameter;
    fastf_t helix_angle;
    fastf_t pitch;
    int number_of_turns;
    int start_cap_type;
    int end_cap_type;
    fastf_t overall_length;
    int left_handed;
    int help;
    struct bu_list sections;
};


static int
coil_cap_type_validate(struct bu_vls *msg, const char *arg)
{
    int value = 0;

    if (!bu_cmd_integer_from_str(&value, arg) || value < 0 || value > 3) {
	if (msg)
	    bu_vls_printf(msg, "coil cap type must be an integer from 0 through 3: %s", arg ? arg : "");
	return -1;
    }
    return 0;
}


static int
coil_section_consume(struct bu_vls *msg, const char *arg, void *storage)
{
    int nt = 0;
    int handedness = 0;
    double od = 0.0;
    double wd = 0.0;
    double ha = 0.0;
    double pitch = 0.0;
    char trailing = '\0';

    if (!arg || sscanf(arg, "%d,%lf,%lf,%lf,%lf,%d%c", &nt, &od, &wd, &ha,
		    &pitch, &handedness, &trailing) != 6 || nt < 0 || od < 0.0 ||
	    wd < 0.0 || ha < 0.0 || pitch < 0.0 || handedness < 0 ||
	    handedness > 1 || !isfinite(od) || !isfinite(wd) || !isfinite(ha) ||
	    !isfinite(pitch)) {
	if (msg)
	    bu_vls_printf(msg, "coil section must be turns,outer_diameter,wire_diameter,helix_angle,pitch,handedness");
	return -1;
    }

    if (storage) {
	struct coil_data_t *section;
	BU_ALLOC(section, struct coil_data_t);
	section->nt = nt;
	section->od = (fastf_t)od;
	section->wd = (fastf_t)wd;
	section->ha = (fastf_t)ha;
	section->p = (fastf_t)pitch;
	section->lhf = handedness ? 1 : -1;
	BU_LIST_INSERT((struct bu_list *)storage, &section->l);
    }
    return 0;
}


static void
coil_sections_free(struct bu_list *sections)
{
    while (!BU_LIST_IS_EMPTY(sections)) {
	struct coil_data_t *section = BU_LIST_FIRST(coil_data_t, sections);
	BU_LIST_DEQUEUE(&section->l);
	BU_PUT(section, struct coil_data_t);
    }
}


static const struct bu_cmd_option coil_schema_options[] = {
    BU_CMD_NONNEGATIVE_NUMBER("d", NULL, struct coil_args, mean_outer_diameter,
	"diameter", "Mean outer diameter"),
    BU_CMD_NONNEGATIVE_NUMBER("w", NULL, struct coil_args, wire_diameter,
	"diameter", "Wire diameter"),
    BU_CMD_NONNEGATIVE_NUMBER("H", NULL, struct coil_args, helix_angle,
	"angle", "Helix angle"),
    BU_CMD_NONNEGATIVE_NUMBER("p", NULL, struct coil_args, pitch,
	"pitch", "Pitch"),
    BU_CMD_NONNEGATIVE_INTEGER("n", NULL, struct coil_args, number_of_turns,
	"turns", "Number of turns"),
    BU_CMD_INTEGER_VALIDATE("s", NULL, struct coil_args, start_cap_type,
	coil_cap_type_validate, "type", "Start cap type (0 through 3)"),
    BU_CMD_INTEGER_VALIDATE("e", NULL, struct coil_args, end_cap_type,
	coil_cap_type_validate, "type", "End cap type (0 through 3)"),
    BU_CMD_CUSTOM("S", NULL, struct coil_args, sections, coil_section_consume,
	"section", "Section: turns,diameter,wire,angle,pitch,handedness"),
    BU_CMD_NONNEGATIVE_NUMBER("l", NULL, struct coil_args, overall_length,
	"length", "Overall length"),
    BU_CMD_FLAG("L", NULL, struct coil_args, left_handed, "Use left-handed winding"),
    BU_CMD_FLAG("h", NULL, struct coil_args, help, "Print command usage"),
    BU_CMD_ALIAS_SHORT("?", "h", 1),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand coil_schema_operands[] = {
    BU_CMD_OPERAND("output_object", BU_CMD_VALUE_STRING, 0, 1,
	"Optional new coil object name", NULL),
    BU_CMD_OPERAND_NULL
};
static const struct bu_cmd_schema coil_cmd_schema = {
    "coil", "Create coil geometry", coil_schema_options, coil_schema_operands,
    BU_CMD_PARSE_OPTIONS_FIRST, {NULL}
};


fastf_t
cap_squared(struct bu_list *head, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t pipe_bend, coil_radius;
    point_t pnt1, pnt2, pnt4, pnt6, pnt8;

    coil_radius = (mean_outer_diameter - wire_diameter)/2.0;
    pipe_bend = coil_radius;

    *need_subtraction += 0;

    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch/2+starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch+starting_pitch - sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch + pitch/8);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch + pitch*3/8 + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt1, 0 , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	return starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius;
    }

    return 0;
}


fastf_t
cap_squared_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t pipe_bend, coil_radius;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;
    struct bu_vls str1 = BU_VLS_INIT_ZERO;

    coil_radius = (mean_outer_diameter - wire_diameter)/2.0;
    pipe_bend = coil_radius;

    *need_subtraction += 1;

    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch/2+starting_pitch);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch+starting_pitch);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch);
	VSET(height, 0, 0, -wire_diameter);
	bu_vls_sprintf(&str1, "%s-startcap.s", prefix);
	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
	(void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, lhf*coil_radius , -coil_radius, starting_pitch + pitch/8);
	VSET(pnt4, lhf*coil_radius , coil_radius, starting_pitch + pitch*3/8 + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt1, 0 , -coil_radius, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(height, 0, 0, wire_diameter);
	bu_vls_sprintf(&str1, "%s-endcap.s", prefix);
	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius+wire_diameter+.1*wire_diameter);
	(void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return starting_pitch + pitch + sin(D2R(helix_angle))*coil_radius;
    }
}


fastf_t
cap_ground(struct rt_wdb *file, struct bu_list *head, char *prefix, struct wmember *coil_subtractions, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int is_start, int *need_subtraction, int lhf)
{
    fastf_t coil_radius, pipe_bend, center_height;
    point_t origin, height, pnt1, pnt2, pnt4, pnt6, pnt8;
    struct bu_vls str1 = BU_VLS_INIT_ZERO;

    coil_radius = (mean_outer_diameter - wire_diameter)/2.0;
    pipe_bend = coil_radius;

    *need_subtraction += 1;

    center_height = sqrt(((coil_radius/cos(D2R(helix_angle)))*(coil_radius/cos(D2R(helix_angle))))-coil_radius*coil_radius) + 0.25*pitch*cos(D2R(helix_angle));

    if (is_start == 1) {
	VSET(pnt1, 0, -coil_radius, starting_pitch);
	VSET(pnt2, lhf*coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, lhf*coil_radius , coil_radius, pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, 0);
	VSET(height, sin(D2R(helix_angle))*(wire_diameter+0.25*pitch), 0, -wire_diameter - 0.25*pitch);
	bu_vls_trunc(&str1, 0);
	bu_vls_printf(&str1, "%s-startcap.s", prefix);
	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius*(1+sin(D2R(helix_angle)))+wire_diameter+.1*wire_diameter);
	(void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return pitch + starting_pitch;
    } else {
	VSET(pnt2, lhf*coil_radius , -coil_radius, pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, lhf*coil_radius , coil_radius,  pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt1, 0 , -coil_radius, pitch+starting_pitch);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt1, wire_diameter, 0.0, pipe_bend);
	VSET(origin, 0, 0, starting_pitch + 15/8*pitch - center_height + sin(D2R(helix_angle))*coil_radius);
	VSET(height, -sin(D2R(helix_angle))*(wire_diameter+0.25*pitch), 0, wire_diameter + 0.25*pitch);
	bu_vls_trunc(&str1, 0);
	bu_vls_printf(&str1, "%s-endcap.s", prefix);
	mk_rcc(file, bu_vls_addr(&str1), origin, height, coil_radius*(1+sin(D2R(helix_angle)))+wire_diameter+.1*wire_diameter);
	(void)mk_addmember(bu_vls_addr(&str1), &(*coil_subtractions).l, NULL, WMOP_UNION);
	bu_vls_free(&str1);
	return pitch+starting_pitch;
    }
}


fastf_t
helical_coil_plain(struct bu_list *head, fastf_t mean_outer_diameter, fastf_t wire_diameter, fastf_t helix_angle, fastf_t pitch, fastf_t starting_pitch, int nt, int lhf)
{
    int i;
    fastf_t coil_radius, pipe_bend;
    point_t pnt2, pnt4, pnt6, pnt8;

    coil_radius = (mean_outer_diameter - wire_diameter)/2.0;
    pipe_bend = coil_radius;

    for (i = 0; i < nt; i++) {
	VSET(pnt2, lhf*coil_radius , -coil_radius, i*pitch + pitch/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt4, lhf*coil_radius , coil_radius, i*pitch + pitch*3/8 + starting_pitch + sin(D2R(helix_angle))*coil_radius);
	VSET(pnt6, lhf*-coil_radius , coil_radius, i*pitch + pitch*5/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	VSET(pnt8, lhf*-coil_radius , -coil_radius, i*pitch + pitch*7/8 + starting_pitch - sin(D2R(helix_angle))*coil_radius);
	mk_add_pipe_pnt(head, pnt2, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt4, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt6, wire_diameter, 0.0, pipe_bend);
	mk_add_pipe_pnt(head, pnt8, wire_diameter, 0.0, pipe_bend);
    }

    return (nt-1)*pitch + pitch*7/8 + starting_pitch;
}


void
make_coil(struct rt_wdb (*file), char *prefix, struct bu_list *sections, int start_cap_type, int end_cap_type)
{
    struct bu_list head;
    fastf_t last_pitch_pt;
    point_t pnt1;
    int need_subtractions = 0;
    struct wmember coil;
    struct wmember coil_subtractions;
    struct coil_data_t *s_data;
    struct coil_data_t *e_data;
    struct coil_data_t *cd;
    struct bu_vls str = BU_VLS_INIT_ZERO;

    BU_LIST_INIT(&coil.l);
    BU_LIST_INIT(&coil_subtractions.l);
    mk_pipe_init(&head);

    s_data = BU_LIST_FIRST(coil_data_t, &(*sections));
    e_data = BU_LIST_LAST(coil_data_t, &(*sections));

    last_pitch_pt = 0;

    switch (start_cap_type) {
	case 0:
	    VSET(pnt1, 0, -1*(s_data->od/2-s_data->wd/2), last_pitch_pt);
	    mk_add_pipe_pnt(&head, pnt1, s_data->wd, 0.0, (s_data->od/2-s_data->wd/2));
	    break;
	case 1:
	    last_pitch_pt = cap_squared(&head, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	case 2:
	    last_pitch_pt = cap_ground(file, &head, prefix, &coil_subtractions, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	case 3:
	    last_pitch_pt = cap_squared_ground(file, &head, prefix, &coil_subtractions, s_data->od, s_data->wd, s_data->ha, s_data->p, 0, 1, &need_subtractions, s_data->lhf);
	    break;
	default:
	    break;
    }

    for (BU_LIST_FOR(cd, coil_data_t, &(*sections))) {
	last_pitch_pt = helical_coil_plain(&head, cd->od, cd->wd, cd->ha, cd->p, last_pitch_pt, cd->nt, cd->lhf);
    }

    switch (end_cap_type) {
	case 0:
	    VSET(pnt1, 0 , -1*(e_data->od/2-e_data->wd/2), 0.125*(e_data->p)+last_pitch_pt);
	    mk_add_pipe_pnt(&head, pnt1, e_data->wd, 0.0, (e_data->od/2-e_data->wd/2));
	    break;
	case 1:
	    (void)cap_squared(&head, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	case 2:
	    (void)cap_ground(file, &head, prefix, &coil_subtractions, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	case 3:
	    (void)cap_squared_ground(file, &head, prefix, &coil_subtractions, e_data->od, e_data->wd, e_data->ha, e_data->p, last_pitch_pt, 0, &need_subtractions, s_data->lhf);
	    break;
	default:
	    break;
    }

    bu_vls_trunc(&str, 0);
    bu_vls_printf(&str, "%s_core.s", prefix);
    mk_pipe(file, bu_vls_addr(&str), &head);

    (void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_UNION);

    if (need_subtractions > 0) {
	bu_vls_trunc(&str, 0);
	bu_vls_printf(&str, "%s_subtractions.c", prefix);
	mk_lcomb(file, bu_vls_addr(&str), &coil_subtractions, 0, NULL, NULL, NULL, 0);
	(void)mk_addmember(bu_vls_addr(&str), &coil.l, NULL, WMOP_SUBTRACT);
    }

    mk_lcomb(file, prefix, &coil, 0, NULL, NULL, NULL, 0);

    bu_vls_free(&str);
    mk_pipe_free(&head);
}


static void
coil_usage(struct ged *gedp)
{
    char *option_help = bu_cmd_schema_describe(&coil_cmd_schema);

    bu_vls_printf(gedp->ged_result_str,
	"Usage: coil [-d diameter] [-w diameter] [-H angle] [-p pitch]\n"
	"            [-n turns] [-s type] [-e type] [-S section] [-l length] [-L] [name]\n"
	"       (units mm)");
    if (option_help) {
	bu_vls_printf(gedp->ged_result_str, "\nOptions:\n%s", option_help);
	bu_free(option_help, "coil native option help");
    }
}


int
ged_coil_core(struct ged *gedp, int argc, const char *argv[])
{
    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct coil_args args = {0};
    struct bu_cmd_validate_result validation = BU_CMD_VALIDATE_RESULT_NULL;
    const char **operands = NULL;
    int operand_index;
    int operand_count;
    int usedefaults;
    fastf_t mean_outer_diameter, wire_diameter, overall_length, nominal_length;
    fastf_t helix_angle, pitch;

    struct coil_data_t *coil_data = NULL;

    int nt; /* Number of turns */
    int start_cap_type, end_cap_type;
    int lhf; /* Winding flag */

    /* initialize result */
    BU_LIST_INIT(&args.sections);
    usedefaults = argc == 1;
    operand_index = bu_cmd_schema_parse(&coil_cmd_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	coil_usage(gedp);
	coil_sections_free(&args.sections);
	bu_vls_free(&name);
	return BRLCAD_ERROR;
    }
    if (args.help) {
	coil_usage(gedp);
	coil_sections_free(&args.sections);
	bu_vls_free(&name);
	return GED_HELP;
    }
    if (bu_cmd_schema_validate(&coil_cmd_schema, (size_t)(argc - 1), argv + 1,
		(size_t)(argc - 1), &validation) != 0 ||
	validation.state != BU_CMD_VALIDATE_VALID) {
	if (validation.hint)
	    bu_vls_printf(gedp->ged_result_str, "%s\n", validation.hint);
	bu_cmd_validate_result_clear(&validation);
	coil_usage(gedp);
	coil_sections_free(&args.sections);
	bu_vls_free(&name);
	return BRLCAD_ERROR;
    }
    bu_cmd_validate_result_clear(&validation);

    operand_count = argc - 1 - operand_index;
    operands = argv + 1 + operand_index;
    if (operand_count == 1)
	bu_vls_strcpy(&name, operands[0]);
    else
	bu_vls_strcpy(&name, DEFAULT_COIL_OBJECT);

    mean_outer_diameter = args.mean_outer_diameter;
    wire_diameter = args.wire_diameter;
    helix_angle = args.helix_angle;
    pitch = args.pitch;
    nt = args.number_of_turns;
    start_cap_type = args.start_cap_type;
    end_cap_type = args.end_cap_type;
    overall_length = args.overall_length;
    lhf = args.left_handed ? -1 : 1;

    /* Handle various potential errors in args and set defaults if nothing supplied */

    if (BU_LIST_IS_EMPTY(&args.sections)) {

	if (usedefaults)
	    bu_vls_printf(gedp->ged_result_str, "Creating %s with default parameters.\n", bu_vls_addr(&name));

	if (mean_outer_diameter < 0 || wire_diameter < 0 || helix_angle < 0 || pitch < 0 || nt < 0 || start_cap_type < 0 || end_cap_type < 0) {
	    bu_vls_printf(gedp->ged_result_str, "negative value in one or more arguments supplied to coil");
	    coil_sections_free(&args.sections);
	    bu_vls_free(&name);
	    return BRLCAD_ERROR;
	}

	if (ZERO(wire_diameter) && ZERO(mean_outer_diameter)) {
	    mean_outer_diameter = 1000;
	    wire_diameter = 100;
	}

	if (ZERO(wire_diameter) && mean_outer_diameter > 0)
	    wire_diameter = mean_outer_diameter/10;

	if (ZERO(mean_outer_diameter) && wire_diameter > 0)
	    mean_outer_diameter = wire_diameter * 10;

	if (ZERO(pitch))
	    pitch = wire_diameter;

	if (pitch < wire_diameter) {
	    bu_vls_printf(gedp->ged_result_str, "WARNING:  Pitch less than wire diameter.  Setting pitch to wire diameter: %f mm\n", wire_diameter);
	    pitch = wire_diameter;
	}

	if (nt == 0)
	    nt = 30;

	BU_ALLOC(coil_data, struct coil_data_t);
	coil_data->nt = nt;
	coil_data->od = mean_outer_diameter;
	coil_data->wd = wire_diameter;
	coil_data->ha = helix_angle;
	coil_data->p = pitch;
	coil_data->lhf = lhf;

	BU_LIST_APPEND(&args.sections, &coil_data->l);

	coil_data = BU_LIST_FIRST(coil_data_t, &args.sections);
    }

    /* If hard clamping the length, have to check some things and maybe clamp some values */

    if (!ZERO(overall_length)) {
	bu_vls_printf(gedp->ged_result_str, "Note:  Length clamping overrides other specified values. If supplied values are\n"
	       "inconsistent with specified length, they will be overridden in this order:\n\n"
	       "When Shrinking:  pitch, number of turns, wire diameter\n"
	       "When Expanding:  number of turns, pitch\n\n");

	/* broken up for c90 and readability */
	bu_vls_printf(gedp->ged_result_str, "Currently, this override order is independent of whether the value is supplied\n"
	       "by the user or calculated internally.  That is, there is no preference for \n"
	       "protecting user specified properties.  Moreover, length clamping will NOT \n"
	       "override explicit section specification with -S\n");

	if (coil_data) {
	    if (start_cap_type != 0 || end_cap_type != 0) {
		bu_vls_printf(gedp->ged_result_str, "Note:  At this time only uncapped coils are allowed when length is constrained.\n");
		start_cap_type = 0;
		end_cap_type = 0;
	    }
	    if (!ZERO(helix_angle)) {
		bu_vls_printf(gedp->ged_result_str, "Note:  At this time variable helix angles are unsupported when length is constrained.\n");
		helix_angle = 0;
	    }
	    /* Thanks to earlier checks, we're guaranteed to have valid data for this calculation regardless of
	     * user input */
	    nominal_length = coil_data->wd + coil_data->p * coil_data->nt;
	    if (nominal_length > overall_length) {
		/* Something has to give - start with pitch */
		coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
		while (coil_data->p < coil_data->wd) {
		    /* That didn't work, start knocking off turns*/
		    while ((coil_data->nt > 1) && (coil_data->p < coil_data->wd)) {
			coil_data->nt--;
			coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
		    }
		    if (coil_data->nt == 1) {
			/* THAT didn't work, change the wire diameter */
			coil_data->wd = overall_length/2;
			coil_data->p = coil_data->wd ;
		    }
		}
	    } else {
		if (!EQUAL(nominal_length, overall_length)) {
		    /* Add turns first, then adjust pitch */
		    while (nominal_length < overall_length) {
			coil_data->nt++;
			nominal_length = coil_data->wd + coil_data->p * coil_data->nt;
		    }
		    coil_data->nt--;
		    coil_data->p = (overall_length - coil_data->wd)/coil_data->nt;
		}
	    }
	}
    }

    /* do it. */
    struct rt_wdb *db_fp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    make_coil(db_fp, bu_vls_addr(&name), &args.sections, start_cap_type, end_cap_type);

    coil_sections_free(&args.sections);
    bu_vls_free(&name);

    return BRLCAD_OK;
}

#include "../include/plugin.h"

#define GED_COIL_COMMANDS(X, XID) \
    X(coil, ged_coil_core, GED_CMD_DEFAULT, &coil_cmd_schema) \

GED_DECLARE_COMMAND_SET_WITH_NATIVE_SCHEMA(GED_COIL_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_NATIVE_SCHEMA("libged_coil", 1, GED_COIL_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
