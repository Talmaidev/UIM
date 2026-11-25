# UIM

Version: 1.0.0. The `main` branch is the canonical history.

UIM is the smallest UI component manager used by ZCSR runtime and editor. It
does not own an OS window, renderer, product loop, page document, page tree, or
page navigation. The runtime host owns the window and loop; the editor project
owns the UI document; the setting page container owns setting page identity and
visibility, while settings page source composes its rows.

This README uses these words with exactly one meaning:

- **component**: the smallest self-contained UI unit. One button is one component.
- **component instance**: one concrete copy of a component, with its own size,
  color, text, input values and identity.
- **UI document**: persisted page and placement data. It is not a live runtime
  component and it is not owned by `zcsr_uim`.
- **setting page**: one named group of setting rows that share one purpose.
- **setting page container**: the independent owner of setting page instances.
  It stores page identity and visibility. It does not own component types, a
  route stack, the menu or a UI document.
- **runtime host**: the owner of the single OS window, surface and main loop.

## Targets

| Target | Responsibility | Public headers |
| --- | --- | --- |
| `zcsr_uim` | Compatibility **INTERFACE** target linking `zcsr_uim_component` and `zcsr_uim_document`. Existing consumers keep this target name. | Aggregate of the two rows below |
| `zcsr_uim_component` | Smallest UI component manager: component registry, component instance, lifecycle, properties, plus the pure-value input modules (input scale, editable text, selection). It defines no page/node document API and has no unresolved page/node references. | `uim_component_manager.h`, `uim_component_manager_lifecycle.h`, `uim_component_registry.h`, `uim_input_scale.h`, `uim_text_input.h`, `uim_game_dpi.h`, `uim_selection.h` |
| `zcsr_uim_document` | UI page/node document, layout, persistence, presets and document-facing runtime queries. It links `zcsr_uim_component`. | `uim_component_manager.h` (page/node section), `uim_component_manager_presets.h`, `uim_component_runtime.h` |
| `zcsr_uim_project` | `.uireproj`/`.uiproj` UI project and asset registry. It links the `zcsr_uim` facade. | `uim_project.h`, `uim_asset_registry.h`, `uim_file_service.h` |
| `zcsr_ui_page_manager` | Legacy target name. The canonical concept is **setting page container**: it creates, finds, destroys and shows/hides setting page instances. It is not a route stack and has no open/back/current. It does not own menu or component types. | `zcsr_ui_page_manager/page_manager.h`, `ui_game_settings.h` |
| `zcsr_ui_menu` | Reusable menu component built only when the consumer provides `zcsr_ui`. Menu is not a setting page and the page manager never calls it. | `zcsr_ui_page_manager/ui_menu.h` |
| `zcsr_uim_zcsr_bridge` | The only tested path that renders a UI document through `zcsr_ui`. Built only when the consumer provides `zcsr_ui`. | `uim_zcsr_runtime_bridge.h` |
| `zcsr_uim_page_view` | Shared UI document to ZCSR page renderer (`uim_page_view.c` + `uim_page_render.c`). Built only when the consumer provides `zcsr_ui`. | `uim_page_view.h`, `uim_page_render.h` |

`zcsr_uim_component` and `zcsr_uim_document` have explicit source lists. Do not
replace them with `file(GLOB src/*.c)`: the component archive must stay free of
page/node symbols, and the target boundary is guarded by
`uim_component_boundary` / `uim_document_boundary` link tests.

**Compatibility note.** `uim_component_manager.h` still declares the page/node
API for source compatibility, but those symbols are compiled only into
`zcsr_uim_document`. Do not add new page/node APIs to the component target.

## Input contracts

Four pure modules live in `zcsr_uim_component`; all are dependency-free and
usable by runtime, editor or any consumer without linking a window or a
renderer.

**`uim_input_scale.h`** maps window-API pointer values into UI logical units.
`dpi_scale` is the window's own ratio (`window_dpi / 96`), never a driver
change: nothing in UIM calls `SetProcessDpiAwareness`, `SetCursorPos` or the
platform mouse-speed API. Absolute positions are divided by `dpi_scale`
only; `sensitivity` is a game-style gain applied to *deltas*
(`raw_delta / dpi_scale * sensitivity`), so the pointer still lands where the
user points while look/aim gain can be tuned. This `sensitivity` is the one
low-level gain implementation; the GameDPI component exposes it as
`multiplier` instead of duplicating the formula. Out-of-range values are clamped
and reported as `uim_ERR_RANGE`; non-finite or non-positive values are
rejected with `uim_ERR_ARGUMENT`.

**`uim_text_input.h`** is the smallest editable text component: UTF-8 buffer,
code-point-based caret and selection, maxLength that refuses instead of
truncating, focus baseline with `submit`/`cancel`, and typed value handoff
(`uim_Value` in/out, strict `int64`/`double` parses). It owns no window,
renderer, IME pump, clipboard or allocation; the host feeds key/text events in
and pushes the committed value into a component instance through the existing
manager API. The editing algorithm itself is not duplicated here: this module
and the engine widget both include `zcsr/ui_text_edit.h` (header-only), so
there is exactly one caret/selection/maxLength implementation. Mouse hit-testing
can call `uim_text_input_set_selection(anchor, cursor)`, while Shift+Arrow uses
`uim_text_input_move_cursor_extend` so the anchor stays fixed without a second
selection model in the host.

**`uim_game_dpi.h`** is the smallest GameDPI component: the only editable value
is `multiplier`, the host supplies `driver_dpi`, and
`game_dpi = driver_dpi * multiplier`. It scales relative counts by the
multiplier only; absolute hit points stay in `uim_input_scale`. That gain is not
a second formula: `uim_game_dpi_delta` delegates to
`uim_InputScale.sensitivity`, which remains the single low-level pointer-gain
primitive. The optional
`uim_GameDpiCursor` integrates scaled deltas into a window-local position,
clamps to the window rectangle, and calls a host sink. A Win32 host can use that
sink for `SetCursorPos` (with the window origin and raw-input deltas) so the
pointer gain really changes inside the window; UIM itself still never changes
system pointer speed or hardware DPI.

**`uim_selection.h`** is the bounded node-selection set that used to live in
the editor as `ui_selection`. It is a plain value (`uim_Selection` +
`ids[64]`/`count`, no heap), with stack `init`/`reset` and the previous heap
`create`/`destroy` kept for compatibility; add/remove/toggle keep
last-added-as-primary and evict the oldest entry at capacity.

## Header-only atoms (pick only what you use)

These are independent `INTERFACE` targets: no archive, no link dependency, no
allocation, no globals. Link only the atom a page needs
(`zcsr::zcsr_uim_atom_layout`) or the whole kit (`zcsr::zcsr_uim_atoms`).

| Atom target | Header | What it owns |
| --- | --- | --- |
| `zcsr_uim_atom_types` | `uim_types.h` | `uim_Result`, `uim_Rect`, `uim_Point`, `uim_Insets`; the component manager now includes it instead of redefining them |
| `zcsr_uim_atom_color` | `uim_color.h` | float RGBA value, `#RRGGBB[AA]` parse/format, source-over blend, WCAG luminance and contrast text |
| `zcsr_uim_atom_layout` | `uim_layout.h` | row/column solver with padding, gap, grow and min/max, cross alignment, grid, split, inset, overflow reporting |
| `zcsr_uim_atom_theme` | `uim_theme.h` | semantic colors, spacing steps, radii and font sizes; DPI applied once at init |
| `zcsr_uim_atom_text_layout` | `uim_text_layout.h` | measure, greedy wrap (space/hyphen/CJK) and ellipsize driven by a caller advance function; reuses the engine's UTF-8 decoder |
| `zcsr_uim_atom_scroll` | `uim_scroll.h` | clamped offsets, wheel/page steps, scrollbar thumb geometry and drag, reveal-item |
| `zcsr_uim_atom_focus` | `uim_focus.h` | id-stable tab ring, wrap-around Tab, nearest-in-direction arrows, hit test; ids must be unique (duplicate rejected) |
| `zcsr_uim_atom_placement` | `uim_placement.h` | popup/tooltip/menu below-above-right-left, flip, bounds clamp, arrow offset |
| `zcsr_uim_atom_format` | `uim_format.h` | grouped ints, fixed decimals, percent, bytes, duration; locale-independent and buffer-safe |
| `zcsr_uim_atom_progress` | `uim_progress.h` | clamped range and fraction, indeterminate phase and moving window |
| `zcsr_uim_atom_virtual_list` | `uim_virtual_list.h` | total height, visible range with overscan, item rect, index-at-point, scroll-to-index |
| `zcsr_uim_atom_anim` | `uim_anim.h` | easing curves and one tween (loop, ping-pong, stop); `start` resets both flags |
| `zcsr_uim_atom_tabs` | `uim_tabs.h` | active index, wrap/clamp step, hit test |
| `zcsr_uim_atom_drag` | `uim_drag.h` | press/threshold/axis-lock/end-cancel for sliders, splitters, scrollbars and node drag |
| `zcsr_uim_atom_button` | `uim_button.h` | hover/press/release click edge, keyboard activation and press-and-hold repeat |
| `zcsr_uim_atom_toggle` | `uim_toggle.h` | checkbox/switch state and click edge |
| `zcsr_uim_atom_slider` | `uim_slider.h` | clamped value, track mapping and drag |
| `zcsr_uim_atom_dropdown` | `uim_dropdown.h` | open state, selected index, wrap/clamp step and item hit test |
| `zcsr_uim_atom_stepper` | `uim_stepper.h` | clamped numeric value, step and optional wrap |
| `zcsr_uim_atom_toast` | `uim_toast.h` | bounded notification queue, dedupe by id, priority eviction, tick |
| `zcsr_uim_atom_numeric_field` | `uim_numeric_field.h` | clamped int/float field: step, drag delta, strict typed parse, commit/revert baseline, read-only |
| `zcsr_uim_atom_dialog` | `uim_dialog.h` | bounded modal/non-modal dialog stack, accept/cancel/close results, focus save/restore |
| `zcsr_uim_atom_menu` | `uim_menu.h` | flat item model for menu bars, dropdowns and context menus; open header, hover, keyboard step, hit test |
| `zcsr_uim_atom_tree` | `uim_tree.h` | caller-id expand/collapse set and primary selection, decoupled from node data and IO |
| `zcsr_uim_atom_tooltip` | `uim_tooltip.h` | hover-delay show/hide and re-trigger; placement delegates to `placement` |
| `zcsr_uim_atom_property_row` | `uim_property_row.h` | label/editor row split plus enabled/read-only interaction state |
| `zcsr_uim_atom_splitter` | `uim_splitter.h` | sash ratio with per-pane minimum sizes; pane rects delegate to `layout_split` |

Cross-atom edges are only header includes: `theme` -> `color`, `scroll` ->
`layout`, `text_layout` -> `types` + `ZCSR_ENGINE_INCLUDE` (the UTF-8 decoder
lives in `zcsr/ui_text_edit.h` so there is exactly one decoder in the tree).

The settings page no longer keeps its private `strlen * 8 * scale` width helper
or its space-only wrapper: it calls `uim_text_measure` / `uim_text_wrap`. ASCII
widths are unchanged; CJK is now counted per glyph instead of three bytes.

## C++ consumption

UIM is a C11 library (`LANGUAGES C`), but its public headers are also consumed
from C++ (the editor is a C++ translation unit). Two rules keep that safe:

- every public declaration sits inside `extern "C"`; a C++ caller links the
  same unmangled symbols as a C caller;
- headers avoid C-only constructs (compound literals, designated initializers,
  `_Generic`, VLAs, implicit `void*` conversions).

`uim_cpp_consumer_contract` is the regression guard: a C++20 TU includes the
UIM headers, calls the manager, text input, selection, input scale, layout,
format and text-layout APIs, and links `zcsr_uim_component`. C++20 is the
strictest probe, not a UIM requirement; `uim_cpp_consumer_cxx14_contract`
compiles and links the same TU at C++14. Both targets are built only when UIM
is the top-level project and a C++ compiler exists; adding UIM to a pure-C
parent does not turn on `CXX` or require a C++ compiler.

## Build

Standalone UIM (no consumer supplying `zcsr_ui`):

```bat
cmake -S . -B ..\zcsr-build\UIM\main -G "Visual Studio 18 2026" -A x64
cmake --build ..\zcsr-build\UIM\main --config Release
ctest --test-dir ..\zcsr-build\UIM\main -C Release --output-on-failure
```

Consumer build (runtime or editor) adds UIM with `add_subdirectory()` and
supplies the engine targets, including `zcsr_ui`. `ZCSR_UIM_WITH_PAGE_MANAGER`
is the legacy build switch for the independent setting page container; it
defaults to `ON` in the current tree. A consumer that does not need setting pages
sets it `OFF`. `ZCSR_UIM_WITH_MENU` builds `zcsr_ui_menu`; menu is a separate
component target, not a setting page. `ZCSR_ENGINE_INCLUDE` points at the engine
`include/` directory; UIM does not add the engine as a subdirectory.

## Test scope

Standalone UIM CTest runs the core contract tests:

- `page_manager`
- `component_registry_contract`
- `uim_component_boundary`
- `uim_document_boundary`
- `uim_file_service_abi`
- `uim_input_scale_contract`
- `uim_text_input_contract`
- `uim_game_dpi_contract`
- `uim_selection_contract`
- `uim_atoms_contract`
- `uim_atoms_stress_contract`
- `uim_cpp_consumer_contract` (only when a C++ compiler is available)
- `uim_cpp_consumer_cxx14_contract` (same condition)

That standalone result does **not** cover settings-page keyboard behavior,
settings overrides, or the ZCSR runtime bridge. Those tests are registered only
when UIM is added to a consumer build that already provides the `zcsr_ui`
target (for example `zcsr_runtime` or `zcsr_editor`):

- `game_settings_overrides`
- `game_settings_keys`
- `uim_zcsr_runtime_bridge_e2e`

`game_settings_keys` drives `zcsr_game_settings_handle_key()` with
`ZCSR_KEY_*`, including Q/E tab switching, Up/Down, Enter/Space, Tab, Esc and
W/S/A/D item movement. A standalone UIM CI result must not be treated as a
settings-page keyboard gate. Runtime's shell-level settings routing test is
registered separately in `zcsr_runtime` and requires
`ZCSR_RUNTIME_WITH_SETTINGS_PAGE=ON`.

## Release 1.0 boundary

- Page textures in `uim_page_view.c` load through the committed
  `zcsr_assets_texture()` API. The experimental zero-heap
  `zcsr_assets_texture_with_buffers()` path and its 40 MB private
  `image_encoded`/`image_arena` buffers are not part of 1.0.
- `zcsr_uim_project` owns persisted UI document content; the editor owns the
  live editing document; the setting page container owns setting page identity
  and visibility, while settings-page source composes rows from component
  instances; runtime owns the single window, loop and input pump. Do not create a
  second window or renderer inside a UI document or setting page.
- Text editing state is part of UIM 1.0 (`uim_text_input.h`): buffer, caret,
  selection, focus baseline, commit/revert and typed value handoff, all covered
  by `uim_text_input_contract`. Glyph rendering, IME candidate windows and the
  editor/player viewport hit-to-caret bridge are still host responsibilities;
  a page may carry `builtin.text_input` and an editor may edit its value, but
  no UI page renders a live text field until the engine text API is committed
  and tested. DPI conversion is likewise host-supplied through
  `uim_input_scale.h`; UIM still does not create a window, change DPI awareness
  or touch the OS pointer driver.

## License

本项目按 GNU Affero General Public License v3.0（AGPL-3.0）授权，完整许可证文本见仓库根目录 `LICENSE`。版权行：Copyright (C) 2026 Talmaidev。
