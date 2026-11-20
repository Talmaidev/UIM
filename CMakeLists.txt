cmake_minimum_required(VERSION 3.16)
project(zcsr_uim LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# zcsr_uim = UI Instance Model. It owns page/node/property/layout state.
# zcsr_ui_page_manager = one target owning page stack/route/pages.
# pages/settings and pages/menu are source subdirectories, not separate projects.
#
# For migration, point ZCSR_ENGINE_INCLUDE at the zcsr engine include directory.
# The engine is not added as a subdirectory here; link/install later.

set(ZCSR_ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../zero-coupling-signal-router/include" CACHE PATH "Path to zero-coupling-signal-router/include")

file(GLOB UIM_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
add_library(zcsr_uim STATIC ${UIM_SOURCES})
target_include_directories(zcsr_uim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_uim PRIVATE ${ZCSR_ENGINE_INCLUDE})
endif()

file(GLOB PAGE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/page_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/pages/settings/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/pages/menu/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/pages/common/*.c)
option(ZCSR_UIM_WITH_PAGE_MANAGER "Build the optional UIM page manager target" ON)
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  add_library(zcsr_ui_page_manager STATIC ${PAGE_SOURCES})
  target_include_directories(zcsr_ui_page_manager PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/include)
  if(ZCSR_ENGINE_INCLUDE)
    target_include_directories(zcsr_ui_page_manager PRIVATE ${ZCSR_ENGINE_INCLUDE})
  endif()
  target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_uim)
endif()

# UIM owns the project document (.uireproj) and asset registry. Runtime and
# editor consume this target; neither compiles editor source for project IO.
add_library(zcsr_uim_project STATIC
    src/uim_project.c
    src/uim_asset_registry.c)
target_include_directories(zcsr_uim_project PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(zcsr_uim_project PUBLIC zcsr_uim)

if(ZCSR_UIM_WITH_PAGE_MANAGER)
  # When this is built inside a consumer that already defines zcsr_ui,
  # link it without adding an engine subdirectory here.
  if(TARGET zcsr_ui)
    target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_ui)
  endif()
endif()

enable_testing()
if(NOT TARGET zcsr_ui)
  message(STATUS "UIM standalone: settings/bridge tests require a consumer providing zcsr_ui (see README.md)")
endif()
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  add_executable(page_manager_test tests/test_page_manager.c)
  target_link_libraries(page_manager_test PRIVATE zcsr_ui_page_manager)
  add_test(NAME page_manager COMMAND page_manager_test)
endif()

add_executable(component_registry_test tests/test_component_registry.c)
target_include_directories(component_registry_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(component_registry_test PRIVATE zcsr_uim)
add_test(NAME component_registry_contract COMMAND component_registry_test)

# Settings is a page in zcsr_ui_page_manager, not an engine module.
# Build this test only when both page manager and zcsr_ui are available.
if(ZCSR_UIM_WITH_PAGE_MANAGER AND TARGET zcsr_ui)
  add_executable(game_settings_overrides_test tests/test_game_settings_overrides.c)
  target_include_directories(game_settings_overrides_test PRIVATE
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(game_settings_overrides_test PRIVATE
      zcsr_ui_page_manager zcsr_ui zcsr_glrender zcsr_core)
  add_test(NAME game_settings_overrides COMMAND game_settings_overrides_test)

  add_executable(game_settings_keys_test tests/test_game_settings_keys.c)
  target_include_directories(game_settings_keys_test PRIVATE
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(game_settings_keys_test PRIVATE
      zcsr_ui_page_manager zcsr_ui zcsr_glrender zcsr_core)
  add_test(NAME game_settings_keys COMMAND game_settings_keys_test)
endif()

# UIM <-> ZCSR runtime bridge is already implemented under integrations/zcsr.
# It is NOT a new UI feature: it is the only tested path that turns the UIM
# document into real ZCSR_UI runtime nodes. Build it only when the consumer
# provides zcsr_ui; standalone UIM builds stay engine-independent.
if(TARGET zcsr_ui)
  add_library(zcsr_uim_zcsr_bridge STATIC
      integrations/zcsr/uim_zcsr_runtime_bridge.c)
  target_include_directories(zcsr_uim_zcsr_bridge PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(zcsr_uim_zcsr_bridge PUBLIC zcsr_uim zcsr_ui zcsr_camera3d)

  # Shared UIM document -> ZCSR_UI page renderer. Only built when the consumer
  # supplies zcsr_ui; standalone UIM stays engine-independent.
  add_library(zcsr_uim_page_view STATIC
      integrations/zcsr/uim_page_view.c
      integrations/zcsr/uim_page_render.c)
  target_include_directories(zcsr_uim_page_view PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(zcsr_uim_page_view PUBLIC
      zcsr_uim zcsr_uim_zcsr_bridge zcsr_ui zcsr_glrender)

  add_executable(uim_zcsr_runtime_bridge_e2e
      integrations/zcsr/uim_zcsr_runtime_bridge_e2e.c)
  target_include_directories(uim_zcsr_runtime_bridge_e2e PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(uim_zcsr_runtime_bridge_e2e PRIVATE
      zcsr_uim_zcsr_bridge zcsr_ui zcsr_core)
  add_test(NAME uim_zcsr_runtime_bridge_e2e
           COMMAND uim_zcsr_runtime_bridge_e2e)
endif()
