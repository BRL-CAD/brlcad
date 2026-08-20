# Configure-probe differential validation

BRL-CAD's configure-time feature tests (headers, functions, struct members,
type sizes, compiler/linker flags, source-compile/run checks) can run two ways:

* **Reference path** - one `try_compile`/`try_run` per test, driven by the main
  generator. Slow, but the long-trusted behavior. Selected with
  `-DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=OFF`.
* **Batched path** - many probes compiled together in generated mini-projects
  and built in parallel. Much faster, and the basis for the planned move to a
  Ninja-driven probe build on Windows. Selected with
  `-DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=ON` (the default).

Speeding these up is only acceptable if the results are **identical**. This
harness makes that a checked invariant rather than an assumption: it captures
every configure result and diffs the reference path against the fast path.

## How it works

1. When configured with `-DBRLCAD_PROBE_SNAPSHOT_FILE=<path>`, the build writes
   a snapshot of every CMake cache variable (the store every `check_*` result
   lands in) to `<path>`, sorted, one `NAME=VALUE` record per line, with a
   `#`-prefixed metadata header (generator, toolset, compiler, probe mode).
   See [`../BRLCAD_Probe_Validation.cmake`](../BRLCAD_Probe_Validation.cmake).
   Normal builds set nothing and pay no cost.
2. Build the native C++17 tool in this directory:

```sh
cmake -S misc/CMake/probe_validation -B .build/probe_validation-tool -G Ninja
cmake --build .build/probe_validation-tool
```

3. [`brlcad_probe_validation`](.build/probe_validation-tool/brlcad_probe_validation)
   compares either snapshots, whole configured build trees, or runs the two
   configures for you and then performs both comparisons.

Because a full configure is expensive, run this when changing the
probe/batch/generator logic - not on every build.

## Comparison modes

* `diff-snapshots --strict` compares **every** cache variable except known
  generator/toolchain/path plumbing. Use it when both configures use the **same
  generator** (phase 1: batch vs. reference), so anything that differs at all is
  caught.
* `diff-snapshots` default (**result-pattern**) mode compares only variables whose names look
  like feature-test results (`HAVE_*`, `SIZEOF_*`, `WORKING_*`, `*_FLAG_FOUND`,
  `*_COMPILE`, `*_FLAG`). Use it across **different generators** (phase 2:
  Ninja vs. MSBuild), where plumbing legitimately differs. Extend with
  `--pattern REGEX` if a new check introduces a new naming convention.
* `compare-dirs` recursively compares the full configured build trees after
  normalizing build-root and source-root paths. This is the broadest check and
  is the mode to use when comparing one source branch against another.

## Usage

### Automated (either phase)

```sh
# Phase 1 - batch vs. reference under the same generator (auto-selects --strict)
.build/probe_validation-tool/brlcad_probe_validation run \
    --source /path/to/brlcad \
    --work-dir /path/to/scratch \
    -- -G "Visual Studio 17 2022" -A x64 -DBRLCAD_EXT_DIR=/path/to/bext

# Phase 2 - candidate under Ninja, reference under Visual Studio
.build/probe_validation-tool/brlcad_probe_validation run \
    --source /path/to/brlcad \
    --work-dir /path/to/scratch \
    --candidate-generator Ninja \
    -- -G "Visual Studio 17 2022" -A x64 -DBRLCAD_EXT_DIR=/path/to/bext

# Branch-to-branch - compare main against a modified tree
.build/probe_validation-tool/brlcad_probe_validation run \
    --reference-source /path/to/brlcad-main \
    --candidate-source /path/to/brlcad-work \
    --work-dir /path/to/scratch \
    --reference-generator Ninja \
    --candidate-generator Ninja \
    -- -DBRLCAD_EXT_DIR=/path/to/bext -DCMAKE_BUILD_TYPE=Debug
```

Everything after `--` is passed verbatim to **both** configures - put your
normal arguments there. The reference build always gets
`BRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=OFF`; the candidate gets `ON`. Re-run just
the comparison over existing builds with `--skip-configure`.  When comparing two
different source trees, the tool always runs a full configured-tree diff and
will run a snapshot diff as well if both trees support snapshot emission.

### Manual

```sh
# Reference (trusted try_compile path)
cmake -S /path/to/brlcad -B build-ref <your args> \
    -DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=OFF \
    -DBRLCAD_PROBE_SNAPSHOT_FILE=$PWD/ref.txt

# Candidate (fast path, optionally -G Ninja)
cmake -S /path/to/brlcad -B build-cand <your args> \
    -DBRLCAD_ENABLE_PARALLEL_CONFIG_PROBES=ON \
    -DBRLCAD_PROBE_SNAPSHOT_FILE=$PWD/cand.txt

.build/probe_validation-tool/brlcad_probe_validation diff-snapshots ref.txt cand.txt --strict
.build/probe_validation-tool/brlcad_probe_validation diff-snapshots ref.txt cand.txt
.build/probe_validation-tool/brlcad_probe_validation compare-dirs build-ref build-cand
```

A `PASS` means the fast path reproduced every compared result. A `FAIL` lists
each divergence as a value mismatch, a reference-only result, or a
candidate-only result.  For configured-tree comparisons it also reports files
that changed, were added, or were removed.  Reconcile these before trusting the
faster path.
