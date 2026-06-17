Phase 1 — Style and surface conventions (mechanical, low-risk)

Current tree notes:
- Pointer-to-member connect syntax is already in use across libqtcad; Q_DISABLE_COPY_MOVE is already present on the public Q_OBJECT widgets/models touched so far.
- The naming cleanup has started: QgConsoleListener, QgPoly*Filter, and QgAsc/QgRhino/QgStepImportDialog are the exported names now, with compatibility aliases still present.
- QgTypes.h already centralizes the QgView_*/quadrant/edit-mode/comb-type compatibility constants behind enum class definitions plus Q_DECLARE_FLAGS for view-update flags.
- Remaining work from this phase is mostly a residual style sweep (for example, lingering NULL usage in tests/docs and any remaining missing override/delete cleanups).

- Adopt a uniform style: replace NULL with nullptr in libqtcad headers and .cpp files; switch all signal/slot connections to the pointer-to-member-function form; apply override everywhere appropriate; add Q_DISABLE_COPY_MOVE (or = delete'd copy/move) to all Q_OBJECT classes.
- Normalize naming: rename the un-prefixed filter classes (QPolyCreateFilter → QgPolyCreateFilter, etc.), QConsoleListener → QgConsoleListener (already the filename), ASCImportDialog/RhinoImportDialog/STEPImportDialog → QgAsc/Rhino/StepImportDialog, give them QTCAD_EXPORT, and provide back-compat typedefs for one release if needed.
- Run clang-format/astyle over src/libqtcad and include/qtcad with the BRL-CAD style.
- Replace #define-style enums (QgView_*, UPPER_RIGHT_QUADRANT, Qg*EditMode, QG_VIEW_*, G_STANDARD_OBJ/…) with enum class (or Q_FLAGS for the bitwise view flags) declared inside the relevant class or a small QgTypes.h.

Phase 2 — Encapsulation pass

Current tree notes:
- This phase is started but not finished: QgGL/QgSW already expose defaultMouseMode as Q_PROPERTY and already have basic view()/displayManager()/frameBuffer() getters.
- QgView now keeps its active event-filter pointer private and manages it through add/remove helpers rather than exposing raw external writes.
- The standalone dm Qt window wrappers now expose their QgGL/QgSW canvases through accessors instead of public member pointers.
- QgToolPaletteElement now exposes its button/control/scroll state through accessors instead of public data members.
- QgViewCtrl now keeps its ged/action state private instead of exposing raw toolbar members.
- QgQuadView now forward-declares view/ged/layout types so its public header no longer drags in libdm/libbv/QgView implementation details.
- QgGL/QgSW/QgView public headers now forward-declare their dm/bview/fb and Qt event/image types instead of exporting the libdm/libbv implementation headers transitively.
- QgItem and QgModel now keep their externally-used state behind accessors instead of exposing raw public members across libqtcad, qged, and the tests.
- QgDockWidget::m, QgTreeView::m, and QgTreeSelectionModel::treeview are now private with model()/setModel()/cadModel()/treeView() accessors; QgToolPalette::selected, selected_style, and mlayout are now private with the selectedElement() accessor for the selection; QgEdPalette updated to use selectedElement() accordingly.
- QgModel.h now uses forward declarations (struct bu_vls; struct bview; struct db_i; struct directory; struct ged;) in place of the #ifndef Q_MOC_RUN raytrace.h/ged.h include block; QgItem::name converted to a heap-allocated struct bu_vls *name_ptr; QgItem::op changed from db_op_t to int; QgAttributesModel.cpp adds its own raytrace.h include now that it is no longer pulled in transitively.
- QgTreeSelectionModel.h removes ged.h and unused STL-container includes; QgTreeView.h removes the unused STL-container includes.
- Phase 2 is complete.

- Promote raw public data to accessors where they're called from outside (compile, follow errors, add minimal getters/setters). Focus on the high-traffic classes first: QgItem/QgModel, QgSW/QgGL, QgQuadView, QgViewCtrl, QgToolPalette*.
- Introduce Q_PROPERTY for state visible to QML/Designer or for state that genuinely is user-configurable (icon size, default mouse mode, default key bindings, etc.).
- Apply the pimpl idiom to QgModel, QgGL, QgSW, QgQuadView, QgTreeView, QgAttributesModel, and QgToolPalette so the public headers no longer leak STL containers, libged structs, or per-implementation state. Keep the existing QgConsole::pqImplementation model.
- Move heavy C-header includes (raytrace.h, ged.h, dm.h, bv.h) out of the public headers wherever pimpl makes that possible. Public headers should declare opaque struct types and pull the implementations in only in the .cpp.

Phase 3 — Canvas widget unification

Current tree notes:
- QgCanvasBase abstract interface (include/qtcad/QgCanvasBase.h) introduced: non-QObject pure-virtual mixin with signals changed()/init_done() (connected via asQObject()), slots need_update()/queued_update()/set_lmouse_move_default(int), all accessors, and isValid().
- QgCanvasState pimpl struct (src/libqtcad/QgCanvasState.h) consolidates the duplicated private state of QgGL and QgSW; shared inline helpers (qgcanvas_render_size, qgcanvas_stash_hashes, qgcanvas_diff_hashes_check, qgcanvas_aet, qgcanvas_set_view) eliminate the textual duplication previously present in both canvas .cpp files.
- QgCanvasInput class (src/libqtcad/QgCanvasInput.h/.cpp) replaces the old bindings.h free-function API with a per-canvas-instance class whose drag-tracking maps are instance members rather than global statics; cross-canvas interference is therefore impossible.
- QgGL and QgSW now inherit QgCanvasBase in addition to QOpenGLWidget/QWidget; their public headers carry only a forward-declared QgCanvasState* pimpl pointer (no STL/libdm/libbv member exposure); render_to_file() and get_viewport_image() added to QgGL.
- QgView now holds a single QgCanvasBase *canvas pointer; the #ifdef BRLCAD_OPENGL duplication that previously appeared in every method body has been replaced with a single make_canvas() factory function plus virtual dispatch; the class header no longer includes QgGL.h or QgSW.h.
- Phase 3 is complete.

Phase 4 — Filter hierarchy unification

Current tree notes:
- QgViewFilter base class introduced (include/qtcad/QgViewFilter.h + src/libqtcad/QgViewFilter.cpp): QObject subclass with pimpl-hidden struct bview *v, set_view()/view() accessors, common view_sync() helper, view_updated(int) signal, and Q_DISABLE_COPY_MOVE; install/remove contract documented.
- QgPolyFilter, QgSelectFilter, QgMeasureFilter, and QgSketchFilter (and all their derived classes) now inherit QgViewFilter; duplicated per-class view_sync() implementations removed.
- QgView gains installFilter(QgViewFilter*) and clearFilter(QgViewFilter*) typed APIs; existing generic add_event_filter(QObject*)/clear_event_filter(QObject*) retained for non-QgViewFilter callers (e.g. the default key-binding controls in QgEdApp).
- All call sites in src/qged/plugins/{polygon,view/measure,view/select} ported to set_view() and installFilter/clearFilter; qsketch test integration updated similarly.
- Phase 4 is complete.

- Define a single QgViewFilter base in a new QgViewFilter.h: a QObject with a bview *v (pimpl-hidden), a common view_sync() helper, a view_updated(int) signal, and a documented contract for installing/removing from a QgView.
- Refactor QgPolyFilter, QgSelectFilter, QgMeasureFilter, QgSketchFilter (and all their derived classes) onto that base; delete the duplicated view_sync() copies.
- Move filter installation into QgView::installFilter(QgViewFilter*) rather than today's lower-level add_event_filter(QObject*) (which is too generic) — clearer ownership and lifetime.

Phase 5 — Signal/slot conventions

Current tree notes:
- QgTypes.h QG_VIEW_* compatibility constants now return QgViewUpdateFlags (a QFlags<QgViewUpdateFlag> type) instead of unsigned long long; all signals and slots carrying view-update flags use QgViewUpdateFlags.
- QgModel::view_change(ull) signal renamed to view_changed(QgViewUpdateFlags); all QgEdMainWindow connections updated accordingly.
- QgViewFilter::view_updated(int) signature updated to view_updated(QgViewUpdateFlags), and QgView::need_update(ull) updated to need_update(QgViewUpdateFlags); the qsketch test's cross-class connection now type-matches.
- QgRoles.h created (include/qtcad/QgRoles.h) with standalone QgDataRoles enum; QgModel::CADDataRoles redirects to QgDataRoles for backward compatibility; QgSignalFlags.h now includes QgRoles.h so one include gives both.
- All libqtcad signal/slot pairs (QgToolPalette, QgTreeView, QgViewCtrl, QgQuadView, QgTreeSelectionModel) and plugin classes (CADViewSelector, CADViewMeasure, QPolyCreate/Mod, CADViewSettings, QEll) updated to use QgViewUpdateFlags.
- Phase 5 is complete.

- Tighten QgSignalFlags.h into a Q_FLAG-decorated enum class and make every signal carrying view flags use it (no more unsigned long long).
- Audit and unify the signal names: pick one of view_changed/view_change/changed and remove the others (currently QgModel has both view_change(ull) and view_changed(ull)).
- Centralize the role enum: move QgModel::CADDataRoles into a shared QgRoles header and document the role IDs in one place.

Phase 6 — Model/data abstraction

Current tree notes:
- QgSession class introduced (include/qtcad/QgSession.h + src/libqtcad/QgSession.cpp): thin
  QObject that owns struct ged * / struct bview * (moved out of QgModel), emits
  db_changed(struct db_i *) and view_changed(QgViewUpdateFlags) signals for signal-driven
  invalidation, and provides an icon(struct directory *) accessor backed by a per-type
  QImage cache so icon pixel data is not reloaded for every tree node.
- QgModel refactored to create and own a QgSession; its ged() accessor now delegates to
  m_session->ged() and a new session() accessor exposes the session to external callers.
  QgModel::g_update notifies the session via notifyDbChanged() so QgAttributesModel and
  other subscribers receive db_changed without routing through QgEdApp::dbi_update.
- QgAttributesModel gains a QgSession* constructor that subscribes directly to
  session->db_changed in its initializer; the legacy struct db_i* constructor is retained
  for backward compatibility.
- QgEdMainWindow::CreateWidgets now constructs the two attribute models with the model's
  session pointer; ConnectWidgets removes the explicit ap->dbi_update → do_dbi_update wires
  for those models since the session subscription now handles them.
- QgItem icon field still holds a QImage (Qt's implicit sharing makes it copy-on-write), but
  the pixel data is now sourced from QgSession::icon() on first use per type and thereafter
  returned from the internal QHash cache, eliminating repeated resource file reads.
- Phase 6 is complete.

- Introduce a thin QgSession (or QgDbiSource) class that owns the struct ged * / struct db_i * references currently scattered across QgModel, QgAttributesModel, QgTreeView, QgViewCtrl, QgGeomImport. Models receive a QgSession* and stop talking to ged_exec directly.
- Use the new session to implement signal-driven invalidation (one QgSession::changed(...) signal that fans out to models that subscribe) instead of the current "every widget knows everyone" pattern.
- Move QgCombType/QgIcon (free QgUtil functions) and the icon cache implied by QgItem::icon to a shared icon provider on the session, so icons aren't duplicated in every tree node.

Phase 7 — Namespacing and packaging

Current tree notes:
- `src/libqtcad/CMakeLists.txt` now uses target-scoped configuration for libqtcad:
  `target_include_directories(...)` with PUBLIC/PRIVATE visibility and
  `target_link_libraries(libqtcad PUBLIC ...)` rather than passing include/lib lists
  through the `brlcad_addlib` call.
- Manual `qt*_wrap_cpp`/`qth_names` maintenance is removed.  MOC headers are now
  derived automatically from `qtcad_srcs` (`*.cpp` basename → matching
  `include/qtcad/*.h` when present), and libqtcad is built with
  `set_target_properties(libqtcad PROPERTIES AUTOMOC ON)`.
- The libqtcad build path now hard-requires Qt6 (`if(NOT Qt6Widgets_FOUND) FATAL_ERROR`)
  and no longer carries the Qt5 branch in this CMakeLists.
  **Revised:** Qt5 support has been restored; both Qt5 and Qt6 are supported via
  conditional branching on `Qt6Widgets_FOUND`.
- `include/qtcad/defines.h` now declares a top-level Doxygen `@defgroup libqtcad`
  group to anchor public API docs.
- `include/qtcad/QgNamespace.h` now provides `namespace qtcad` aliases for the
  public libqtcad API symbols, and `include/qtcad/QgNamespaceCompat.h` provides
  a one-release compatibility using block.

- Wrap all public symbols in namespace qtcad { … }; provide a deprecated using block in a compatibility header for one release.
- Switch the CMakeLists to target_include_directories/target_link_libraries with PUBLIC/PRIVATE/INTERFACE, enable CMAKE_AUTOMOC for this target only (set_target_properties(libqtcad PROPERTIES AUTOMOC ON)), and drop the hand-maintained qth_names list.
- Drop the Qt5 code paths once Qt6 is mandatory; if Qt5 must remain, hide the difference behind a single helper module.
- Add a Doxygen group @defgroup libqtcad and ensure every public class has at least a one-paragraph class-level comment.

Phase 8 — Test and CI coverage

Current tree notes:
- Headless QApplication coverage has already started: src/libqtcad/tests contains qgmodel/qgview test programs plus the offscreen ged_test_qged_swrast integration test, and the latter is wired into CTest with QT_QPA_PLATFORM=offscreen.
- QAbstractItemModelTester coverage now includes a dedicated offscreen CTest entry (test_qgmodel_model_tester) over QgModel, with QSignalSpy checks for fetch/open and layout-change signals; the old Model_Test TODO in QgModel.h is removed.

- Add headless QApplication-based unit tests (using QSignalSpy and QTest) for the most logic-heavy classes: QgModel (tree fetch/hierarchy), QgKeyValModel, QgAttributesModel, each filter family (synthetic mouse events), QgFlowLayout, QgToolPalette selection logic, QgConsole command echo / completion, QgSignalFlags flag round-tripping.
- Add a QAbstractItemModelTester instance over QgModel (the TODO in QgModel.h referencing wiki.qt.io/Model_Test becomes obsolete).
- Ensure the existing tests/ programs are run under xvfb in CI so canvas widgets actually instantiate.

Phase 9 — Threading and ownership cleanup

Current tree notes:
- QgConsoleListener: `QThread m_thread` (value member) replaced by `QThread *m_thread`
  (heap-allocated, parented to `this`).  The destructor still calls
  `m_thread->quit() + m_thread->wait()` before the QObject parent chain
  tears down the thread object.  `m_notifier` is now private; callers use
  `disconnectNotifier()` instead of touching the member directly.  A
  `QObject *parent` parameter was added to the constructor for standard
  Qt parent–child ownership.  Lifetime/threading contract documented in
  the class Doxygen comment.
- QgConsole: `listeners` map renamed to `m_listeners` and made private.
  Two public accessors added: `findListener(p,t)` and `removeListener(p,t)`.
  Listeners are constructed with `this` as QObject parent so QgConsole
  destructor automatically cleans up any listeners still alive at shutdown.
- QgEdApp.cpp (qt_delete_io_handler): updated to use `findListener()`,
  `removeListener()`, and `disconnectNotifier()` instead of reaching into
  QgConsole's internal map and QgConsoleListener's private notifier member.
- QgCanvasState.h: expanded ownership documentation for v/local_v/dmp/ifp/
  dm_set.  Fixed `qgcanvas_set_view(s, nullptr)` to revert `s.v` to
  `s.local_v` (the widget-owned view) instead of setting it to `nullptr`,
  consistent with the documented contract in QgCanvasBase.h.
- Phase 9 is complete.

Sequencing and risk

- Phases 0–2 and 5 are low-risk (mechanical, no behavior change) and unblock everything else.
- Phase 3 (canvas unification) is the highest-value change for the drawing stack work currently in flight, but it must land while qged, archer, and the dm-qtgl/dm-swrast plugins are buildable at each step.
- Phase 4 (filter unification) has a localized blast radius (mostly src/qged/plugins/{polygon,view/measure,view/select,…}); doable independently.
- Phase 6 (session abstraction) is the most invasive design change and should follow Phase 2, after the headers no longer leak C structs.
- Phase 7 (namespacing) is best done last so it only renames once, with shim headers for one release.
- Phase 8 (tests) can and should be interleaved with the earlier phases — each refactored class lands with new tests.

At every step the gating check is:

cmake -DBRLCAD_ENABLE_QT=ON ...;
cmake --build … --target libqtcad qged archer dm-qtgl dm-swrast

and the existing tests under src/libqtcad/tests must continue to pass.
