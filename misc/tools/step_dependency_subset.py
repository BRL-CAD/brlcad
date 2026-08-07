#!/usr/bin/env python3
"""Extract a small, forward-complete STEP geometry subset.

This helper is intended for local diagnosis and for identifying the minimal
geometry that a project-owned synthetic regression test must reproduce.  It
copies the dependency closure of one or more STEP face entities and wraps them
in an OPEN_SHELL product, retaining the source entity numbers so converter
diagnostics remain directly comparable to the original file.  A diagnostic
--closed-shell mode instead uses CLOSED_SHELL/MANIFOLD_SOLID_BREP so reductions
can exercise topology rules which only apply to asserted solids.  A face subset
is not necessarily a valid closed shell, so that mode is for failure reduction,
not evidence that the extracted result is valid geometry.  The output keeps the
input model's provenance and is not automatically suitable for committing or
redistribution.
"""

import argparse
import re
from pathlib import Path


ADVANCED_FACE = re.compile(
    r"^[ \t]*#[0-9]+[ \t]*=[ \t]*ADVANCED_FACE[ \t]*\(",
    re.IGNORECASE,
)
FILE_SCHEMA = re.compile(
    r"FILE_SCHEMA\s*\(\s*\(\s*'([^']+)'", re.IGNORECASE | re.DOTALL
)
ENTITY_TYPE = re.compile(
    r"^[ \t]*#[0-9]+[ \t]*=[ \t]*([A-Z0-9_]+)[ \t]*\(",
    re.IGNORECASE,
)


def instance_end(text, start):
    """Return the first record-ending semicolon outside strings/comments."""
    in_string = False
    in_comment = False
    offset = start
    while offset < len(text):
        current = text[offset]
        following = text[offset + 1] if offset + 1 < len(text) else ""
        if in_comment:
            if current == "*" and following == "/":
                in_comment = False
                offset += 2
                continue
        elif in_string:
            if current == "'":
                if following == "'":
                    offset += 2
                    continue
                in_string = False
        elif current == "/" and following == "*":
            in_comment = True
            offset += 2
            continue
        elif current == "'":
            in_string = True
        elif current == ";":
            return offset + 1
        offset += 1
    raise ValueError("unterminated STEP instance starting at byte {}".format(start))


def instances(text):
    """Return Part 21 instances without recognizing text in strings/comments."""
    result = {}
    offset = 0
    line_start = True
    in_string = False
    in_comment = False
    while offset < len(text):
        current = text[offset]
        following = text[offset + 1] if offset + 1 < len(text) else ""
        if in_comment:
            if current == "*" and following == "/":
                in_comment = False
                offset += 2
                continue
        elif in_string:
            if current == "'":
                if following == "'":
                    offset += 2
                    continue
                in_string = False
        elif current == "/" and following == "*":
            in_comment = True
            offset += 2
            continue
        elif current == "'":
            in_string = True
        elif current == "\n" or current == "\r":
            line_start = True
        elif line_start and current in " \t":
            pass
        elif line_start and current == "#":
            digit_end = offset + 1
            while digit_end < len(text) and text[digit_end].isdigit():
                digit_end += 1
            separator = digit_end
            while separator < len(text) and text[separator] in " \t":
                separator += 1
            if digit_end > offset + 1 and separator < len(text) and \
                    text[separator] == "=":
                entity_id = int(text[offset + 1:digit_end])
                end = instance_end(text, offset)
                if entity_id in result:
                    raise ValueError(
                        "duplicate STEP entity #{}".format(entity_id)
                    )
                result[entity_id] = text[offset:end].strip()
                offset = end
                line_start = False
                continue
            line_start = False
        else:
            line_start = False
        offset += 1
    return result


def references(record):
    """Yield entity references outside Part 21 strings and comments."""
    offset = 0
    in_string = False
    in_comment = False
    while offset < len(record):
        current = record[offset]
        following = record[offset + 1] if offset + 1 < len(record) else ""
        if in_comment:
            if current == "*" and following == "/":
                in_comment = False
                offset += 2
                continue
        elif in_string:
            if current == "'":
                if following == "'":
                    offset += 2
                    continue
                in_string = False
        elif current == "/" and following == "*":
            in_comment = True
            offset += 2
            continue
        elif current == "'":
            in_string = True
        elif current == "#":
            digit_end = offset + 1
            while digit_end < len(record) and record[digit_end].isdigit():
                digit_end += 1
            if digit_end > offset + 1:
                yield int(record[offset + 1:digit_end])
                offset = digit_end
                continue
        offset += 1


def dependency_closure(records, roots):
    pending = list(roots)
    selected = set()
    while pending:
        entity_id = pending.pop()
        if entity_id in selected:
            continue
        record = records.get(entity_id)
        if record is None:
            raise ValueError("STEP entity #{} is not present".format(entity_id))
        selected.add(entity_id)
        for referenced_id in references(record):
            if referenced_id != entity_id and referenced_id not in selected:
                pending.append(referenced_id)
    return selected


def record_type(record):
    match = ENTITY_TYPE.match(record)
    return match.group(1).upper() if match else ""


def adjacent_faces(records, faces):
    """Return ADVANCED_FACE entities sharing an EDGE_CURVE with faces."""
    reverse = {}
    for entity_id, record in records.items():
        for referenced_id in references(record):
            reverse.setdefault(referenced_id, set()).add(entity_id)

    edge_curves = {
        entity_id
        for entity_id in dependency_closure(records, faces)
        if record_type(records[entity_id]) == "EDGE_CURVE"
    }
    oriented_edges = {
        entity_id
        for edge_id in edge_curves
        for entity_id in reverse.get(edge_id, ())
        if record_type(records[entity_id]) == "ORIENTED_EDGE"
    }
    edge_loops = {
        entity_id
        for oriented_id in oriented_edges
        for entity_id in reverse.get(oriented_id, ())
        if record_type(records[entity_id]) == "EDGE_LOOP"
    }
    face_bounds = {
        entity_id
        for loop_id in edge_loops
        for entity_id in reverse.get(loop_id, ())
        if record_type(records[entity_id]) in {
            "FACE_BOUND", "FACE_OUTER_BOUND"
        }
    }
    return {
        entity_id
        for bound_id in face_bounds
        for entity_id in reverse.get(bound_id, ())
        if record_type(records[entity_id]) == "ADVANCED_FACE"
    }


def quoted(value):
    return value.replace("'", "''")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument(
        "--face",
        type=int,
        action="append",
        default=[],
        help="ADVANCED_FACE entity ID to retain (repeatable)",
    )
    parser.add_argument(
        "--face-range",
        action="append",
        default=[],
        metavar="FIRST:LAST",
        help="inclusive contiguous ADVANCED_FACE ID range to retain (repeatable)",
    )
    parser.add_argument(
        "--face-neighbor-depth",
        type=int,
        default=0,
        metavar="N",
        help=(
            "also retain ADVANCED_FACE neighbors reachable through N layers "
            "of shared EDGE_CURVEs"
        ),
    )
    parser.add_argument("--name", default="reduced_step_regression")
    parser.add_argument(
        "--context",
        type=int,
        help=(
            "source GEOMETRIC_REPRESENTATION_CONTEXT entity ID to preserve; "
            "recommended when source angle or length units are not SI defaults"
        ),
    )
    parser.add_argument(
        "--uncertainty",
        type=float,
        default=1.0e-6,
        help="fixture uncertainty in millimetres (default: 1e-6)",
    )
    parser.add_argument(
        "--closed-shell",
        action="store_true",
        help=(
            "wrap the face subset as CLOSED_SHELL/MANIFOLD_SOLID_BREP "
            "instead of OPEN_SHELL/SHELL_BASED_SURFACE_MODEL"
        ),
    )
    args = parser.parse_args()
    faces = list(args.face)
    for face_range in args.face_range:
        fields = face_range.split(":", 1)
        if len(fields) != 2:
            parser.error("--face-range must be FIRST:LAST")
        try:
            first, last = (int(field) for field in fields)
        except ValueError:
            parser.error("--face-range must contain integer entity IDs")
        if first > last:
            parser.error("--face-range FIRST must not exceed LAST")
        faces.extend(range(first, last + 1))
    faces = sorted(set(faces))
    if not faces:
        parser.error("at least one --face or --face-range is required")
    if args.face_neighbor_depth < 0:
        parser.error("--face-neighbor-depth must be nonnegative")

    source = args.input.read_text(encoding="latin-1")
    records = instances(source)
    for entity_id in faces:
        record = records.get(entity_id)
        if record is None:
            parser.error("STEP entity #{} is not present".format(entity_id))
        if not ADVANCED_FACE.match(record):
            parser.error(
                "STEP entity #{} is not an ADVANCED_FACE".format(entity_id)
            )
    expanded_faces = set(faces)
    frontier = set(faces)
    for _ in range(args.face_neighbor_depth):
        neighbors = adjacent_faces(records, frontier) - expanded_faces
        if not neighbors:
            break
        expanded_faces.update(neighbors)
        frontier = neighbors
    faces = sorted(expanded_faces)
    closure_roots = list(faces)
    if args.context is not None:
        closure_roots.append(args.context)
    selected = dependency_closure(records, closure_roots)
    schema_match = FILE_SCHEMA.search(source)
    schema = (
        schema_match.group(1)
        if schema_match
        else "AUTOMOTIVE_DESIGN { 1 0 10303 214 3 1 1 1 }"
    )

    next_id = max(records) + 1
    wrapper_ids = list(range(next_id, next_id + 18))
    (
        shell_id,
        model_id,
        length_id,
        angle_id,
        solid_angle_id,
        uncertainty_id,
        context_id,
        application_id,
        protocol_id,
        product_context_id,
        definition_context_id,
        product_id,
        formation_id,
        definition_id,
        definition_shape_id,
        representation_id,
        shape_definition_id,
        _unused_id,
    ) = wrapper_ids

    safe_name = quoted(args.name)
    face_refs = ",".join("#{}".format(entity_id) for entity_id in faces)
    if args.closed_shell:
        wrapper = [
            "#{}=CLOSED_SHELL('{} shell',({}));".format(
                shell_id, safe_name, face_refs
            ),
            "#{}=MANIFOLD_SOLID_BREP('{} model',#{});".format(
                model_id, safe_name, shell_id
            ),
        ]
    else:
        wrapper = [
            "#{}=OPEN_SHELL('{} shell',({}));".format(
                shell_id, safe_name, face_refs
            ),
            "#{}=SHELL_BASED_SURFACE_MODEL('{} model',(#{}));".format(
                model_id, safe_name, shell_id
            ),
        ]
    if args.context is None:
        wrapper.extend(
            [
                "#{}=(LENGTH_UNIT() NAMED_UNIT(*) "
                "SI_UNIT(.MILLI.,.METRE.));".format(length_id),
                "#{}=(NAMED_UNIT(*) PLANE_ANGLE_UNIT() "
                "SI_UNIT($,.RADIAN.));".format(angle_id),
                "#{}=(NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) "
                "SOLID_ANGLE_UNIT());".format(solid_angle_id),
                "#{}=UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE({:.17g}),"
                "#{},'distance_accuracy_value','maximum gap value');".format(
                    uncertainty_id, args.uncertainty, length_id
                ),
                "#{}=(GEOMETRIC_REPRESENTATION_CONTEXT(3) "
                "GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT((#{})) "
                "GLOBAL_UNIT_ASSIGNED_CONTEXT((#{},#{},#{})) "
                "REPRESENTATION_CONTEXT('fixture','3D'));".format(
                    context_id,
                    uncertainty_id,
                    length_id,
                    angle_id,
                    solid_angle_id,
                ),
            ]
        )
    representation_context_id = (
        args.context if args.context is not None else context_id
    )
    wrapper.extend(
        [
        "#{}=APPLICATION_CONTEXT('automotive design');".format(application_id),
        "#{}=APPLICATION_PROTOCOL_DEFINITION("
        "'fixture','automotive_design',2010,#{});".format(
            protocol_id, application_id
        ),
        "#{}=PRODUCT_CONTEXT('',#{},'mechanical');".format(
            product_context_id, application_id
        ),
        "#{}=PRODUCT_DEFINITION_CONTEXT('part definition',#{},'design');".format(
            definition_context_id, application_id
        ),
        "#{}=PRODUCT('{}','{}','reduced corpus regression fixture',(#{}));".format(
            product_id, safe_name, safe_name, product_context_id
        ),
        "#{}=PRODUCT_DEFINITION_FORMATION_WITH_SPECIFIED_SOURCE("
        "'1','',#{},.MADE.);".format(formation_id, product_id),
        "#{}=PRODUCT_DEFINITION('design','',#{},#{});".format(
            definition_id, formation_id, definition_context_id
        ),
        "#{}=PRODUCT_DEFINITION_SHAPE('','',#{});".format(
            definition_shape_id, definition_id
        ),
        "#{}={}('',(#{}),#{});".format(
            representation_id,
            "ADVANCED_BREP_SHAPE_REPRESENTATION" if args.closed_shell else
            "MANIFOLD_SURFACE_SHAPE_REPRESENTATION",
            model_id,
            representation_context_id,
        ),
        "#{}=SHAPE_DEFINITION_REPRESENTATION(#{},#{});".format(
            shape_definition_id, definition_shape_id, representation_id
        ),
        ]
    )

    lines = [
        "ISO-10303-21;",
        "HEADER;",
        "FILE_DESCRIPTION(('BRL-CAD reduced corpus regression fixture'),'2;1');",
        "FILE_NAME('{}','',('BRL-CAD'),('BRL-CAD'),'','','');".format(safe_name),
        "FILE_SCHEMA(('{}'));".format(quoted(schema)),
        "ENDSEC;",
        "DATA;",
    ]
    lines.extend(records[entity_id] for entity_id in sorted(selected))
    lines.extend(wrapper)
    lines.extend(("ENDSEC;", "END-ISO-10303-21;", ""))
    args.output.write_text("\n".join(lines), encoding="latin-1")


if __name__ == "__main__":
    main()
