#!/usr/bin/env python3
"""Generate the STEP/IGES semantic-storage audit matrix.

Arbitrary .g attributes and binary payloads intentionally do not count as
semantic preservation in this inventory.
"""

from __future__ import annotations

import argparse
import csv
import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCHEMAS = {
    "AP203": "src/conv/step/step-g/ap203.exp",
    "AP203E2": "src/conv/step/ap203e2/ap203e2.exp",
    "AP214E3": "src/conv/step/ap214/ap214e3.exp",
    "AP242E1": "src/conv/step/ap242/ap242e1.exp",
    "AP242E2": "src/conv/step/ap242/ap242e2.exp",
    "AP242E3": "src/conv/step/ap242/ap242e3.exp",
    "AP242E4": "src/conv/step/ap242/ap242e4.exp",
}
EXPECTED_ENTITY_COUNT = 2469
EXPECTED_TYPE_COUNT = 552
EXPECTED_IGES_GAPS = frozenset({141, 143, 146, 148, 182, 192, 194, 196, 198, 204, 213, 316})
EXPECTED_IGES_EXTENSION = frozenset({700})

ENTITY_RE = re.compile(r"\bENTITY\s+([A-Za-z][A-Za-z0-9_]*)\s*;?(.*?)(?=\bEND_ENTITY\s*;)", re.I | re.S)
TYPE_RE = re.compile(r"\bTYPE\s+([A-Za-z][A-Za-z0-9_]*)\s*(.*?)(?=\bEND_TYPE\s*;)", re.I | re.S)
SUBTYPE_RE = re.compile(r"\bSUBTYPE\s+OF\s*\((.*?)\)", re.I | re.S)
BLOCK_COMMENT_RE = re.compile(r"\(\*.*?\*\)", re.S)
LINE_COMMENT_RE = re.compile(r"--[^\n]*")
IDENTIFIER_RE = re.compile(r"[A-Za-z][A-Za-z0-9_]*")
IGES_TYPE_RE = re.compile(r"\{\s*(\d+)\s*,")

CATEGORY_INFO = {
    "analysis_fea": ("defer-domain", "none", "defer-sidecar"),
    "annotation_graphics": ("partial-native", "annot", "pmi-links-if-needed"),
    "external_reference": ("missing-native", "none", "safe-reference-link-only"),
    "generic_semantic_graph": ("missing-native", "none", "typed-relationship-graph"),
    "geometric_constraints": ("missing-native", "constraint-is-insufficient", "typed-constraints-defer-solver"),
    "kinematics": ("partial-native", "joint-is-display-oriented", "occurrence-constraint-graph"),
    "manufacturing_domain": ("defer-domain", "none", "domain-module"),
    "materials": ("partial-native", "material-and-combination-style", "typed-properties-and-assignments"),
    "native_geometry": ("geometry-only", "brep-nmg-sketch-primitives-combination", "translator-or-kernel-work"),
    "not_applicable": ("not-applicable", "none", "no-persistent-object"),
    "parametric_procedural": ("defer-do-not-add", "script-is-not-interoperable-semantics", "do-not-execute-foreign-procedures"),
    "pdm_administration": ("missing-native", "none", "product-pdm-graph-or-explicitly-external"),
    "pmi_semantics": ("missing-native", "datum-and-annot-are-subsets", "typed-pmi-and-subshape-links"),
    "point_cloud_scan": ("partial-native", "pnts-and-bot", "scan-schema-if-required"),
    "presentation_style": ("partial-native", "combination-style-and-annot", "layer-style-view-sheet-graph"),
    "product_structure": ("partial-native", "combination-and-rigid-matrices", "product-occurrence-configuration-graph"),
    "quality_validation": ("defer-domain", "none", "quality-report-sidecar"),
    "quantities_units": ("partial-native", "database-linear-unit-scale", "typed-values-and-unit-graph"),
    "representation_graph": ("missing-native", "none", "representation-context-graph"),
    "shape_feature_semantics": ("geometry-only", "brep-or-csg-result", "subshape-and-feature-graph"),
    "static_assembly": ("partial-native", "combination-and-rigid-matrices", "relationship-graph-for-source-intent"),
    "tessellated_geometry": ("partial-native", "bot", "mesh-semantics-if-required"),
}

QUALITY_PREFIXES = (
    "a3m_", "a3ma_", "a3ms_", "abrupt_change_", "abrupt_normal_", "data_equivalence_", "data_quality_", "detailed_equivalence_", "detailed_report_", "different_", "disconnected_", "duplicate_", "erroneous_", "excessive_", "extreme_", "g1_", "g2_", "gap_", "geometric_gap_", "geometry_with_", "high_degree_", "inapt_", "inappropriate_", "inconsistent_", "indistinct_", "inspection_", "intersecting_", "mismatch_", "missing_", "multiply_defined_", "narrow_", "nearly_degenerate_", "non_manifold_", "non_referenced_", "non_smooth_", "overcomplex_", "overlapping_", "partly_overlapping_", "quality_", "self_intersecting_", "shape_data_quality_", "shape_inspection_", "shape_summary_", "short_length_", "small_", "steep_", "summary_equivalence_", "summary_report_", "topology_related_", "tsdq_", "unused_", "validation_", "wrong_", "wrongly_", "zero_",
)

IGES_BASE = (
    (0, "Null Entity", "not_applicable", "standard entity"),
    (100, "Circular Arc", "native_geometry", "standard entity"),
    (102, "Composite Curve", "native_geometry", "standard entity"),
    (104, "Conic Arc", "native_geometry", "standard entity"),
    (108, "Plane", "native_geometry", "standard entity"),
    (110, "Line", "native_geometry", "form 0"),
    (112, "Parametric Spline Curve", "native_geometry", "standard entity"),
    (114, "Parametric Spline Surface", "native_geometry", "standard entity"),
    (116, "Point", "native_geometry", "standard entity"),
    (118, "Ruled Surface", "native_geometry", "standard entity"),
    (120, "Surface of Revolution", "native_geometry", "standard entity"),
    (122, "Tabulated Cylinder", "native_geometry", "standard entity"),
    (123, "Direction", "native_geometry", "standard entity"),
    (124, "Transformation Matrix", "static_assembly", "standard entity"),
    (125, "Flash", "annotation_graphics", "standard entity"),
    (126, "Rational B-Spline Curve", "native_geometry", "standard entity"),
    (128, "Rational B-Spline Surface", "native_geometry", "standard entity"),
    (130, "Offset Curve", "native_geometry", "standard entity"),
    (132, "Connect Point", "generic_semantic_graph", "standard entity"),
    (134, "Node", "generic_semantic_graph", "standard entity"),
    (136, "Finite Element", "analysis_fea", "standard entity"),
    (138, "Nodal Displacement and Rotation", "analysis_fea", "standard entity"),
    (140, "Offset Surface", "native_geometry", "standard entity"),
    (141, "Boundary", "native_geometry", "standard entity"),
    (142, "Curve on a Parametric Surface", "native_geometry", "standard entity"),
    (143, "Bounded Surface", "native_geometry", "standard entity"),
    (144, "Trimmed Parametric Surface", "native_geometry", "standard entity"),
    (146, "Nodal Results", "analysis_fea", "standard entity"),
    (148, "Element Results", "analysis_fea", "standard entity"),
    (150, "Block", "native_geometry", "standard entity"),
    (152, "Right Angular Wedge", "native_geometry", "standard entity"),
    (154, "Right Circular Cylinder", "native_geometry", "standard entity"),
    (156, "Right Circular Cone Frustum", "native_geometry", "standard entity"),
    (158, "Sphere", "native_geometry", "standard entity"),
    (160, "Torus", "native_geometry", "standard entity"),
    (162, "Solid of Revolution", "native_geometry", "standard entity"),
    (164, "Solid of Linear Extrusion", "native_geometry", "standard entity"),
    (168, "Ellipsoid", "native_geometry", "standard entity"),
    (180, "Boolean Tree", "native_geometry", "standard entity"),
    (182, "Selected Component", "generic_semantic_graph", "standard entity"),
    (184, "Solid Assembly", "static_assembly", "standard entity"),
    (186, "Manifold Solid B-Rep Object", "native_geometry", "standard entity"),
    (190, "Plane Surface", "native_geometry", "standard entity"),
    (192, "Right Circular Cylindrical Surface", "native_geometry", "standard entity"),
    (194, "Right Circular Conical Surface", "native_geometry", "standard entity"),
    (196, "Spherical Surface", "native_geometry", "standard entity"),
    (198, "Toroidal Surface", "native_geometry", "standard entity"),
    (202, "Angular Dimension", "pmi_semantics", "standard entity"),
    (204, "Curve Dimension", "pmi_semantics", "standard entity"),
    (206, "Diameter Dimension", "pmi_semantics", "standard entity"),
    (208, "Flag Note", "annotation_graphics", "standard entity"),
    (210, "General Label", "annotation_graphics", "standard entity"),
    (212, "General Note", "annotation_graphics", "standard entity"),
    (213, "New General Note", "annotation_graphics", "standard entity"),
    (214, "Leader (Arrow)", "annotation_graphics", "standard entity"),
    (216, "Linear Dimension", "pmi_semantics", "standard entity"),
    (218, "Ordinate Dimension", "pmi_semantics", "standard entity"),
    (220, "Point Dimension", "pmi_semantics", "standard entity"),
    (222, "Radius Dimension", "pmi_semantics", "standard entity"),
    (228, "General Symbol", "annotation_graphics", "standard entity"),
    (230, "Sectioned Area", "annotation_graphics", "standard entity"),
    (302, "Associativity Definition", "generic_semantic_graph", "standard entity"),
    (304, "Line Font Definition", "presentation_style", "standard entity"),
    (306, "MACRO Definition", "parametric_procedural", "standard entity"),
    (308, "Subfigure Definition", "static_assembly", "standard entity"),
    (310, "Text Font Definition", "presentation_style", "standard entity"),
    (312, "Text Display Template", "annotation_graphics", "standard entity"),
    (314, "Color Definition", "presentation_style", "standard entity"),
    (316, "Units Data", "quantities_units", "standard entity"),
    (320, "Network Subfigure Definition", "generic_semantic_graph", "standard entity"),
    (322, "Attribute Table Definition", "generic_semantic_graph", "standard entity"),
    (404, "Drawing", "annotation_graphics", "standard entity"),
    (408, "Singular Subfigure Instance", "static_assembly", "standard entity"),
    (410, "View", "presentation_style", "form 0"),
    (412, "Rectangular Array Subfigure Instance", "static_assembly", "standard entity"),
    (414, "Circular Array Subfigure Instance", "static_assembly", "standard entity"),
    (416, "External Reference", "external_reference", "standard entity"),
    (418, "Nodal Load/Constraint", "analysis_fea", "standard entity"),
    (420, "Network Subfigure Instance", "generic_semantic_graph", "standard entity"),
    (422, "Attribute Table Instance", "generic_semantic_graph", "form 0"),
    (430, "Solid Instance", "static_assembly", "standard entity"),
    (502, "Vertex List", "native_geometry", "form 1"),
    (504, "Edge List", "native_geometry", "form 1"),
    (508, "Loop", "native_geometry", "standard entity"),
    (510, "Face", "native_geometry", "standard entity"),
    (514, "Shell", "native_geometry", "standard entity"),
    (600, "MACRO Instance", "parametric_procedural", "standard entity"),
    (700, "Transformation Matrix (4x4)", "static_assembly", "BRL-CAD legacy extension"),
)

IGES_VARIANTS = (
    (106, "Copious Data", "native_geometry", "forms 1-3"),
    (106, "Linear Path", "native_geometry", "forms 11-13"),
    (106, "Centerline", "annotation_graphics", "forms 20-21"),
    (106, "Section", "native_geometry", "forms 31-38"),
    (106, "Witness Line", "annotation_graphics", "form 40"),
    (106, "Simple Closed Planar Curve", "native_geometry", "form 63"),
    (402, "Group Associativity", "generic_semantic_graph", "form 1"),
    (402, "Views Visible Associativity", "presentation_style", "form 3"),
    (402, "Views Visible/Color/Line Weight Associativity", "presentation_style", "form 4"),
    (402, "Entity Label Display Associativity", "presentation_style", "form 5"),
    (402, "Group Without Back Pointers", "generic_semantic_graph", "form 7"),
    (402, "Single Parent Associativity", "generic_semantic_graph", "form 9"),
    (402, "External Reference File Index Associativity", "external_reference", "form 12"),
    (402, "Dimensioned Geometry Associativity", "pmi_semantics", "forms 13 and 21"),
    (402, "Ordered Group with Back Pointers", "generic_semantic_graph", "form 14"),
    (402, "Ordered Group without Back Pointers", "generic_semantic_graph", "form 15"),
    (402, "Planar Associativity", "generic_semantic_graph", "form 16"),
    (402, "Flow Associativity", "generic_semantic_graph", "form 18"),
    (402, "Segmented Views Visible Associativity", "presentation_style", "form 19"),
    (402, "Piping Flow Associativity", "generic_semantic_graph", "form 20"),
    (410, "Perspective View", "presentation_style", "form 1"),
    (422, "Attribute Table Instance", "generic_semantic_graph", "form 1"),
)

IGES_PROPERTY_FORMS = (
    (1, "Definition Levels", "presentation_style"), (2, "Region Restriction", "generic_semantic_graph"), (3, "Level Function", "presentation_style"), (5, "Line Widening", "presentation_style"), (6, "Drilled Hole", "shape_feature_semantics"), (7, "Reference Designator", "generic_semantic_graph"), (8, "Pin Number", "generic_semantic_graph"), (9, "Part Number", "product_structure"), (10, "Hierarchy", "product_structure"), (11, "Tabular Data", "generic_semantic_graph"), (12, "External Reference File List", "external_reference"), (13, "Nominal Size", "pmi_semantics"), (14, "Flow Line Specification", "generic_semantic_graph"), (15, "Name", "generic_semantic_graph"), (16, "Drawing Size", "presentation_style"), (17, "Drawing Units", "quantities_units"), (18, "Intercharacter Spacing", "presentation_style"), (19, "Line Font", "presentation_style"), (20, "Highlight", "presentation_style"), (21, "Pick", "presentation_style"), (22, "Uniform Rectangular Grid", "presentation_style"), (23, "Associativity Group Type", "generic_semantic_graph"), (24, "Level to LEP Layer Map", "presentation_style"), (25, "LEP Artwork Stackup", "manufacturing_domain"), (26, "LEP Drilled Hole", "manufacturing_domain"), (27, "Generic Data", "generic_semantic_graph"), (28, "Dimension Units", "quantities_units"), (29, "Dimension Tolerance", "pmi_semantics"), (30, "Dimension Display Data", "annotation_graphics"), (31, "Basic Dimension", "pmi_semantics"), (32, "Drawing Sheet Approval", "pdm_administration"), (33, "Drawing Sheet ID", "generic_semantic_graph"), (34, "Underscore", "annotation_graphics"), (35, "Overscore", "annotation_graphics"), (36, "Closure", "annotation_graphics"),
)


def strip_comments(text):
    return LINE_COMMENT_RE.sub("", BLOCK_COMMENT_RE.sub("", text))


def type_kind(body):
    declaration = body.lstrip(" \t\r\n=").upper()
    for kind in ("SELECT", "ENUMERATION", "ARRAY", "BAG", "LIST", "SET", "STRING", "BINARY", "BOOLEAN", "LOGICAL", "INTEGER", "REAL", "NUMBER"):
        if declaration.startswith(kind):
            return kind.lower()
    return "alias"


def step_constructs():
    entities = defaultdict(lambda: {"schemas": set(), "parents": set()})
    types = defaultdict(lambda: {"schemas": set(), "kinds": set()})
    for schema, relative_path in SCHEMAS.items():
        text = strip_comments((ROOT / relative_path).read_text(encoding="utf-8", errors="replace"))
        for match in ENTITY_RE.finditer(text):
            name, body = match.group(1).lower(), match.group(2)
            parent_match = SUBTYPE_RE.search(body)
            parents = () if parent_match is None else IDENTIFIER_RE.findall(parent_match.group(1))
            entities[name]["schemas"].add(schema)
            entities[name]["parents"].update(parent.lower() for parent in parents)
        for match in TYPE_RE.finditer(text):
            name, body = match.group(1).lower(), match.group(2)
            types[name]["schemas"].add(schema)
            types[name]["kinds"].add(type_kind(body))
    return entities, types


def ancestors(name, entities):
    result, pending = set(), list(entities[name]["parents"])
    while pending:
        candidate = pending.pop()
        if candidate in result:
            continue
        result.add(candidate)
        if candidate in entities:
            pending.extend(entities[candidate]["parents"])
    return result


def classify(name, superclasses=()):
    name_space = f"_{name}_"
    text = f"{name_space}{'_'.join(sorted(superclasses))}_"
    def has(*terms):
        return any(f"_{term}_" in text for term in terms)
    if name.startswith(QUALITY_PREFIXES) or has("shape_measurement_accuracy", "software_for_data_quality_check", "criterion_report_item_with_number_of_instances", "criterion_report_item_with_value"):
        return "quality_validation"
    if name.startswith(("analysis_", "assigned_analysis", "contained_analysis")) or has("finite_element", "nodal", "element_result", "structural", "fea"):
        return "analysis_fea"
    kinematic_pair_names = (
        "actuated_kinematic_", "assembly_joint", "assembly_shape_joint", "component_feature_joint", "connection_zone_based_assembly_joint", "cylindrical_pair", "gear_pair", "high_order_kinematic_pair", "homokinetic_pair", "item_link_motion", "link_motion", "low_order_kinematic_pair", "motion_link", "pair_actuator", "planar_pair", "point_on_planar_curve_pair", "point_on_surface_pair", "prismatic_pair", "rack_and_pinion_pair", "revolute_pair", "screw_pair", "sliding_curve_pair", "spherical_pair", "surface_pair", "unconstrained_pair", "universal_pair",
    )
    if has("kinematic", "mechanism") or name.startswith(kinematic_pair_names):
        return "kinematics"
    if has("machining", "manufacturing", "additive", "tool", "operation", "mould", "mold", "casting", "laminate", "ply", "harness"):
        return "manufacturing_domain"
    if has("geometric_constraint", "assembly_constraint", "shape_constraint", "equal_parameter", "simultaneous_constraint_group", "constraint_group", "free_form_constraint", "explicit_constraint"):
        return "geometric_constraints"
    if has("geometric_tolerance", "tolerance_zone", "tolerance_value", "datum_reference", "datum_target", "datum_system", "datum_feature", "common_datum", "dimensional", "dimension", "limits_and_fits", "plus_minus_tolerance", "runout_zone", "gps", "modifier_with_value", "semantic_pmi") or name == "datum":
        return "pmi_semantics"
    if has("annotation", "draughting", "leader", "callout", "drawing", "character_glyph", "text", "symbol", "fill_area", "marker", "tile"):
        return "annotation_graphics"
    if has("tessellated", "triangulated", "complex_triangulated", "cubic_bezier_triangulated"):
        return "tessellated_geometry"
    if has("point_cloud", "scan", "scanner", "camera_image", "camera_model"):
        return "point_cloud_scan"
    if has("procedural", "functionally_defined", "defined_function", "generic_expression", "mathematical", "variable_semantics", "expression_extension", "concept_feature_operator", "expression", "function"):
        return "parametric_procedural"
    if has("shape_aspect", "feature", "profile", "hole", "pocket", "slot", "boss", "bead", "rib", "thread", "pattern", "shape_element", "shape_transition", "chamfer", "blend", "thickened", "shelled", "sculptured", "knurl") or name in {"apex", "tangent", "perpendicular_to"}:
        return "shape_feature_semantics"
    if has("material", "physical_property", "mechanical_property", "thermal_property", "optical_property"):
        return "materials"
    if has("measure", "unit", "dimensional_exponents", "precision", "uncertainty", "qualification", "qualifier", "value_format", "ratio", "conversion_based", "si_unit", "derived_unit", "named_unit"):
        return "quantities_units"
    if has("product", "assembly", "component", "usage", "occurrence", "make_from", "alternate_product", "mapped_item", "representation_map", "context_dependent_shape_representation") or name == "product":
        return "product_structure"
    if has("approval", "certification", "contract", "security", "person", "organization", "address", "date", "time", "document", "classification", "effectivity", "configuration", "event", "experience", "state", "identification", "uuid", "project", "position", "risk", "language", "application_context", "application_protocol", "environment", "external_source", "external_identification", "action"):
        return "pdm_administration"
    if has("external", "uri", "resource_identifier"):
        return "external_reference"
    if has("style", "colour", "color", "rendering", "presentation", "invisibility", "font", "line_font", "camera", "light_source", "styled_item"):
        return "presentation_style"
    if any(f"_{term}_" in name_space for term in ("representation", "represented", "item_defined", "item_identified")):
        return "representation_graph"
    if has("geometric_representation_item", "topological_representation_item", "solid_model", "surface", "curve", "cartesian", "direction", "vector", "axis", "placement", "transformation", "topological", "edge", "vertex", "loop", "shell", "face", "brep", "csg", "boolean", "swept", "revolved", "extruded", "primitive", "torus", "sphere", "cone", "cylinder", "block", "half_space", "wireframe", "manifold", "bounded", "offset", "replica", "area", "polyhedron", "hexahedron", "tetrahedron", "pyramid", "cyclide", "plane", "point", "volume"):
        return "native_geometry"
    return "generic_semantic_graph"


def iges_constructs():
    rows = list(IGES_BASE) + list(IGES_VARIANTS)
    rows.extend((406, f"Property: {name}", category, f"form {form}") for form, name, category in IGES_PROPERTY_FORMS)
    return rows


def current_iges_catalog():
    source = (ROOT / "src/conv/iges/iges_types.c").read_text(encoding="utf-8", errors="replace")
    return {int(value) for value in IGES_TYPE_RE.findall(source)}


def validate(entities, types, iges_rows):
    if len(entities) != EXPECTED_ENTITY_COUNT:
        raise RuntimeError(f"expected {EXPECTED_ENTITY_COUNT} STEP ENTITY declarations, found {len(entities)}")
    if len(types) != EXPECTED_TYPE_COUNT:
        raise RuntimeError(f"expected {EXPECTED_TYPE_COUNT} STEP TYPE declarations, found {len(types)}")
    standard_codes = {code for code, _, _, _ in iges_rows if code != 700}
    if len(standard_codes) != 90:
        raise RuntimeError(f"expected 90 IGES 5.3 entity codes, found {len(standard_codes)}")
    catalog = current_iges_catalog()
    gaps, extension = standard_codes - catalog, catalog - standard_codes
    if gaps != EXPECTED_IGES_GAPS:
        raise RuntimeError(f"unexpected IGES catalogue gaps: {sorted(gaps)}")
    if extension != EXPECTED_IGES_EXTENSION:
        raise RuntimeError(f"unexpected IGES catalogue extensions: {sorted(extension)}")
    return gaps, extension


FIELDS = (
    "format",
    "source_families",
    "construct_kind",
    "identifier",
    "form_or_definition",
    "semantic_family",
    "native_status",
    "native_targets",
    "recommended_disposition",
    "catalogue_status",
)


def matrix(entities, types, iges_rows):
    result = []
    for name in sorted(entities):
        entity = entities[name]
        category = classify(name, ancestors(name, entities))
        status, targets, disposition = CATEGORY_INFO[category]
        result.append(
            {
                "format": "STEP",
                "source_families": ";".join(sorted(entity["schemas"])),
                "construct_kind": "ENTITY",
                "identifier": name,
                "form_or_definition": ";".join(sorted(entity["parents"])) or "root ENTITY",
                "semantic_family": category,
                "native_status": status,
                "native_targets": targets,
                "recommended_disposition": disposition,
                "catalogue_status": "schema-declared",
            }
        )
    for name in sorted(types):
        type_record = types[name]
        category = classify(name)
        status, targets, disposition = CATEGORY_INFO[category]
        result.append(
            {
                "format": "STEP",
                "source_families": ";".join(sorted(type_record["schemas"])),
                "construct_kind": "TYPE",
                "identifier": name,
                "form_or_definition": ";".join(sorted(type_record["kinds"])),
                "semantic_family": category,
                "native_status": status,
                "native_targets": targets,
                "recommended_disposition": disposition,
                "catalogue_status": "schema-declared",
            }
        )
    catalog = current_iges_catalog()
    for code, name, category, forms in iges_rows:
        status, targets, disposition = CATEGORY_INFO[category]
        standard = code != 700
        catalogue_status = "BRL-CAD-extension" if not standard else "listed" if code in catalog else "not-listed"
        if code == 0:
            catalogue_status = "listed-as-unknown"
        result.append(
            {
                "format": "IGES",
                "source_families": "IGES-5.3" if standard else "BRL-CAD legacy extension",
                "construct_kind": "ENTITY-TYPE",
                "identifier": str(code),
                "form_or_definition": f"{name}; {forms}",
                "semantic_family": category,
                "native_status": status,
                "native_targets": targets,
                "recommended_disposition": disposition,
                "catalogue_status": catalogue_status,
            }
        )
    return result


def summary(entities, types, iges_rows, gaps, extension, rows):
    print(f"STEP ENTITY declarations: {len(entities)}")
    print(f"STEP TYPE declarations: {len(types)}")
    print(f"IGES 5.3 entity codes: {len({code for code, _, _, _ in iges_rows if code != 700})}")
    print(f"IGES form-specific rows: {len(iges_rows)}")
    print(f"IGES legacy type-table gaps: {', '.join(str(code) for code in sorted(gaps))}")
    print(f"IGES legacy type-table extensions: {', '.join(str(code) for code in sorted(extension))}")
    print("Semantic family counts:")
    for family, count in sorted(Counter(row["semantic_family"] for row in rows).items()):
        print(f"  {family}: {count}")
    print("Native status counts:")
    for status, count in sorted(Counter(row["native_status"] for row in rows).items()):
        print(f"  {status}: {count}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--format", choices=("summary", "csv"), default="summary")
    parser.add_argument("--check", action="store_true")
    arguments = parser.parse_args()
    entities, types = step_constructs()
    iges_rows = iges_constructs()
    gaps, extension = validate(entities, types, iges_rows)
    rows = matrix(entities, types, iges_rows)
    if arguments.format == "csv":
        writer = csv.DictWriter(sys.stdout, fieldnames=FIELDS, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    else:
        summary(entities, types, iges_rows, gaps, extension, rows)
    if arguments.check:
        print("Inventory check: PASS")


if __name__ == "__main__":
    main()
