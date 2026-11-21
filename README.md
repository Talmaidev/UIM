# UIM

UIM is the UI instance/document/page model used by ZCSR runtime and editor. It
does not own an OS window, renderer, or product loop; those belong to runtime.
This README is the usage overview for every UIM target. API details live in the
public headers.

## Targets

| Target | Responsibility | Public headers |
| --- | --- | --- |
| `zcsr_uim` | Core instance model: component manager, lifecycle, persistence, presets, component registry and runtime. | `uim_component_manager.h`, `uim_component_manager_lifecycle.h`, `uim_component_manager_presets.h`, `uim_component_registry.h`, `uim_component_runtime.h` |
| `zcsr_uim_project` | `.uireproj` project document and asset registry. Runtime and editor consume this target; neither compiles project IO itself. | `uim_project.h`, `uim_asset_registry.h` |
| `zcsr_ui_page_manager` | Optional page stack/route/pages target. Settings and menu are source subdirectories, not separate projects. | `zcsr_ui_page_manager/page_manager.h`, `ui_game_settings.h`, `ui_menu.h` |
| `zcsr_uim_zcsr_bridge` | The only tested path that turns a UIM document into real `zcsr_ui` runtime nodes. Built only when the consumer provides `zcsr_ui`. | `uim_zcsr_runtime_bridge.h` |
| `zcsr_uim_page_view` | Shared UIM document to ZCSR page renderer (`uim_page_view.c` + `uim_page_render.c`). Built only when the consumer provides `zcsr_ui`. | `uim_page_view.h`, `uim_page_render.h` |

`zcsr_uim` has an explicit six-source list. `uim_project.c` and
`uim_asset_registry.c` belong only to `zcsr_uim_project`. Do not replace the
list with `file(GLOB src/*.c)`: consumers linking `zcsr_uim` alone must not
receive project/asset symbols, and the target boundary must stay enforceable.

## Build

Standalone UIM (no consumer supplying `zcsr_ui`):

```bat
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Consumer build (runtime or editor) adds UIM with `add_subdirectory()` and
supplies the engine targets, including `zcsr_ui`. `ZCSR_UIM_WITH_PAGE_MANAGER`
defaults to `ON`; set it `OFF` only when the consumer does not need pages.
`ZCSR_ENGINE_INCLUDE` points at the engine `include/` directory; UIM does not
add the engine as a subdirectory.

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
- UIM owns page content; runtime owns the single window, loop and input pump.
  Do not create a second window or renderer inside a UIM page.
- Text/IME input is not part of the 1.0 runtime shell contract. UIM pages that
  need text fields must wait for the engine text API to be committed and tested.
