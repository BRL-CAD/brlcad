# MGED graphical feature and test coverage

This catalog describes GUI entry points exposed by MGED's classic Tk interface
and in-scene Faceplate.  The machine-readable companion
`mged_gui_menu_manifest.txt` records every main-menubar entry, including its
menu type and enabled state.  The Xmin regression compares the live menu tree
with that manifest so additions, removals, and accidental disabling cannot go
unnoticed.

## Main menu

| Menu | Graphically exposed features | Headless exercise strategy |
| --- | --- | --- |
| File | New/open database, ASCII and binary import, ASCII and object export, load script, raytrace, render-view scripts/plot/PostScript, units and command editing preferences, color/font settings, command clear, exit | Inventory and menu rendering are covered.  File operations need temporary input/output fixtures; renderer launchers need their child-process results checked. |
| Edit | Primitive and matrix selection, primitive editor, combination editor, attribute editor, geometry browser | Primitive Editor is edited and applied through XTEST.  Matrix selection is covered by an exact geometry-result test.  The remaining editors can be isolated with generated database fixtures. |
| Create | ARB variants, cones/cylinders, ellipsoids, and all other supported primitive creation dialogs | Inventory is covered.  Comprehensive action tests require one valid form fixture per primitive type and database-result comparison. |
| View | Orthographic and oblique views, zoom, default/multipane defaults, zero | Menu rendering is covered.  The Faceplate 35,25 equivalent is action/result tested; the remaining deterministic view commands can use the same before/after view-state oracle. |
| ViewRing | Add/select/delete/next/previous/last saved views | Inventory is covered.  A complete test can build a ring, exercise every transition, and compare center, size, and orientation. |
| Settings | Mouse behavior, transform space, constraint coordinates, rotation origin, pane/application scope, query-ray effects, grid controls and spacing, framebuffer behavior, axes position | Inventory is covered.  Settings divide into variable-state tests, pane-layout tests, and framebuffer tests requiring a known image fixture. |
| Modes | Grid/snap/framebuffer/listener, rubber band, ADC, Faceplate, axes, multipane, edit/status/command/DM panels, collaboration, rate knobs, display lists | Inventory is covered.  These are suitable for action/result testing via the controlling MGED variable plus visible widget, DM, or view state.  Listener/collaboration also need socket lifecycle checks. |
| Misc | Faceplate/menu visibility, predictor, perspective, faceplate GUI, depth cueing, Z buffer, lighting | Depth cueing, Z buffer, and lighting are physically toggled and restored on every DM that reports the capability. |
| Tools | ADC, grid, query-ray and raytrace panels; BoT editor; pattern, color, geometry browser/search, LOD and overlap tools; database upgrade; command/graphics windows | Inventory is covered.  Each panel can be opened and dismissed headlessly; meaningful action coverage needs BoT, overlap, search, and legacy-database fixtures. |
| Help | Dedication/about, command manuals, shift-grip guide, apropos, manual search, manual | Inventory is covered.  Static dialogs are straightforward; manual lookup needs a known installed manual and input-dialog checks. |

The manifest is deliberately captured from a live Tk hierarchy instead of
duplicating `openw.tcl` parsing logic.  Cascades, radiobuttons, checkbuttons,
commands, disabled states, and configuration-dependent entries are therefore
all represented exactly as a user sees them.

## Dialog families

The graphical commands above lead to these independently testable dialog
families:

- database and filesystem dialogs: new/open, concatenate, import/export,
  script loading, and render output;
- geometry construction and editing: primitive creation, Primitive Editor,
  Combination Editor (including shaders), Attribute Editor, BoT editor, and
  the Sketch Editor;
- view and display controls: ADC, grid, query ray, raytrace, colors, fonts,
  LOD, framebuffer, and multipane controls;
- browsing and analysis: geometry browser/search, overlap inspection, and
  database upgrade;
- informational and text input: about/dedication, apropos, manual search,
  command manuals, confirmations, warnings, and errors.

Xmin can exercise the full Tk event path for all of these.  Tests that touch
files, invoke subprocesses, listen on sockets, or overwrite databases must use
isolated fixtures and verify the external result in addition to observing the
dialog.  Merely constructing a dialog is useful smoke coverage but is not a
complete feature test.

## Faceplate and in-scene interaction

The general Faceplate exposes edit accept/reject, standard views, view
save/restore, angle/distance cursor, view-size reset, scroll sliders,
rate/absolute mode, zoom, primitive illumination, and matrix illumination.
Primitive edit adds rotate, translate, scale, and a type-specific contextual
menu.  Matrix edit adds uniform/axis scale, X/Y/XY moves, and rotation.

Type-specific menus currently exist for ARB, ARS, BoT, B-spline, CLINE, DSP,
EBM, EHY, ELL, EPA, ETO, extrude, HRT, HYP, metaball, NMG, particle, pipe, RHC,
RPC, sketch, superellipsoid, TGC, torus, and VOL primitives.  Generic
primitives still expose the common rotate/translate/scale operations.

The GUI regression physically exercises Faceplate expansion and 35,25 view,
pipe point-selection mode, viewport point selection, next-point navigation,
the exposed Split Segment mode with a viewport split point, and a matrix XY
move.  The stored five-point pipe and matrix are checked after MGED exits, so
the test cannot pass on a transient display-only change.  The pipe library
test covers every edit operation, including navigation boundaries, point
add/delete/insert, per-point and whole-pipe dimensions, bend radii, split, and
projected edits.

Sketch has two graphical surfaces.  Normal MGED solid selection diverts to the
custom Tk Sketch Editor, which exposes line/circle/arc/Bezier construction,
segment and vertex selection/move/delete, arc complement/radius/tangency,
zoom/reset/save/dismiss, and coordinate entry.  The GUI regression creates a
line with real canvas clicks, checks the vertex-list result, tests zoom and
reset, and dismisses the editor.  The lower edit regression covers the newer
20-command contextual API, including NURB knot/weight editing, split, plane,
reverse, arc orientation/radius, and tangency.  The contextual sketch menu is
not normally reachable through MGED because of the Sketch Editor diversion;
the two surfaces should not be treated as interchangeable coverage.

## Raytrace and framebuffer integration

The regression opens the Raytrace Control Panel through the Tools menu on
`tkswrast`, `X`, and `ogl`.  For each DM it sets a 128x128 size through the Tk
entry, prepares a standalone `rt -M` render from the same saved view, launches
the embedded render through the panel, waits for the child process to become
idle, and exports the embedded framebuffer with `fb2pix`.  The standalone,
embedded, and panel-to-file images must have identical dimensions, no channel
may differ by more than one quantization level, and at least 90 percent of
channels must be byte-identical.  Each embedded and panel-to-file render must
also complete within five seconds, with its actual elapsed time printed in the
CTest log.  The panel file destination and restoration to the embedded pane
are themselves driven through keyboard input.

Visible composition is checked separately from framebuffer memory.  A crop of
the actual composed DM area must change in overlay mode; overlay, interlay, and
underlay are selected through the panel menu and recorded; disabling and
reenabling the framebuffer must visibly change the pane.  Finally the panel is
dismissed, a main-window menu is opened, and an MGED view query is executed to
prove that Tk, MGED, and the framebuffer server remain responsive.

These checks found two distinct X-path defects.  Xmin did not send the
`NoExpose` completion required after `XCopyArea`, leaving Tk inside its text
scroll operation and starving MGED's framebuffer socket.  The X DM's
`if_X24` path also uploaded incremental raytrace tiles only to its off-screen
pixmap; the visible window was stale until a later full redraw.  Xmin now
delivers the completion event, and `if_X24` presents changed pixmap rectangles
to the Tk window as they arrive.  The visual oracle runs before `fb2pix`, whose
refresh would otherwise hide the latter failure.

## X font portability

The X and OpenGL DMs still use X core-font lookup and `XDrawString`; their
bundled-font behavior is therefore different from the fontstash/Struetype
paths used by the software rasterizer and Qt renderer.  Xmin supplies an
embedded fixed font and now recognizes MGED's legacy short names and XLFD
aliases, which makes headless tests independent of host font packages.

Using Struetype as a fallback on a real X server is feasible, but it is not a
drop-in replacement for `XLoadQueryFont`: the X DMs would need glyph
rasterization, caching, color/clip handling, and upload/compositing of glyph
images.  The maintainable follow-on is a shared libdm portable-text backend
used by all non-native-font renderers, rather than a second X-only font stack.
The bundled `src/libbu/struetype.h` already has the newer buffer-size-aware
interfaces; updating it and adding malformed-font tests should be handled as a
separate, focused dependency change.

## Test tiers and completeness

The automated matrix is intentionally tiered:

1. `tkswrast` performs the canonical full menu-manifest comparison and all GUI
   actions.
2. `X` and `ogl` repeat the user-visible GUI actions and geometry or state
   assertions, catching display-manager-specific input/rendering failures.
3. Renderer-specific depth cue, Z buffer, and lighting controls run only when
   the active DM reports support.
4. `rt_edit_test_pipe` and `rt_edit_test_sketch` exhaust complex edit APIs that
   are impractical or currently impossible to reach through one GUI surface.

Every semantically checked GUI state is captured into one UTC-datestamped
`<datestamp>_MGED_GUI_test_run.apng`.  Frames from all tested DMs are ordered in
the same file at one frame per second.  Temporary PPMs are removed after the
APNG is reopened and its frame count is verified; on failure they are retained
with the state log for diagnosis.

A comprehensive suite is feasible, but requires more than clicking every menu
entry once.  The manifest provides exhaustive discovery coverage.  Full
behavioral completeness then needs a fixture and result oracle for each menu
leaf, each primitive-specific Faceplate operation, conditional display modes,
and external integrations.  The current suite establishes that framework and
deeply covers the high-risk event paths (Tk entry/button actions, Faceplate,
viewport mouse editing, pipe, sketch, matrix editing, and all Xmin-backed DMs)
while making the remaining families explicit rather than silently claiming
coverage.

## Confidence strategy

Exhaustively testing every ordering and combination is infeasible, so coverage
should be systematic about which combinations it selects:

1. Treat each editor as a state machine.  Cover every state and transition at
   least once, then add invalid transitions and endpoint boundaries.  For
   example, pipe navigation covers no selection, first, interior, last, and
   attempts to move beyond both ends.
2. Partition inputs by behavior rather than enumerating values.  Test one
   ordinary value plus zero, minimum/maximum, invalid, projected, and
   numerically awkward representatives where applicable.
3. Use pairwise or small covering arrays for mostly independent settings such
   as DM, projection, transform coordinates, snapping, and edit type.  Reserve
   full cross-products for interactions known to be coupled.
4. Prefer semantic oracles over pixels: compare database objects, matrices,
   view state, files, process status, or protocol results.  APNG playback is a
   review and diagnosis aid, not the sole correctness oracle.  For rendering,
   compare an exported framebuffer with a standalone reference image and use
   screen crops only for composition behavior that an image file cannot show.
5. Add deterministic property tests below the GUI.  Generated valid edits
   should preserve primitive invariants; accept/reject and inverse operations
   should obey their algebraic properties; invalid projected edits should
   either fail atomically or produce valid geometry.  Record the random seed
   for exact replay.
6. Use differential runs where practical: replay the same trace on tkswrast,
   X, and ogl, and periodically compare Xmin behavior with a reference X
   server.  Differences must be classified rather than silently tolerated.
7. Measure native code coverage for the focused edit tests and GUI run, then
   target uncovered branches with meaningful cases.  Mutation testing on the
   edit validators and state transitions is a stronger check that assertions
   can actually detect bad behavior.
8. Track each feature in a coverage ledger with separate inventory, open/close,
   action, semantic-result, negative-path, and backend columns.  A menu entry
   being inventoried or a dialog merely opening must not be reported as full
   behavioral coverage.

This approach gives higher confidence with a bounded suite: exhaustive
inventory, transition and boundary coverage for complex editors, pairwise
configuration coverage, and deeper generated tests for the geometry kernels.
