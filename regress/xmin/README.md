# Xmin GUI test support

These tests exercise real BRL-CAD GUI applications on an isolated Xmin server.
They are registered only by builds configured with:

```
-DBRLCAD_X11_PROVIDER=XMIN -DXmin_ROOT=/path/to/xmin-sdk
```

Tk, Qt, X11, and GLX must all use that SDK.  CMake rejects a mixed provider
build because Xlib's public types hide implementation-private state which GLX
and toolkit code must interpret consistently.

## Shared architecture

The support has three layers:

1. `brlcad_add_xmin_test` owns the private-server lifecycle, fixed screen,
   environment, timeout, and CTest registration.
2. `xmin_gui_smoke.sh` owns the application lifecycle: launch, resolve the
   mapped top-level to a stable window ID, wait for rendering to settle,
   capture, run an optional action, compare captures, and close cleanly.
3. Application fixtures own their semantic oracle.  Tk applications source
   `xmin_gui_test.tcl` for widget and menu discovery.  XTEST action scripts
   drive paths that must traverse the actual X input protocol.

Resolving a window name once is important for Qt, which may create mapped and
unmapped helper windows with the same title.  Repeating a title lookup can
silently redirect later input or geometry queries to the helper.

The shared Tcl library deliberately stops at mechanisms common to stable Tk
applications: assertions, recursive widget inventory, complete menu-tree
inventory, menu lookup and invocation, and an asynchronous Tcl error ledger.
A semantic fixture installs the ledger before entering the event loop, drains
pending events during teardown, and fails if Tk sent an otherwise-unobserved
error to ``bgerror``.  The library does not encode application menu paths,
object models, or screen layouts.  Those belong in each fixture so a change in
one application cannot weaken another application's contract.

## Test tiers

`regress-archer-xmin-smoke` launches the installed-style Archer executable,
types `draw all.g` into the real command entry with XTEST, and requires a
substantial pixel change.  `regress-archer-xmin-gui` then checks the exact
99-entry live menu manifest, records every widget class, verifies relative
database paths, the GED display list and view state, and background changes,
and opens and closes Center, Preferences, Plug-ins, About, and the HTML Help
browser.  It also edits a combination color through the live Combination
Editor and verifies that a deliberately right-nested Boolean tree is preserved
exactly, checks hierarchy discovery for an object name containing spaces,
and exercises all 26 primitive editors.  Each editor initializes from a real
database object, writes its unchanged state, and reloads it; structured numeric
comparison catches canonical-form or unit-conversion drift without relying on
unstable string formatting.  Focused sketch cases cover bounded-arc tangency at
both endpoints and an unrelated vertex, reversed orientation, full circles,
and Bezier drawing and serialization.  The fixture also constructs the
Combination Editor and performs functional checks of Attribute Groups, LOD,
and BoT utilities.  It runs the Human, Tire, and Tank wizards through actual
geometry creation and verifies their database or XML results.  Finally, it
starts and aborts an actual toolbar raytrace while validating its asynchronous
callback and button state.

`regress-rtwizard-xmin-smoke` changes image type through real pointer input and
requires a visible page rebuild.  `regress-rtwizard-xmin-gui` checks the exact
17-entry base menu, the database-bearing window title and About dialog,
constructs all six image types, and verifies every type's exact dynamic Steps
menu.  It exercises the framebuffer dimensions, destination, output file, cut
animation, frame rate and range, direction, and ambient-occlusion controls;
verifies their widget-to-state bindings; writes the window layout to an
explicitly isolated configuration file; and completes real 64 by 64 renders
for all six picture types, checking each exact output size.
The command-line suite separately verifies multi-object edge occlusion.

MGED's fixtures are registered from `regress/xmin/mged`.  The deep fixture runs
tkswrast, X, and ogl in one private-server session and combines XTEST
interaction with database, view-state, framebuffer, and image-comparison
oracles.  A focused controls fixture exercises every ViewRing transition,
including deletion; applies and resets grid and ADC state; applies and persists
font changes; dismisses each panel through its live widgets; and opens and
dismisses About MGED.  A workflow fixture drives every generic primitive plus
DSP and BINUNIF creation, New/Open and core import/export operations, script
loading, Geometry Browser actions, and a collaborating second GUI.  It checks
the resulting database, files, display, synchronized view, and clean teardown.
The detailed coverage ledger is
`regress/xmin/mged/MGED_GUI_TEST_COVERAGE.md`.  MGED's retiring Predictor feature
is absent from both the menu manifest and behavioral coverage.

A focused search-exec fixture draws and redraws more than 1,000 m35.g paths,
verifies display-list hierarchy semantics, rejects GUI event-loop command
reentry, exercises Control-C, and confirms that the next search succeeds.  A
focused ShotVis fixture also edits, redraws, persists, reloads, and re-edits
an actual visualization through its live Tk controls.  Predictor's removal is
not part of the test contract.

`regress-qged-xmin-smoke` is intentionally experimental.  It proves only that
the Qt application starts, its console accepts `draw all.g`, and the OpenGL
view changes.  qged is not yet stable enough for a menu or widget compatibility
manifest.

The libtclcad Itcl 3 tests complement application fixtures by loading the GUI
package graph in both `btclsh` and `bwish`, checking representative Sdialogs,
Swidgets, DataUtils, and cadwidgets behavior, and repeating package and object
initialization in independent, destroyed, and replacement Tcl interpreters.
This separates package/load-order and interpreter-lifetime failures from the
larger application workflows.

## Adding a fixture

Prefer semantic state over pixels whenever the application can report it.
Pixel comparison is appropriate for proving that an X input action reached a
renderer or rebuilt a page, but it should not replace a database, command, or
widget-state assertion.

For a conventional executable:

```
brlcad_add_xmin_test(
  NAME regress-example-xmin-smoke
  TIMEOUT 180
  ENVIRONMENT
    "XMIN_TEST_SHELL=${CMAKE_CURRENT_SOURCE_DIR}/xmin_test.sh"
    "XMIN_SMOKE_ACTION=${CMAKE_CURRENT_SOURCE_DIR}/example_action.sh"
  COMMAND "${SH_EXEC}" "${CMAKE_CURRENT_SOURCE_DIR}/xmin_gui_smoke.sh"
    "Expected title" $<TARGET_FILE:example>
)
```

The action receives the resolved main-window ID and artifact directory.  It
should derive input locations from live geometry or published widget data,
not from absolute root coordinates.  Keep settle intervals named and
overridable, and choose the smallest meaningful pixel-difference threshold.

Tcl fixtures should publish locations atomically only after their widgets are
mapped and should retain artifacts on failure.  Do not report a dialog as
tested merely because it was constructed: invoke it through the same command
a user reaches, assert its characteristic widgets or application state, and
dismiss it through a normal control.

## Scope

Xmin validates X11 protocol, toolkit dispatch, input, composition, and GLX
paths.  It does not replace platform-specific runs on Aqua or Windows, GPU
driver coverage, accessibility testing, or visual review.  The fixtures are
also not a license to hide timing races with long sleeps: wait for observable
state where possible and use short settle delays only for rendering that has
no stronger completion signal.
