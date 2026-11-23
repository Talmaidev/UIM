cmake_minimum_required(VERSION 3.16)
project(zcsr_uim VERSION 1.0.0 LANGUAGES C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(MSVC)
  add_compile_options(/utf-8)
endif()

# zcsr_uim_component = component registry, instances and property values.
# zcsr_uim_document = page/node document, layout, persistence, presets and
#   document-facing runtime queries; it links zcsr_uim_component.
# zcsr_uim = compatibility INTERFACE target linking both. Existing consumers
#   keep linking zcsr_uim; the component archive no longer defines page/node
#   symbols.
# zcsr_ui_page_manager = one independent setting page container.
# It owns setting page instances only. It is not a route stack and does not
# own menu, UI document, component types, scene or runtime.
#
# For migration, point ZCSR_ENGINE_INCLUDE at the zcsr engine include directory.
# The engine is not added as a subdirectory here; link/install later.

set(ZCSR_ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../zero-coupling-signal-router/include" CACHE PATH "Path to zero-coupling-signal-router/include")

# Explicit component source list: zcsr_uim_component is the core UI instance
# model only. Page/node document APIs live in zcsr_uim_document. Project
# document and asset registry belong to zcsr_uim_project. Never glob src/*.c
# here, or target boundaries collapse.
set(UIM_COMPONENT_SOURCES
    src/uim_component_manager.c
    src/uim_component_manager_lifecycle.c
    src/uim_component_registry.c)
add_library(zcsr_uim_component STATIC ${UIM_COMPONENT_SOURCES})
add_library(zcsr_uim_core ALIAS zcsr_uim_component)
target_include_directories(zcsr_uim_component PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_uim_component PRIVATE ${ZCSR_ENGINE_INCLUDE})
endif()

# Page/node document APIs are a separate physical target.  They share the
# manager's private storage layout but own their own symbols, so linking
# zcsr_uim_component alone no longer pulls page/node document code.
add_library(zcsr_uim_document STATIC
    src/uim_document.c
    src/uim_component_manager_persist.c
    src/uim_component_manager_presets.c
    src/uim_component_runtime.c)
target_link_libraries(zcsr_uim_document PUBLIC zcsr_uim_component)
target_include_directories(zcsr_uim_document PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_uim_document PRIVATE ${ZCSR_ENGINE_INCLUDE})
endif()

# Compatibility facade: existing consumers keep linking zcsr_uim and receive
# both the component registry and the document layer.
add_library(zcsr_uim INTERFACE)
target_include_directories(zcsr_uim INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(zcsr_uim INTERFACE zcsr_uim_component zcsr_uim_document)

set(PAGE_MANAGER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/page_manager.c
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/pages/settings/settings_page.c)
# Legacy target name. Canonical concept: independent setting page container.
option(ZCSR_UIM_WITH_PAGE_MANAGER "Build the independent setting page container target" ON)
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  add_library(zcsr_ui_page_manager STATIC ${PAGE_MANAGER_SOURCES})
  add_library(zcsr_page_manager ALIAS zcsr_ui_page_manager)
  target_include_directories(zcsr_ui_page_manager PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  if(ZCSR_ENGINE_INCLUDE)
    target_include_directories(zcsr_ui_page_manager PRIVATE ${ZCSR_ENGINE_INCLUDE})
  endif()
  target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_uim)
endif()

# Menu is not a setting page. It is a separate reusable menu component target;
# the setting page container never calls it.
option(ZCSR_UIM_WITH_MENU "Build the reusable menu component target" ON)
if(ZCSR_UIM_WITH_MENU AND TARGET zcsr_ui)
  add_library(zcsr_ui_menu STATIC
      ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/src/pages/menu/menu_page.c)
  target_include_directories(zcsr_ui_menu PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      $<BUILD_INTERFACE:${ZCSR_ENGINE_INCLUDE}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_link_libraries(zcsr_ui_menu PUBLIC zcsr_ui zcsr_glrender)
endif()

# UIM owns the project document (.uireproj) and asset registry. Runtime and
# editor consume this target; neither compiles editor source for project IO.
add_library(zcsr_uim_project STATIC
    src/uim_project.c
    src/uim_asset_registry.c
    src/uim_file_service.c)
target_include_directories(zcsr_uim_project PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(zcsr_uim_project PUBLIC zcsr_uim)

if(ZCSR_UIM_WITH_PAGE_MANAGER)
  # When this is built inside a consumer that already defines zcsr_ui,
  # link it without adding an engine subdirectory here.
  if(TARGET zcsr_ui)
    target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_ui)
  endif()
endif()

# --- Install / export package -------------------------------------------------
set(UIM_INSTALL_TARGETS zcsr_uim zcsr_uim_component zcsr_uim_document zcsr_uim_project)
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  list(APPEND UIM_INSTALL_TARGETS zcsr_ui_page_manager)
endif()
if(ZCSR_UIM_WITH_MENU AND TARGET zcsr_ui_menu)
  list(APPEND UIM_INSTALL_TARGETS zcsr_ui_menu)
endif()
if(TARGET zcsr_uim_zcsr_bridge)
  list(APPEND UIM_INSTALL_TARGETS zcsr_uim_zcsr_bridge zcsr_uim_page_view)
endif()

install(TARGETS ${UIM_INSTALL_TARGETS}
  EXPORT zcsr_uimTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  install(DIRECTORY zcsr_ui_page_manager/include/
          DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(EXPORT zcsr_uimTargets
  FILE zcsr_uimTargets.cmake
  NAMESPACE zcsr::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_uim)
configure_package_config_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/zcsr_uimConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_uimConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_uim)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_uimConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_uimConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/zcsr_uimConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/zcsr_uim)

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

# Link-boundary guards: component-only consumers must not need page/node
# symbols, and document consumers must get the page/node implementation.
add_executable(component_boundary_test tests/test_component_boundary.c)
target_include_directories(component_boundary_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(component_boundary_test PRIVATE zcsr_uim_component)
add_test(NAME uim_component_boundary COMMAND component_boundary_test)

add_executable(document_boundary_test tests/test_document_boundary.c)
target_include_directories(document_boundary_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(document_boundary_test PRIVATE zcsr_uim_document)
add_test(NAME uim_document_boundary COMMAND document_boundary_test)

add_executable(file_service_test tests/test_file_service.c)
target_include_directories(file_service_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(file_service_test PRIVATE zcsr_uim_project)
add_test(NAME uim_file_service_abi COMMAND file_service_test)

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

# Every UIM test can be selected independently by module label.
set(ZCSR_UIM_TESTS
    component_registry_contract uim_component_boundary
    uim_document_boundary uim_file_service_abi)
if(ZCSR_UIM_WITH_PAGE_MANAGER)
  list(APPEND ZCSR_UIM_TESTS page_manager)
endif()
if(TARGET game_settings_overrides_test)
  list(APPEND ZCSR_UIM_TESTS game_settings_overrides)
endif()
if(TARGET game_settings_keys_test)
  list(APPEND ZCSR_UIM_TESTS game_settings_keys)
endif()
set_tests_properties(${ZCSR_UIM_TESTS} PROPERTIES
    LABELS "module;uim;fast")

# UIM <-> ZCSR runtime bridge is already implemented under integrations/zcsr.
# It is NOT a new UI feature: it is the only tested path that turns the UIM
# document into real ZCSR_UI runtime nodes. Build it only when the consumer
# provides zcsr_ui; standalone UIM builds stay engine-independent.
if(TARGET zcsr_ui)
  add_library(zcsr_uim_zcsr_bridge STATIC
      integrations/zcsr/uim_zcsr_runtime_bridge.c)
  target_include_directories(zcsr_uim_zcsr_bridge PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      $<BUILD_INTERFACE:${ZCSR_ENGINE_INCLUDE}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_link_libraries(zcsr_uim_zcsr_bridge PUBLIC zcsr_uim zcsr_ui zcsr_camera3d)

  # Shared UIM document -> ZCSR_UI page renderer. Only built when the consumer
  # supplies zcsr_ui; standalone UIM stays engine-independent.
  add_library(zcsr_uim_page_view STATIC
      integrations/zcsr/uim_page_view.c
      integrations/zcsr/uim_page_render.c)
  target_include_directories(zcsr_uim_page_view PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
      $<BUILD_INTERFACE:${ZCSR_ENGINE_INCLUDE}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
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

  # Module-scoped label for the bridge test added in this directory.
  set_tests_properties(uim_zcsr_runtime_bridge_e2e PROPERTIES
      LABELS "module;uim;fast")

  # Bridge/page-view targets are created after the common install block, so
  # append them to the same export set here.
  install(TARGETS zcsr_uim_zcsr_bridge zcsr_uim_page_view
    EXPORT zcsr_uimTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
