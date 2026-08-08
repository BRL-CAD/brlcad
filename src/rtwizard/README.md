# RtWizard architecture

RtWizard is a C++17 application with one rendering implementation shared by
its command-line and optional Qt interfaces.  It has no Tcl/Tk runtime
dependency.

- `main.c` owns option parsing, command-line compatibility, and expansion of
  declarative render specifications.
- `animation.cpp` validates versioned JSON, converts specifications to normal
  options, evaluates typed animation tracks, and safely updates camera
  keyframes.
- `render.cpp` resolves views and model bounds with librt/libged, launches
  `rt` and `rtedge` through libbu, composites layers with libicv, and writes
  stills, numbered frames, APNG, or MJPEG output.
- `gui.cpp` is a libqtcad/Qt workspace.  It edits the same version 1 render
  specification consumed by headless mode and starts a `--no-gui` child job,
  keeping render failures and cancellation isolated from the interface.

Qt is optional at build time.  A build without Qt still produces the complete
headless renderer; an incomplete invocation reports that the graphical
interface is unavailable.  The retired Tcl source remains under
`src/tclscripts/rtwizard` for migration reference but is neither indexed nor
installed.

The GUI never evaluates `.rtwizardrc`.  On first launch it recognizes only the
old numeric window-size and pane-position assignments, then stores settings in
`QSettings`.
