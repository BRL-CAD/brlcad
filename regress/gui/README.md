# GUI regression support

The shared support has three layers:

1. `brlcad_add_gui_test` registers a test and delegates session setup to the
   selected backend.
2. `GUI_TEST_CTL` is the window-control interface supplied by the backend.
3. `gui_test.sh`, `gui_smoke.sh`, `gui_tcl_test.sh`, and `gui_test.tcl` provide
   shell and Tcl helpers for application lifecycle, artifacts, window input,
   screen comparisons, widgets, menus, and asynchronous Tcl errors.

Application fixtures own their semantic oracles.  `sketch_test.tcl` contains
shared sketch assertions used by Archer and MGED.  The core helpers do not
encode application menus, object models, or screen layouts.

## Controller contract

The controller takes one command followed by arguments:

| Command | Result |
| --- | --- |
| `wait-window --timeout milliseconds title` | Return an opaque window token. |
| `geometry token` | Print `x y width height`. |
| `activate token`, `close token` | Control the identified window. |
| `click [--delay milliseconds] token x y button`, `mouse-move token x y`, `button button down|up` | Send pointer input. |
| `key name`, `type text` | Send keyboard input. |
| `wait-stable --quiet milliseconds --timeout milliseconds token` | Wait for visual quiescence. |
| `capture-screen path` | Write a binary PPM of the composed screen. |
| `screen-geometry` | Print `width height`. |

`root` denotes screen coordinates.  Tk fixtures publish `winfo id` values as
window tokens and widget-local points as coordinates.  The backend resolves
the tokens; fixtures do not interpret their representation.  Capture and
screen geometry must describe the same screen so root-coordinate cropping is
consistent.

The only implemented backend is Xmin.  See [its fixtures](../xmin/README.md)
for setup and test coverage.  A native backend will also need a CMake session
provider and working display/input permissions on its runner.
