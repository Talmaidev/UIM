# UIM

UIM is the UI instance/document/page model used by ZCSR runtime and editor.

## Test scope

Standalone UIM build (`cmake -S . -B build` with no consumer supplying
`zcsr_ui`) runs only two tests:

- `page_manager`
- `component_registry_contract`

That standalone CTest does **not** cover settings-page keyboard behavior,
settings overrides, or the ZCSR runtime bridge. Those tests are registered only
when UIM is added to a consumer build that already provides the `zcsr_ui`
target (for example `zcsr_runtime` or `zcsr_editor`):

- `game_settings_overrides`
- `game_settings_keys`
- `uim_zcsr_runtime_bridge_e2e`

`game_settings_keys` drives `zcsr_game_settings_handle_key()` with
`ZCSR_KEY_*`, including Q/E tab switching, Up/Down, Enter/Space, Tab, Esc and
W/S/A/D item movement. A standalone UIM CI result must not be treated as a
settings-page keyboard gate.

Runtime's own shell-level settings routing test is registered separately in
`zcsr_runtime` and requires `ZCSR_RUNTIME_WITH_SETTINGS_PAGE=ON`.
