# UIM

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
| `zcsr_uim_component` | Smallest UI component manager: component registry, component instance, lifecycle, properties. It defines no page/node document API and has no unresolved page/node references. | `uim_component_manager.h`, `uim_component_manager_lifecycle.h`, `uim_component_registry.h` |
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

## Build

Standalone UIM (no consumer supplying `zcsr_ui`):

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Consumer build (runtime or editor) adds UIM with `add_subdirectory()` and
supplies the engine targets, including `zcsr_ui`. `ZCSR_UIM_WITH_PAGE_MANAGER`
is the legacy build switch for the independent setting page container; it
defaults to `ON` in the current tree. A consumer that does not need setting pages
sets it `OFF`. `ZCSR_UIM_WITH_MENU` builds `zcsr_ui_menu`; menu is a separate
component target, not a setting page. `ZCSR_ENGINE_INCLUDE` points at the engine
`include/` directory; UIM does not add the engine as a subdirectory.

## Test scope

Standalone UIM CTest runs two tests:

- `page_manager`
- `component_registry_contract`

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
- Text/IME input is not part of the 1.0 runtime shell contract. UIM pages that
  need text fields must wait for the engine text API to be committed and tested.
