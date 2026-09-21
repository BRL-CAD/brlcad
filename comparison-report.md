# GQA sampling-method comparison

## Executive result

The new methods are a substantial practical improvement, but Crofton is not a
uniform numerical improvement over grids.

- **Crofton QMC is the best general-purpose stochastic method.**  It is far
  more accurate than ordinary random Crofton at the same ray count, avoids the
  severe orientation bias seen in the legacy grid on thin or oblique models,
  supports uncertainty intervals, and reached a 1% volume-interval target on
  every bounded `share/db` case in the 10-second study.
- **Rotated grid is the strongest deterministic control.**  It converges very
  quickly on the analytic fixtures and usually agrees closely with Crofton on
  complex-model volume, centroid, and inertia.  It is not uniformly reliable:
  the full Generic Twin surface-area result remains about 6.5% above the two
  Crofton results.
- **Legacy axis-aligned GQA remains useful for compatibility, not truth.**  It
  can be excellent on aligned or symmetric geometry, but agreement among its
  three axial views does not detect shared grid bias.  Havoc volume is 18.2%
  above the new-method consensus at the finest practical legacy grid tested.
- **Ordinary random Crofton is primarily a control.**  Its observed error falls
  close to the expected inverse-square-root rate, but QMC needs substantially
  fewer rays for the same precision on these tests.

The recommended experimental default is therefore time-first QMC with an
explicit accuracy status, retaining rotated grid and random Crofton as
diagnostic controls.  Legacy output should remain available as a compatibility
and comparison control.  The experimental path now supplies conservative
bounds, traditional Plot3 files, bounded representative plots, and incremental
MGED visualization.

## Test protocol

- Analytic fixtures: a 1000 mm cube and radius-1000 mm sphere.
- Solid density: synthetic 1 g/cm^3.  Consequently mass relative error equals
  volume relative error.  The air fixture uses 0.001 g/cm^3 for modeled air.
- Random and QMC convergence values are medians across ten fixed outer seeds.
- Comparable-effort analytic table: 76,800 legacy rays, about 60,000 rotated
  rays, and 100,000 random/QMC rays.
- Complex-model comparison: the finest practical legacy grid, the deepest
  completed rotated grid, and fixed high-ray Crofton runs.  Those are
  stabilization comparisons, not strictly equal-work comparisons.
- The four complex models are Havoc, the M35 cab, wrapped `Goliath.c`, and the
  complete Generic Twin `all` hierarchy.
- Complex models have no analytic truth.  “Consensus” below is the arithmetic
  mean of the three new-method values and is only a comparison reference.

For uncertainty, the implementation uses independent randomized replications,
following the design guidance in A. B. Owen, [“Error estimation for quasi-Monte
Carlo” (2025), section 5](https://arxiv.org/abs/2501.00150v3).  The pilot
forecast and its safety factor are engineering heuristics, not results
attributed to Owen.  Only fresh production replications determine the reported
interval.

## Analytic measurement accuracy

All entries are errors; lower is better.  Experimental inertia tensors were
translated from the model origin to the object centroid before comparison with
legacy output.

| Output | Fixture | Legacy axis grid | Rotated grid | Crofton random | Crofton QMC | Result |
|---|---|---:|---:|---:|---:|---|
| Volume | Cube | **0%** | 0.00392% | 0.18574% | 0.00674% | Legacy wins this specially aligned case |
| Volume | Sphere | 0.01170% | 0.00679% | 0.24152% | **0.00211%** | QMC improves on legacy by 82.0% |
| Mass | Cube | **0%** | 0.00392% | 0.18574% | 0.00674% | Same relative result as volume |
| Mass | Sphere | 0.01170% | 0.00679% | 0.24152% | **0.00211%** | Same relative result as volume |
| Surface area | Cube | unavailable | **0.00468%** | 0.12256% | 0.03220% | New capability; rotated wins |
| Surface area | Sphere | unavailable | 0.01267% | 0.39150% | **0.00200%** | New capability; QMC wins |
| Centroid | Cube | 3.6079 mm | **0.0102 mm** | 1.1650 mm | 0.1952 mm | Rotated improves on legacy by 99.7% |
| Centroid | Sphere | **approximately 0** | 0.0261 mm | 3.0817 mm | 0.1139 mm | Axial symmetry gives legacy exact cancellation |
| Inertia | Cube | 0.00276% | **0.00224%** | 0.32629% | 0.02884% | Rotated improves on legacy by 18.8% |
| Inertia | Sphere | 0.01534% | **0.01222%** | 0.42519% | 0.01539% | QMC is effectively tied; rotated wins |

Selected relative changes make the tradeoff clearer:

| Comparison at comparable effort | Change relative to legacy error |
|---|---:|
| Sphere volume, rotated | 41.9% lower |
| Sphere volume, QMC | 82.0% lower |
| Sphere volume, random | 1,964% higher |
| Cube centroid, rotated | 99.7% lower |
| Cube centroid, QMC | 94.6% lower |
| Cube inertia, QMC | 944% higher |
| Cube inertia, random | 11,713% higher |
| Sphere inertia, rotated | 20.4% lower |

The apparent contradictions are real sampling effects.  An aligned grid can
integrate an aligned box volume exactly while placing its estimated centroid
at cell centers.  Symmetric axial views can cancel sphere centroid error.  That
does not make axis-aligned sampling uniformly more accurate on arbitrary
geometry.

## Convergence rates

The fitted relation is `error ~ rays^slope`; a more negative slope indicates
faster observed error reduction.  Fits span the measured sampling ladders and
should not be extrapolated as guarantees.

| Method | Volume slope | Area slope | Centroid slope | Inertia slope |
|---|---:|---:|---:|---:|
| Legacy axis grid | -1.025 on sphere; cube exact | unavailable | -0.500 on cube; sphere cancels | -1.000 to -1.037 |
| Rotated grid | -0.650 to -0.918 | -0.673 to -0.681 | -0.934 to -1.118 | -0.863 to -0.898 |
| Crofton random | -0.460 to -0.462 | -0.480 to -0.487 | -0.487 to -0.531 | -0.480 to -0.546 |
| Crofton QMC | -0.576 to -0.696 | -0.573 to -0.753 | -0.786 to -0.844 | -0.757 to -0.834 |

Random Crofton behaves close to inverse square root.  QMC improves every
measured random-Crofton slope.  Grid convergence is faster on these smooth
fixtures, but its result can plateau around an orientation-specific bias.

The next table reports the first tested sampling rung meeting each threshold
on **both** analytic fixtures.  A dash means the threshold was not reached by
the largest tested rung.

| Method | Volume 1% / 0.1% / 0.01% | Area 1% / 0.1% / 0.01% | Centroid 1 mm / 0.1 mm | Inertia 1% / 0.1% / 0.01% |
|---|---:|---:|---:|---:|
| Legacy axis grid | 1,200 / 19,200 / 307,200 | — | 4,915,200 / — | 1,200 / 19,200 / 307,200 |
| Rotated grid | 2,940 / 14,373 / 239,005 | 2,940 / 59,501 / 954,145 | 2,940 / 59,501 | 2,940 / 14,373 / 239,005 |
| Crofton random | 25,000 / 1,600,000 / — | 25,000 / 800,000 / — | 800,000 / — | 25,000 / 3,200,000 / — |
| Crofton QMC | 12,500 / 12,500 / 100,000 | 12,500 / 50,000 / 1,600,000 | 25,000 / 400,000 | 12,500 / 25,000 / 800,000 |

## Time-first and adaptive behavior on `share/db`

### Ten seconds or a 1% interval

The implemented pilot/forecast/production planner was run on all 138 bounded,
non-half-containing roots from the original corpus.  The requested result was
a pointwise 95% volume-interval half-width no larger than 1%, with a 10-second
deadline and no ray cap.

| Outcome | Result |
|---|---:|
| Target met | **138 / 138** |
| Median rays | 300,000 |
| 90th-percentile rays | 1,863,584 |
| Maximum rays | 45,943,072 |
| Median time | 0.11 s |
| 90th-percentile time | 1.41 s |
| 95th-percentile time | 2.53 s |
| Maximum time | 8.40 s |
| Median relative half-width | 0.368% |
| 90th-percentile relative half-width | 0.652% |
| Maximum relative half-width | 0.896% |

This replaces the misleading earlier 100,000-total-ray result, where only
49/138 met 1% because the cap also paid for a discarded pilot and often left
only 25,000–75,000 production rays.  Ray count varies by more than two orders
of magnitude across the corpus, while all wall times remained under the
declared deadline.  That strongly supports time as the primary user-facing
resource control.

### Automatic dimensional stability with a 60-second default

The dimensional stability rule used a 0.00005 mm target, scaled downward for
very small objects, and required two stable doubling comparisons.

| Outcome | Result |
|---|---:|
| Stopped stable before deadline | 23 / 138 |
| Reached 60-second limit | 115 / 138 |
| Mean rays | 104.2 million |
| Within the independent adaptive result's 95% interval | 129 / 138 |
| Median difference from adaptive point estimate | 0.100% |
| 90th-percentile difference | 0.334% |
| 95th-percentile difference | 0.454% |
| Maximum difference | 1.204% |

The independent adaptive interval is not analytic truth, so the interval count
is an agreement check rather than a coverage claim.  The practical conclusion
is still clear: the modeling-distance stability floor is much stricter than a
1% answer requirement for most corpus objects.  It is valuable as an absolute
terminal criterion, but using it alone makes the default consume the full
minute in 83.3% of cases.

### Legacy comparison over the corpus

At the earlier 100,000-ray cap, 123 roots produced paired legacy and Crofton
volumes.  Median relative difference was 0.544%, the 90th percentile was
3.573%, and 113/123 were within 5%.  Fifteen legacy runs exceeded 120 seconds;
all had fast Crofton answers.  Analytic checks on notable disagreements favor
the new estimator:

| Model | Analytic volume (mm^3) | Legacy error | Crofton error |
|---|---:|---:|---:|
| `pic.g/box.r` | 1.17368243 | -10.126% | -0.175% |
| `cube.g/light.r` | 6.08899047 | +1.940% | -0.0146% |
| `lgt-test.g/box.r` | 19,233.2255 | +2.496% | -0.175% |

## Large complex models

Volumes are in billions of mm^3.  Legacy uses its finest practical tested grid;
Havoc, M35, and Goliath use rotated level 3 and 3.2 million Crofton rays.  The
full Generic Twin uses rotated level 5 and 12.8 million Crofton rays.

| Model | Legacy | Rotated | Random | QMC | Legacy vs new-method consensus |
|---|---:|---:|---:|---:|---:|
| Havoc | 2.864720 | 2.420378 | 2.422244 | 2.426672 | **+18.226%** |
| M35 cab | 0.547483 | 0.537888 | 0.538267 | 0.537940 | +1.757% |
| Goliath | 0.708337 | 0.709737 | 0.709080 | 0.709610 | -0.161% |
| Generic Twin, full | 29.415300 | 29.725600 | 29.830135 | 29.801455 | -1.244% |

The legacy finest-grid centroids differ from the new-method centroid mean by
about 547 mm on Havoc, 11.1 mm on M35, 2.1 mm on Goliath, and 8.1 mm on the full
Generic Twin.  Havoc is the clearest evidence of axis-aligned aliasing in a
real model.

Cross-method spread among the three new methods summarizes consistency for all
numeric outputs.  Volume and mass have the same relative spread because of the
uniform synthetic density.  Inertia spread uses tensor trace about the model
origin; centroid reports the maximum pairwise distance.

| Model | Volume/mass spread | Area spread | Centroid separation | Inertia-trace spread |
|---|---:|---:|---:|---:|
| Havoc | 0.260% | 0.336% | 12.31 mm | 0.203% |
| M35 cab | 0.070% | 0.113% | 1.73 mm | 0.107% |
| Goliath | 0.093% | 0.0097% | 0.68 mm | 0.204% |
| Generic Twin, full | 0.351% | **6.445%** | 23.75 mm | 0.344% |

The Generic Twin area disagreement is the main unresolved answer regression.
Random and QMC agree with each other to 0.108%, while rotated grid is 6.5%
higher.  This points to orientation/boundary-count behavior in the rotated
area estimator rather than Crofton noise.

The declared 10-second, 1%-interval mode also succeeded on all four complex
models after the half-space correction:

| Model | Volume (10^9 mm^3) | Relative 95% half-width | Rays | Internal time | Difference from deep new-method consensus |
|---|---:|---:|---:|---:|---:|
| Havoc | 2.430949 | 0.600% | 3,119,616 | 1.691 s | +0.324% |
| M35 cab | 0.536708 | 0.352% | 1,360,416 | 0.407 s | -0.246% |
| Goliath | 0.710126 | 0.221% | 300,000 | 1.221 s | +0.092% |
| Generic Twin, full | 29.871938 | 0.463% | 3,746,112 | 1.526 s | +0.289% |

Goliath illustrates why time and rays are separate concerns: only 300,000 rays
were needed statistically, but its geometry makes each ray much more expensive
than in the other models.

## Modeled-air consistency

Expected volume is 2.0 billion mm^3 with air included and 1.0 billion mm^3
without it.  Expected included mass is 1,001,000 g and excluded mass is
1,000,000 g.

| Output | Legacy | Rotated | Random | QMC |
|---|---:|---:|---:|---:|
| Included volume error | **0%** | +0.00339% | +0.39436% | +0.00286% |
| Excluded volume error | **0%** | -0.00289% | +0.02053% | +0.00081% |
| Included mass error | **0%** | -0.00288% | +0.02127% | +0.00081% |
| Included centroid error | 14.43 mm | **0.052 mm** | 1.01 mm | 0.184 mm |
| Excluded centroid error | 14.43 mm | **0.052 mm** | 1.01 mm | 0.184 mm |

All methods apply air inclusion consistently.  The legacy exact totals again
coexist with a cell-center centroid bias.

## Diagnostic outputs

Diagnostics are sampled observations, not proofs that unobserved problems are
absent.  “Largest” values are therefore directional extreme samples and
generally increase as more favorable rays are found.

| Check | Legacy | Rotated | Random | QMC | Assessment |
|---|---|---|---|---|---|
| Overlap | Found; max 8,000 mm | Found; 2,888.6 mm | Found; 9,714.2 mm | Found; 10,185.5 mm | Analytic directional maximum is 11,357.8 mm; QMC is closest |
| Gaps/internal voids | Found | Found | Found | Found | New output distinguishes `internal-void` and both region orders |
| Adjacent air | Legacy stand-alone run crashed without a plot prefix | Found at 0 mm | Found at 0 mm | Found at 0 mm | Existing legacy defect; experimental modes are clean |
| Exposed air | Found; max 9,000 mm | Found; 12,122.8 mm | Found; 12,765.4 mm | Found; 12,406.1 mm | Analytic box diagonal is 14,456 mm; random is closest at this budget |

Crofton has much better angular coverage than three fixed views for overlap and
exposed-air extremes.  Rotating only one triad does not guarantee that it will
sample the direction of a narrow or maximal defect.

## Output and interface coverage

| Capability | Legacy | Rotated grid | Crofton random/QMC | Status |
|---|---|---|---|---|
| Volume | yes | yes | yes | Comparable |
| Mass/weight | yes | yes | yes | Comparable with density data |
| Centroid | yes | yes | yes | Comparable |
| Moments/products of inertia | yes, about centroid | yes, about model origin | yes, about model origin | Convention mismatch must remain explicit |
| Surface area | no | yes | yes | Experimental improvement |
| Overlaps | yes | yes | yes | Crofton negative result remains probabilistic |
| Gaps/internal voids | yes | yes | yes | Experimental labeling is clearer |
| Adjacent air | yes, but tested crash | yes | yes | Legacy defect exposed |
| Exposed air | yes | yes | yes | Better angular coverage experimentally |
| Modeled-air include/exclude | yes | yes | yes | Consistent |
| Per-object/per-region results | text | JSON/text | JSON/text | Experimental output is easier to consume |
| Bounding-box report | yes | yes | yes | Experimental bounds need no sampling rays and reject unbounded results |
| Plot3 diagnostic output | yes | all or representative | all or representative | Experimental modes also support incremental MGED display |
| Progress, replay, uncertainty | no | progress | progress, invalid-ray replay, intervals | Experimental improvement |

## Half-space behavior

Half-spaces must remain in ray tracing when they clip finite geometry; simply
removing them changes Boolean meaning.  The corrected implementation derives
conservative sampling bounds from prepared region trees and ignores infinite
solids only when constructing the Crofton sampling sphere.

- A cube intersected by a half-space and the complementary subtraction each
  report the expected 500,000,000 mm^3.
- The combined split reports the original 1,000,000,000 mm^3 in both grid and
  Crofton tests.
- The complete Generic Twin, whose fuel regions use half-space clipping, now
  runs successfully and meets the adaptive 1% target in 1.526 seconds.
- `cube.g/cloud.r.bak` is another bounded half-containing root and now runs.
- A standalone half-space, or a selected union containing an independently
  unbounded region, is rejected with an explicit unbounded-region diagnostic.
- Unboundedness is now propagated through `SUBMODEL`.  This prevents the prior
  `primitives.g/other` infinite-hit crash while preserving bounded submodels
  whose internal half-space is clipped by finite geometry.

Of the twelve original direct or indirect half-containing corpus roots, two
are bounded and now analyzable; ten are genuinely unbounded selections and are
rejected before sampling.

## Recommendations

1. **Use QMC for the experimental accuracy-oriented default.**  Keep a time
   ceiling as the primary resource control, report progress, and always expose
   `target_met`, `target_not_met`, or `time_limit` with the achieved interval.
2. **Retain the geometry-scale stability rule as an absolute floor, not the
   sole ordinary stopping target.**  It consumed the full minute on 115/138
   cases even though the independent comparison was already subpercent for
   almost all of them.
3. **Keep rotated grid as a deterministic control.**  It is excellent on the
   analytic fixtures and gives users a non-statistical alternative, but it
   should not replace QMC until the Generic Twin area discrepancy is resolved.
4. **Investigate rotated-grid surface-area accumulation next.**  Test multiple
   independent triads, internal-interface counting, clipped boundary-cell
   weights, and convergence under orientation changes.  The Crofton pair's
   0.108% agreement gives a strong reference, though not analytic truth.
5. **Keep ordinary random Crofton for validation and debugging.**  It is an
   important unbiased control for QMC, but should not be the performance
   default at the tested precision levels.
6. **Keep representative plotting as the interactive default.**  The `all`
   policy is valuable for exact legacy-style debugging but grows directly with
   ray count.  Continue using bounded spatial representatives for ordinary
   live viewing, and clearly document inertia reference conversion when
   presenting legacy and experimental tensors side by side.
7. **Expand interval calibration.**  Repeat the 10-second corpus with multiple
   independent outer seeds and analytic fixtures to measure empirical coverage,
   especially for area, centroid, inertia, near-zero results, and
   `--accuracy-scope all`.
8. **Retain half-space/submodel regressions.**  Test finite intersection,
   subtraction, bounded submodels, standalone unbounded regions, and nested
   unbounded submodels.  Do not filter half-spaces out of Boolean evaluation.
9. **Treat diagnostic negatives conservatively.**  Report sampling evidence
   and event-probability bounds, never “no overlap exists,” unless a separate
   exhaustive geometric proof is implemented.
