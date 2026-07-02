#!/bin/sh
set -eu

cmake_bin=$1
src_dir=$2
bin_dir=$3

test_dir="$bin_dir/changes-rules-test"
rm -rf "$test_dir"
mkdir -p "$test_dir"

rules="$test_dir/minimally-impacting.rules"
cat > "$rules" <<'RULES'
RENAME FUNCTION old_func TO new_func SINCE test # function rename
RENAME MACRO OLD_MACRO TO NEW_MACRO SINCE test
RENAME HEADER old.h TO new/header.h SINCE test
RENAME FORM "legacy_pair\\(([^,]*), ([^)]*)\\)" TO "modern_pair(\\2, \\1)" SINCE test
DROP FUNCTION sample PARAMS 2 SINCE test
ADD FUNCTION sample PARAMS 2=DEFAULT_VALUE SINCE test
MOVE FUNCTION shuffle PARAMS 1,2,3 TO 3,1,2 SINCE test
DROP CALL old_noop SINCE test
RULES

"$cmake_bin" \
  -DCHANGES_SOURCE_DIR="$src_dir/../.." \
  -DCHANGES_BINARY_DIR="$test_dir" \
  -DCHANGES_RULES_FILE="$rules" \
  -DCHANGES_APPLY_TEMPLATE="$src_dir/changes_apply_rules.sh.in" \
  -DCHANGES_GENERATED_ADOC="$test_dir/doc/changes/minimally-impacting.adoc" \
  -DCHANGES_GENERATED_SCRIPT="$test_dir/doc/changes/changes_apply_rules.sh" \
  -P "$src_dir/changes_generate_rules.cmake"

sample="$test_dir/sample.c"
cat > "$sample" <<'SRC'
#include "old.h"
int a = OLD_MACRO;
old_noop();
old_func(1);
legacy_pair(left, right);
sample(first, second, third);
shuffle(one, two, three);
SRC

if "$test_dir/doc/changes/changes_apply_rules.sh" --check "$sample"; then
  echo "dirty file should fail --check" >&2
  exit 1
fi

"$test_dir/doc/changes/changes_apply_rules.sh" "$sample"

grep '#include "new/header.h"' "$sample" >/dev/null
grep 'NEW_MACRO' "$sample" >/dev/null
grep 'new_func(1)' "$sample" >/dev/null
grep 'modern_pair(right, left)' "$sample" >/dev/null
grep 'sample(first, DEFAULT_VALUE, third)' "$sample" >/dev/null
grep 'shuffle(three, one, two)' "$sample" >/dev/null
if grep 'old_noop' "$sample" >/dev/null; then
  echo "DROP CALL did not remove old_noop" >&2
  exit 1
fi
