# libged Commands vs. MGED Availability

_Generated audit of documented libged commands and their availability in MGED._

## Question

Which commands are (a) registered in `libged` via the `X()`/`XID()` macro command
lists **and** (b) have a manual page under `doc/asciidoc/system/mann/`, but are **not**
reachable as a command name in MGED (`mged_cmdtab[]` in `src/mged/setup.c`)?
For each, can it be trivially exposed in MGED, or does it require real work?

## Method

- **libged command names**: parsed every `#define GED_*_COMMANDS(X, XID)` list in
  `src/libged/**` (both `X(token,...)` and `XID(sym,"name",...)` forms). Each token
  auto-generates a public `ged_exec_<token>()` wrapper.
- **Documented commands**: every `doc/asciidoc/system/mann/<name>.adoc`.
- **MGED commands**: every quoted name in `mged_cmdtab[]`.
- **Candidates** = (libged ∩ mann) − mged.

Counts: **352** libged command names, **401** mann pages, **327** documented-and-in-libged.
Before this change MGED exposed **334** command names, leaving **90 candidates**.

Each candidate was then inspected (core function's gedp usage, existing MGED equivalent,
and name collisions against `mged_cmdtab[]`, Tcl built-ins, and the `src/tclscripts`
procs) and classified.

## Result summary

| Class | Count | Action |
|-------|-------|--------|
| Genuinely missing, trivially addable | 53 | **Added** to `mged_cmdtab[]` |
| Documented alias of an existing MGED command (safe) | 18 | **Added** as alias |
| Documented alias needing a custom wrapper / with a conflict | 9 | Not added (see below) |
| Non-trivial (stub, no-op, dispatcher, or name conflict) | 10 | Not added (see below) |

After the change MGED exposes **405** command names; **19 candidates remain** (documented below).

---

## 1. Added to MGED — genuinely missing commands

These had no MGED equivalent under any name. Each was added as a single
`mged_cmdtab[]` line with the wrapper appropriate to the gedp state it touches
(`cmd_ged_plain_wrapper` for database / named-object commands, `cmd_ged_view_wrapper`
for view-mutating commands, `cmd_ged_dm_wrapper` for framebuffer/display-manager commands).

### plain (database / named-object) — `cmd_ged_plain_wrapper`

`combmem`, `dbot_dump`, `dplot`, `edarb`, `find_arb_edge`, `find_bot_edge`, `find_bot_pnt`, `find_pipe_pnt`, `how`, `human`, `importFg4Section`, `isize`, `log`, `metaball_delete_pnt`, `metaball_move_pnt`, `move_arb_edge`, `move_arb_face`, `ocenter`, `orotate`, `otranslate`, `pathsum`, `pipe_append_pnt`, `pipe_delete_pnt`, `pipe_move_pnt`, `pipe_prepend_pnt`, `pmodel2view`, `protate`, `pscale`, `pset`, `ptranslate`, `redraw`, `remove`, `rmap`, `rot_point`, `rotate_arb_face`, `rselect`, `rtwizard`, `sdata_lines`, `sphgroup`

### view-mutating — `cmd_ged_view_wrapper`

`data_lines`, `eye_pos`, `perspective`, `pmat`, `quat`, `rect`, `rmat`, `rot_about`, `ypr`

### framebuffer / display-manager — `cmd_ged_dm_wrapper` (needs an attached DM at runtime)

`ert`, `fb2pix`, `fbclear`, `pix2fb`, `png2fb`

> Note: the framebuffer/DM commands (`fb2pix`, `pix2fb`, `fbclear`, `png2fb`, `ert`) and
> `rtwizard`/`rselect` are exposed but only function when a display manager / framebuffer
> is attached (or, for `rtwizard`, the external `rtwizard` program is installed).

## 2. Added to MGED — documented aliases of existing commands

Same underlying `ged_*_core` function as a command MGED already had under a shorter/
different name. Adding the documented spelling is a one-line alias through the same
wrapper the existing sibling uses. (This matches the intent noted in the `cmd_setup()`
comment in `setup.c` that all plain libged commands should be reachable from MGED.)

| Added name | Wrapper | Already available in MGED as |
|------------|---------|------------------------------|
| `aet` | `cmd_ged_view_wrapper` | `ae` |
| `comb_std` | `cmd_ged_plain_wrapper` | `c` |
| `copy` | `cmd_ged_plain_wrapper` | `cp` |
| `eye` | `cmd_ged_view_wrapper` | `eye_pt` |
| `find` | `cmd_ged_info_wrapper` | `dbfind` |
| `group` | `cmd_ged_plain_wrapper` | `g` |
| `instance` | `cmd_ged_plain_wrapper` | `i` |
| `move` | `cmd_ged_plain_wrapper` | `mv` |
| `move_all` | `cmd_ged_plain_wrapper` | `mvall` |
| `nmg_cmface` | `cmd_ged_plain_wrapper` | `nmg cmface` |
| `nmg_kill_f` | `cmd_ged_plain_wrapper` | `nmg kill F` |
| `nmg_kill_v` | `cmd_ged_plain_wrapper` | `nmg kill V` |
| `nmg_make_v` | `cmd_ged_plain_wrapper` | `nmg make V` |
| `nmg_mm` | `cmd_ged_plain_wrapper` | `nmg mm` |
| `nmg_move_v` | `cmd_ged_plain_wrapper` | `nmg move V` |
| `orient` | `cmd_ged_view_wrapper` | `orientation` |
| `print` | `cmd_ged_plain_wrapper` | `view print` |
| `region` | `cmd_ged_plain_wrapper` | `r` |

---

## 3. NOT added — documented aliases requiring a custom wrapper or with a conflict

The functionality is already reachable in MGED under the listed name; exposing the
documented spelling is possible but is **not** a standard one-line add.

### `concat`  (already available as `dbconcat`)

**Name conflict:** Tcl's built-in `concat` command concatenates lists and is used by
MGED's Tcl scripts. Registering `ged_exec_concat` as a top-level command replaces that
built-in with the database concatenation command, breaking normal Tcl evaluation.

**To expose the documented name:** it cannot safely be exposed unqualified in MGED's
Tcl interpreter. Continue using `dbconcat`, or provide the GED spelling in a namespace.

### `glob`  (already available as `db_glob`)

**Name conflict:** Tcl's built-in `glob` command performs filesystem pathname matching.
The libged command instead matches database objects. Registering `ged_exec_glob` as a
top-level command replaces Tcl's filesystem command and breaks scripts that use it.

**To expose the documented name:** it cannot safely be exposed unqualified in MGED's
Tcl interpreter. Continue using `db_glob`, or provide the GED spelling in a namespace.

### `list`  (already available as `l`)

**Name conflict:** Tcl's built-in `list` command constructs lists and is fundamental to
MGED's Tcl command and callback handling. Registering `ged_exec_list` as a top-level
command replaces the Tcl built-in, causing ordinary list elements to be interpreted as
database object names and breaking MGED startup and regression scripts.

**To expose the documented name:** it cannot safely be exposed unqualified in MGED's
Tcl interpreter. Continue using `l`, or provide the GED spelling in a namespace.

### `blast`  (already available as `B`)

`ged_blast_core` is reached in MGED through the custom `cmd_blast` (`cmd.cpp`), which runs the blast GED command and then resizes/updates all display-manager views. A bare standard wrapper would skip that multi-DM view resize.

**To expose the documented name:** add `{MGED_CMD_MAGIC, "blast", cmd_blast, GED_FUNC_PTR_NULL, NULL}` reusing the custom `cmd_blast`.

### `zap`  (already available as `Z`)

`ged_zap_core` (erase all) is reached through the custom `cmd_zap` (`chgview.c`), which also rejects any in-progress MGED solid/object edit (BE_REJECT) and runs `solid_list_callback`. A bare wrapper would bypass that editing-state cleanup.

**To expose the documented name:** add `{MGED_CMD_MAGIC, "zap", cmd_zap, GED_FUNC_PTR_NULL, NULL}` reusing the custom `cmd_zap`.

### `scale`  (already available as `sca`)

`ged_scale_core` (view scale) is reached through the custom `cmd_sca` (`chgview.c`), which in solid/object edit state applies *edit* scaling and otherwise performs the view scale. A bare plain wrapper named `scale` would give only the view-scale half and would not match `sca`.

**To expose the documented name:** add `{MGED_CMD_MAGIC, "scale", cmd_sca, GED_FUNC_PTR_NULL, NULL}` reusing `cmd_sca` (decide whether `scale` should carry the edit-state behavior).

### `slew`  (already available as `sv`)

`ged_slew_core` (shift view center) is reached through the custom `f_slewview` (`chgview.c`), which additionally does MGED `ModelDelta` / `set_absolute_tran` bookkeeping after the GED call.

**To expose the documented name:** add `{MGED_CMD_MAGIC, "slew", f_slewview, GED_FUNC_PTR_NULL, NULL}` reusing `f_slewview`.

### `screen_grab`  (already available as `screengrab`)

In libged `screen_grab` and `screengrab` are two tokens for the same `ged_screen_grab_core`. MGED exposes `screengrab` through the custom `cmd_screengrab` (`cmd.cpp`), which forces a scene render and sets `ged_gvp->dmp` before reading pixels. `screen_grab.adoc` explicitly calls it an alias for `screengrab`.

**To expose the documented name:** add `{MGED_CMD_MAGIC, "screen_grab", cmd_screengrab, ged_exec_screen_grab, NULL}` reusing `cmd_screengrab`.

### `which`  (already available as `whichid`)

`which`, `whichair`, and `whichid` all map to `ged_which_core`; `which` is functionally identical to `whichid` (which MGED already exposes). It was **not** added because `which` collides with a `proc which` shipped in `src/tclscripts/tkcon.tcl` (the console's command locator); creating a C command `which` would shadow it.

**To expose the documented name:** if desired, resolve the tkcon `which` shadowing first, then add `{MGED_CMD_MAGIC, "which", cmd_ged_plain_wrapper, ged_exec_which, NULL}`.

---

## 4. NOT added — non-trivial commands

These cannot be exposed by a table line alone.

### `apropos`

**Name conflict:** apropos (Tcl proc in src/tclscripts/mged/help.tcl:409)

libged 'apropos' maps to ged_help_core (help.cpp:288). However mged already defines its own richer Tcl 'proc apropos' (help.tcl:409) that does a -k/-K keyword search across mged_help_data and manual page text. This is a different implementation with different semantics, so it is not an alias and adding a table entry would collide with (and be shadowed by / conflict with) the existing Tcl command.

**Required:** Would require reconciling the semantic difference between libged's ged_help_core apropos and mged's Tcl apropos keyword-search proc (deciding which behavior wins, or renaming), not just a table line.

### `clear`

**Name conflict:** clear (Tcl proc in src/tclscripts/mged/clear.tcl:29)

libged 'clear' (ged_clear_core, clear.cpp) is an intentional no-op that only exists to fire application callback hooks. mged already has a 'clear' Tcl proc (clear.tcl:29) that clears the MGED command window/console. These do entirely different things, so adding a ged_exec_clear table line would collide with the existing, differently-behaving mged clear command.

**Required:** Would require resolving the name clash with mged's console-clearing Tcl proc; the libged no-op provides no user-visible functionality mged wants under this name, so no simple table add is appropriate.

### `editit`

**Name conflict:** requires an interactive external text editor and, in mged, prior ged_set_editor()/ged_clear_editor() setup keyed on classic vs GUI mode

ged_editit_core spawns an interactive external text editor as a blocking subprocess via _ged_editit (ged_util.cpp) and is intended as an internal helper for red/color/edmater editing, not a standalone top-level user command. In mged it is only invoked from tedit.c, which first calls ged_set_editor(s->gedp, s->classic_mged) to choose an appropriate editor/terminal for the current (classic vs GUI) mode and ged_clear_editor() afterward. A bare cmd_ged_plain_wrapper table line would run it without that mode-aware setup, so the wrong editor/terminal handling would occur in GUI mode. Exposing it correctly needs a custom mged wrapper.

**Required:** Add a custom cmd_* wrapper in cmd.cpp (modeled on tedit.c's editit()) that calls ged_set_editor(s->gedp, s->classic_mged) before ged_exec and ged_clear_editor() after, so the correct editor/terminal is chosen for classic vs GUI mode; then register that wrapper (not cmd_ged_plain_wrapper) in mged_cmdtab.

### `grid`

**Name conflict:** Tk built-in 'grid' geometry manager command

ged_grid_core (src/libged/grid/grid.c:97) gets/sets the view grid parameters (gedp->ged_gvp->gv_s->gv_grid: draw, snap, res_h/v, anchor, color, and grid vsnap which moves gv_center). Functionally it would need cmd_ged_view_wrapper because it mutates view state. It is not currently in mged_cmdtab. The blocking issue is a hard name collision: 'grid' is a core Tk command (the geometry manager) used pervasively by mged's Tk GUI. Registering a top-level mged 'grid' command would shadow Tk's grid and break GUI layout. mged historically manages grid state through its own dm/color_scheme machinery instead.

**Required:** Cannot use the 'grid' name without breaking Tk's geometry manager. Would require choosing a non-colliding command name (or namespacing), wiring it through cmd_ged_view_wrapper, and reconciling it with mged's existing dm-level grid handling (color_scheme cs_grid, update_grids).

### `help`

**Name conflict:** mged Tcl 'help'/'apropos'/'?' procs (help_comm in src/tclscripts/helpcomm.tcl, src/tclscripts/mged/help.tcl) providing the real per-command help browser

ged_help_core (src/libged/help/help.cpp:250) is an experimental/incomplete implementation: it recursively lists and tokenizes doc files (emitting 'Processing ...'/'Found N files' logs) and returns nothing useful to the user. mged already provides a fully functional 'help', 'apropos', and '?' via Tcl procs (help_comm/mged_help_data driven by helplib.tcl) that print per-command usage and descriptions. Registering ged_exec_help would collide with and override the working Tcl help system with an inferior/broken one.

**Required:** The libged help core would first need to be made a genuine per-command help provider, and the collision with mged's established Tcl help/apropos/? system resolved. Not a single-line table add.

### `illum`

**Name conflict:** `proc illum` in `src/tclscripts/mged/eobjmenu.tcl`

`ged_illum_core` illuminates a named path in the display list. It was reclassified as non-trivial during validation: `src/tclscripts/mged/eobjmenu.tcl` defines a global `proc illum` (created when the object-edit menu `eobjmenu` runs) that means something different (drive the solid-illuminate / solid-edit menu state). A C command `illum` would be redefined/shadowed by that proc, so behavior would be inconsistent.

**Required:** reconcile the two `illum` meanings (rename one, or namespace the eobjmenu proc) before exposing the libged command.

### `label`

ged_label_core is an unimplemented stub: it validates args and then returns the literal message 'Not yet implemented!' with BRLCAD_OK. Exposing it in mged would surface a non-functional command. There is no real functionality to wire up.

**Required:** Implement ged_label_core in libged (currently a stub that arranges nothing). Only after the core actually draws object labels would a table line make sense; today there is nothing to expose.

### `man`

**Name conflict:** `proc man` in `src/tclscripts/mged/man.tcl`

`ged_man_core` opens a manual page. It was reclassified as non-trivial during validation: MGED already ships a global `proc man` in `src/tclscripts/mged/man.tcl` that is the intended MGED man interface (it drives the `ManBrowser` GUI in graphical mode and console output otherwise). A C command `man` created at startup would shadow that richer proc (Tcl auto-load only fires for unknown commands), regressing MGED's `man`.

**Required:** decide whether MGED's `man.tcl` ManBrowser proc or the libged `man` command should win; if the libged one, remove/rename the proc and wire any GUI behavior through a callback.

### `tables`

Although 'tables' is a token mapping to ged_tables_core (src/libged/tables/tables.c:658), the core keys off argv[0] and only handles "solids", "regions", and "idents"; when invoked as "tables" it falls through to the 'should never reach here' branch and returns a BRLCAD_ERROR input error (tables.c:509-523). So the 'tables' command itself is non-functional - it produces no table. The real functionality (idents/regions/solids) is already exposed in mged (setup.c:208,319,362). Exposing 'tables' would add a command that always errors, so it is not a useful trivial add.

**Required:** Would require either changing ged_tables_core to give the 'tables' name a real behavior (e.g. defaulting to a table type), or deciding it should remain unexposed. As-is a table-line add yields a command that always returns an input error.

### `view_func`

ged_view_func_core (src/libged/view/view.c:795) is a legacy multiplexer/dispatcher for the old 'view <subcommand>' interface (ae/aet/center/eye/data_lines/quat/size/ypr...). The mann page view_func.adoc describes it as the 'legacy view control multiplexer'. It is a sub-command router, not a real top-level command, and its individual sub-functions are already exposed in mged as first-class commands (ae, center, eye, quat, size, etc.). It is not meaningful to expose as a standalone mged builtin.

**Required:** Not appropriate as a top-level mged command; it is an internal dispatcher. Exposing view functionality is already done via the individual view commands, so no wrapper is applicable.

---

## Verification status

- All 71 added entries reference a `ged_exec_<name>` symbol confirmed present in the generated
  `ged/ged_cmds.h`, and a wrapper (`cmd_ged_plain_wrapper` / `_view_` / `_dm_` / `_info_`)
  declared in `src/mged/cmd.h` (already included by `setup.c`).
- The `mged_cmdtab[]` array structure (sentinel, special entries) is intact; the resulting
  command-table delta from the pre-audit baseline is 71 insertions.
- The Ninja `mged` target builds successfully, and the `regress-bots`, `regress-mged`, and
  `regress-shaders` targets pass with the Tcl-conflicting aliases omitted.
