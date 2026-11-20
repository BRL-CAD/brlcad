/*                         M I M E . H
 * BRL-CAD
 *
 * Copyright (c) 2004-2026 United States Government as represented by
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

#ifndef BU_MIME_RUNTIME_H
#define BU_MIME_RUNTIME_H

#include "common.h"
#include "bu/defines.h"
#include "bu/vls.h"

__BEGIN_DECLS

/** Runtime MIME descriptors use canonical type/subtype strings.
 * Deprecated enum adapters are declared separately in bu/mime.h. */
#define BU_MIME_ABI_VERSION 3
/** Default maximum prefix inspected (64 KiB). */
#define BU_MIME_DEFAULT_SCAN_LIMIT (64 * 1024)
#define BU_MIME_DEFAULT_PRIORITY 50
#define BU_MIME_MAX_PRIORITY 100

struct bu_mime_registry;

/** Owned metadata snapshot. Arrays are sorted and deduplicated. */
struct bu_mime_info {
    const char *name;
    const char *description;
    const char **aliases;
    size_t alias_count;
    const char **extensions;
    size_t extension_count;
};

/** Copy metadata for name (including aliases), or all types when name is
 * NULL. Return 1 on success, including an empty result, 0 on invalid
 * arguments. Snapshots survive registry changes and destruction. */
BU_EXPORT extern int bu_mime_list(struct bu_mime_registry *registry,
    const char *name, struct bu_mime_info **info, size_t *count);
BU_EXPORT extern void bu_mime_list_free(struct bu_mime_info *info, size_t count);

/** Evidence strength, independent of preference between handlers. */
enum bu_mime_evidence {
    BU_MIME_EVIDENCE_EXTENSION = 1,
    BU_MIME_EVIDENCE_HEURISTIC,
    BU_MIME_EVIDENCE_SIGNATURE
};

/** Match value bytes at any offset in the inclusive [offset, last_offset]
 * range.  A NULL mask compares all bits; otherwise mask has length bytes.
 * Integer signatures are expressed as bytes in the format's byte order.
 * Tests never read beyond the registry's scan limit. */
struct bu_mime_test {
    size_t offset;
    size_t last_offset;
    const unsigned char *value;
    const unsigned char *mask;
    size_t length;
};

/** All tests must match.  A definition's rules are alternatives (OR).
 * Priority breaks ties within an evidence class, from 0 to
 * BU_MIME_MAX_PRIORITY. */
struct bu_mime_rule {
    const struct bu_mime_test *tests;
    size_t test_count;
    enum bu_mime_evidence evidence;
    unsigned int priority;
};

/** One contributor's definition.  Registration copies all referenced data.
 * Extensions omit the dot and may contain multiple components (tar.gz).
 * MIME names and extensions use ASCII case-insensitive comparisons.
 * Aliases resolve directly to name; conflicting aliases are rejected.
 * Names contain type/subtype only, without parameters. */
struct bu_mime_definition {
    unsigned int abi_version;
    size_t struct_size;
    const char *name;
    const char *description;
    const char *const *aliases;
    size_t alias_count;
    const char *const *extensions;
    size_t extension_count;
    const struct bu_mime_rule *rules;
    size_t rule_count;
};

/** A token identifies one contribution within its registry.  Zero is invalid. */
typedef uint64_t bu_mime_token;

enum bu_mime_value_kind {
    BU_MIME_VALUE_STRING = 0,
    BU_MIME_VALUE_SET
};

/** Format traits refine a MIME identity; they are not MIME parameters.
 * Keys use lowercase ASCII letters, digits, dot, dash and underscore.
 * Strings and set members are nonempty and case-sensitive. A set is a
 * complete observation; omit the key while its membership is unknown.
 * An empty set is known to contain no members. String traits use value;
 * set traits use members/member_count and have a NULL value. */
struct bu_mime_trait {
    const char *key;
    const char *value;
    enum bu_mime_value_kind kind;
    const char *const *members;
    size_t member_count;
};
#define BU_MIME_TRAIT_STRING(_key, _value) {_key, _value, BU_MIME_VALUE_STRING, NULL, 0}
#define BU_MIME_TRAIT_SET(_key, _members, _count) {_key, NULL, BU_MIME_VALUE_SET, _members, _count}

/** Zero-initialize before using the allocation APIs. Owned traits survive
 * registry/context changes. A borrowed view may refer to a static array;
 * do not free or modify that view with the allocation APIs. */
struct bu_mime_traits {
    const struct bu_mime_trait *items;
    size_t count;
};
#define BU_MIME_TRAITS_INIT_ZERO {NULL, 0}

/** bu_opt callback: append key=value or key[]=["member",...] (JSON array).
 * Repeated identical values are allowed; conflicting requests are errors. */
BU_EXPORT int bu_opt_mime_trait(struct bu_vls *messages, size_t argc,
    const char **argv, void *set_var);

/** Copy and append a fact. Repeated identical facts are deduplicated;
 * conflicting observations are retained, so get returns NULL for that key.
 * Return 0 for invalid arguments, leaving the destination unchanged. */
BU_EXPORT int bu_mime_traits_add(struct bu_mime_traits *traits, const char *key, const char *value);
/** Copy a complete set, sorting and deduplicating its members. An empty
 * set uses NULL members and count zero. Different observations of the
 * same key are retained as contradictions, as with string traits. */
BU_EXPORT int bu_mime_traits_add_set(struct bu_mime_traits *traits, const char *key,
    const char *const *members, size_t count);
BU_EXPORT int bu_mime_traits_copy(struct bu_mime_traits *destination, const struct bu_mime_traits *source);
BU_EXPORT void bu_mime_traits_free(struct bu_mime_traits *traits);
/** Borrow a string value, or NULL if absent, a set, or contradicted. */
BU_EXPORT const char *bu_mime_traits_get(const struct bu_mime_traits *traits, const char *key);
/** Borrow a complete set. Return 1 when known, including the empty set;
 * return 0 for an absent, string or contradicted key. Outputs are cleared
 * on failure and remain valid for the lifetime of the source collection. */
BU_EXPORT int bu_mime_traits_get_set(const struct bu_mime_traits *traits, const char *key,
    const char *const **members, size_t *count);
/** Validate syntax and, when unambiguous is nonzero, reject conflicting keys. */
BU_EXPORT int bu_mime_traits_valid(const struct bu_mime_traits *traits, int unambiguous);
/** Return 1 when every requested fact is established exactly, including
 * complete sets. Supplemental observations supply only keys absent from
 * observed; a present but conflicting key cannot fall back to supplemental.
 * Missing or contradictory requested facts fail. NULL/empty requests impose
 * no requirements. All collections must be syntactically valid and requests
 * must be unambiguous. Neither capabilities nor resolved selection defaults
 * are observations. Readers must reject failed source validation before
 * treating an absent key as available for supplementation. */
BU_EXPORT int bu_mime_traits_match(const struct bu_mime_traits *observed,
    const struct bu_mime_traits *supplemental, const struct bu_mime_traits *requested);
/** Append traits, including conflicting observations, for diagnostics. */
BU_EXPORT void bu_mime_traits_describe(const struct bu_mime_traits *traits, struct bu_vls *messages);

/** Compare nonnegative decimal versions with dot-separated components.
 * Missing trailing components are zero: 2, 2.0 and 02.00 compare equal.
 * Return 1 and set comparison to -1, 0 or 1; return 0 for invalid syntax,
 * clearing comparison. No suffixes or format compatibility are inferred.
 * This does not change the exact matching of string capability traits. */
BU_EXPORT int bu_mime_version_compare(const char *left, const char *right, int *comparison);

/** Common capability operation bits. A zero capability mask applies to
 * every operation implemented by its handler/codec. */
enum bu_mime_operation {
    BU_MIME_READ = 1,
    BU_MIME_READ_MEMORY = 2,
    BU_MIME_WRITE = 4,
    BU_MIME_WRITE_MEMORY = 8,
    BU_MIME_METADATA = 16,
    BU_MIME_READ_SEQUENCE = 32,
    BU_MIME_WRITE_SEQUENCE = 64
};

/** Build resolved selection traits from observations and explicit requests.
 * Contradictions involving a requested key fail. A missing key may be supplied
 * provisionally for selection; it does not become an observed source fact.
 * Readers use bu_mime_traits_match with independent facts for input validation.
 * Unrequested conflicting observations remain in the resolved collection.
 * Destination is unchanged on failure. */
BU_EXPORT int bu_mime_traits_resolve(struct bu_mime_traits *destination,
    const struct bu_mime_traits *observed, const struct bu_mime_traits *requested);

enum bu_mime_capability_flags {
    /** Missing input facts may be checked by the full reader. Known
     * contradictions and unsupported values still fail selection. A
     * handler declaring this must verify its constraints before producing
     * converted data; prefix inspection alone need not establish them. */
    BU_MIME_DEFER_INPUT_VALIDATION = 1
};

/** One AND clause. Multiple clauses for the same type are alternatives.
 * Clauses may differ per MIME type in a multi-format implementation.
 * A clause with no traits accepts any input variant, including unknown.
 * String constraints require equality. A set constraint lists supported
 * members: the file's complete set must be known and a subset of it.
 * For output, every requested trait must be explicitly supported by a
 * matching clause; requested sets must be subsets of supported members.
 * Empty clauses describe only default output. Unrequested set constraints
 * do not select output features; the writer retains its default policy.
 * Libraries copy descriptors, including trait strings, at registration. */
struct bu_mime_capability {
    const char *mime_type;
    const struct bu_mime_trait *traits;
    size_t trait_count;
    unsigned int operations;
    unsigned int flags;
};
#define BU_MIME_CAPABILITY(_type, _traits, _count, _operations) {_type, _traits, _count, _operations, 0}

/** Copy/free one owned capability. Initialize destination to zero before
 * copying. Copies include the MIME name and every trait string. */
BU_EXPORT int bu_mime_capability_copy(struct bu_mime_capability *destination,
    const struct bu_mime_capability *source);
BU_EXPORT void bu_mime_capability_free(struct bu_mime_capability *capability);

enum bu_mime_direction {
    BU_MIME_INPUT,
    BU_MIME_OUTPUT
};
/** Input requirements must be known and satisfied unless the reader
 * explicitly defers missing facts to full validation. Conflicting facts
 * never satisfy constraints. Output requests must all be acknowledged.
 * No regexes, implicit coercion or wildcard values. */
BU_EXPORT int bu_mime_capability_match(const struct bu_mime_capability *capability,
    const struct bu_mime_traits *traits, enum bu_mime_direction direction);

struct bu_mime_candidate {
    const char *name;
    enum bu_mime_evidence evidence;
    unsigned int priority;
    struct bu_mime_traits traits;
};

/** Owned detection result, independent of subsequent registry changes.
 * Candidates are ordered by evidence, then priority, then canonical name.
 * incomplete means additional bytes could change the result. */
struct bu_mime_result {
    struct bu_mime_candidate *candidates;
    size_t count;
    int incomplete;
};

enum bu_mime_status {
    BU_MIME_ERROR = -1,
    BU_MIME_UNKNOWN_RESULT = 0,
    BU_MIME_MATCH,
    BU_MIME_AMBIGUOUS
};

/** Read-only input for cooperative, bounded library-specific probes.
 * Probes inspect only these bytes and metadata; they must not perform I/O
 * or decode the full document.  file_size is meaningful only when known. */
struct bu_mime_probe_input {
    const unsigned char *data;
    size_t size;
    int complete;
    uint64_t file_size;
    int file_size_known;
};

enum bu_mime_probe_status {
    BU_MIME_PROBE_ERROR = -1,
    BU_MIME_PROBE_NO_MATCH = 0,
    BU_MIME_PROBE_HEURISTIC,
    BU_MIME_PROBE_SIGNATURE,
    BU_MIME_PROBE_INCOMPLETE
};

/** Merge library-specific evidence into an owned result.  The name is
 * resolved through registry aliases and copied.  Priority concerns format
 * evidence, independently of handler preference.  Return the updated match
 * status or BU_MIME_ERROR without changing result for invalid arguments. */
BU_EXPORT extern enum bu_mime_status bu_mime_result_add(struct bu_mime_registry *registry,
							struct bu_mime_result *result, const char *name,
							enum bu_mime_evidence evidence, unsigned int priority);

/** Resolve one matching capability. For a nonempty output request, fill
 * unrequested traits from the clause so callbacks can honor its complete
 * configuration. An empty request preserves the implementation's normal
 * defaults. Clause order defines defaults for unrequested output choices. */
BU_EXPORT int bu_mime_capability_resolve(const struct bu_mime_capability *capability,
    const struct bu_mime_traits *traits, enum bu_mime_direction direction,
    struct bu_mime_traits *resolved);

/** Merge copied observations into an existing canonical candidate. Conflicts
 * remain visible and do not satisfy constrained handlers. Return 1 on success,
 * 0 for invalid facts or a candidate absent from the result. */
BU_EXPORT int bu_mime_result_add_traits(struct bu_mime_result *result, const char *canonical,
    const struct bu_mime_traits *traits);

/** Detect using a bounded prefix and any available file-size metadata. */
BU_EXPORT enum bu_mime_status bu_mime_identify(struct bu_mime_registry *registry,
    const char *filename, const struct bu_mime_probe_input *input,
    struct bu_mime_result **result);

/** Create a registry seeded with built-in definitions.  A zero scan_limit
 * selects BU_MIME_DEFAULT_SCAN_LIMIT.  Registrations and lookups are
 * synchronized; destruction requires exclusive ownership. */
BU_EXPORT extern struct bu_mime_registry *bu_mime_registry_create(size_t scan_limit);
BU_EXPORT extern void bu_mime_registry_destroy(struct bu_mime_registry *registry);
/** Return the immutable maximum prefix length, or zero for NULL. */
BU_EXPORT extern size_t bu_mime_scan_limit(const struct bu_mime_registry *registry);

/** Add a contribution atomically.  Return zero on invalid descriptors or
 * conflicts; otherwise return its removal token.  Built-ins cannot be removed. */
BU_EXPORT extern bu_mime_token bu_mime_register(struct bu_mime_registry *registry,
						const struct bu_mime_definition *definition);
/** Return 1 if removed, 0 for an unknown or built-in token. */
BU_EXPORT extern int bu_mime_unregister(struct bu_mime_registry *registry, bu_mime_token token);

/** Resolve a name or alias into canonical, returning 1 on success and 0
 * if unrecognized.  Output is cleared on failure. */
BU_EXPORT extern int bu_mime_canonical(struct bu_mime_registry *registry,
				       const char *name, struct bu_vls *canonical);
/** Copy the most recently contributed nonempty description.  Return 1 for
 * a recognized type (possibly with an empty description), 0 otherwise. */
BU_EXPORT extern int bu_mime_description(struct bu_mime_registry *registry,
					 const char *name, struct bu_vls *description);

/** Resolve a MIME name/alias or extension to all associated identities.
 * Return an owned result (also when unknown), or NULL on error. Candidate
 * evidence is EXTENSION, since this is a name lookup, not inspection.
 * Contexts may narrow ambiguous extensions using available capabilities. */
BU_EXPORT enum bu_mime_status bu_mime_resolve_candidates(struct bu_mime_registry *registry,
    const char *format, struct bu_mime_result **result);

/** Resolve an explicit MIME name, alias, or extension (without a dot).
 * Output is cleared unless a unique type is found. */
BU_EXPORT extern enum bu_mime_status bu_mime_resolve(struct bu_mime_registry *registry,
    const char *format, struct bu_vls *canonical);

/** Detect from a filename and/or bytes.  complete means the buffer contains
 * the entire input, not merely a prefix.  NULL data requests filename-only
 * lookup.  Pass a non-NULL buffer of length zero for an empty input.
 * On success (including unknown), *result must be freed by the caller.
 * A unique highest-ranked candidate gives BU_MIME_MATCH; tied highest
 * candidates give BU_MIME_AMBIGUOUS.  incomplete is independent of status.
 * On error *result is NULL. */
BU_EXPORT extern enum bu_mime_status bu_mime_detect(struct bu_mime_registry *registry,
						    const char *filename, const void *data, size_t size, int complete,
						    struct bu_mime_result **result);
/** Open a file and inspect at most the configured prefix limit.  I/O errors
 * give BU_MIME_ERROR, not an extension-only success. */
BU_EXPORT extern enum bu_mime_status bu_mime_detect_file(struct bu_mime_registry *registry,
							 const char *filename, struct bu_mime_result **result);
BU_EXPORT extern void bu_mime_result_free(struct bu_mime_result *result);
/** Append candidate names, evidence, priorities, and inspection limits to
 * messages. A NULL result is reported as unavailable. */
BU_EXPORT extern void bu_mime_result_describe(const struct bu_mime_result *result,
    struct bu_vls *messages);


__END_DECLS

#endif
