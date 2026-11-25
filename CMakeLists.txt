cmake_minimum_required(VERSION 3.16)

# --- Hard guard: the build tree must live OUTSIDE the source tree ------------
# Repositories under C:\Users\chen\Documents\GitHub (and their Linux checkouts)
# must stay free of build output: no in-source builds, no in-repo build/ or
# build-*/ directories, no scratch CMakeCache.txt. This is enforced here at
# configure time so a stale command line cannot silently re-pollute the tree.
#
# Guard interval: CMAKE_BINARY_DIR == CMAKE_SOURCE_DIR, or CMAKE_BINARY_DIR is
# located underneath CMAKE_SOURCE_DIR. Comparison is case-insensitive on
# Windows (paths there are case-insensitive) and separator-aware, so a sibling
# directory whose name merely starts with the source path is still allowed.
if(CMAKE_BINARY_DIR)
  # Normalise to forward slashes first (file(TO_CMAKE_PATH) exists since 3.16,
  # unlike cmake_path, which would raise the minimum CMake version).
  file(TO_CMAKE_PATH "${CMAKE_SOURCE_DIR}" _zcsr_guard_src)
  file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}" _zcsr_guard_bin)
  if(WIN32)
    string(TOLOWER "${_zcsr_guard_src}" _zcsr_guard_src_cmp)
    string(TOLOWER "${_zcsr_guard_bin}" _zcsr_guard_bin_cmp)
  else()
    set(_zcsr_guard_src_cmp "${_zcsr_guard_src}")
    set(_zcsr_guard_bin_cmp "${_zcsr_guard_bin}")
  endif()
  string(LENGTH "${_zcsr_guard_src_cmp}" _zcsr_guard_src_len)
  string(SUBSTRING "${_zcsr_guard_bin_cmp}" 0 ${_zcsr_guard_src_len}
         _zcsr_guard_bin_head)
  set(_zcsr_guard_in_repo FALSE)
  if(_zcsr_guard_bin_cmp STREQUAL _zcsr_guard_src_cmp)
    set(_zcsr_guard_in_repo TRUE)
  elseif(_zcsr_guard_bin_head STREQUAL _zcsr_guard_src_cmp)
    string(SUBSTRING "${_zcsr_guard_bin_cmp}" ${_zcsr_guard_src_len} 1
           _zcsr_guard_sep)
    if(_zcsr_guard_sep STREQUAL "/" OR _zcsr_guard_sep STREQUAL "\\")
      set(_zcsr_guard_in_repo TRUE)
    endif()
  endif()
  if(_zcsr_guard_in_repo)
    message(FATAL_ERROR
      "In-repo build directory refused by ZCSR build-layout guard.\n"
      "  CMAKE_SOURCE_DIR = ${CMAKE_SOURCE_DIR}\n"
      "  CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR}\n"
      "\n"
      "Build trees must live outside the source tree: this repository (like\n"
      "every repository under C:\\Users\\chen\\Documents\\GitHub) must stay clean.\n"
      "Put the build tree at one of these instead:\n"
      "  Windows: C:\\Users\\chen\\Documents\\build\\zcsr\\UIM\\<preset>\n"
      "  Linux:   /home/chen/zcsr-builds/UIM/<preset>\n"
      "\n"
      "Example (Windows, Visual Studio generator):\n"
      "  cmake -S . -B \"C:\\Users\\chen\\Documents\\build\\zcsr\\UIM\\win-msvc\" -G \"Visual Studio 18 2026\"\n"
      "Example (Linux):\n"
      "  cmake -S . -B /home/chen/zcsr-builds/UIM/linux-gcc\n"
      "\n"
      "If an old in-repo build directory already exists, delete it and use the\n"
      "layout above; see C:\\Users\\chen\\Documents\\build\\_reports\\BUILD_LAYOUT.md.\n")
  endif()
  unset(_zcsr_guard_src)
  unset(_zcsr_guard_bin)
  unset(_zcsr_guard_src_cmp)
  unset(_zcsr_guard_bin_cmp)
  unset(_zcsr_guard_src_len)
  unset(_zcsr_guard_bin_head)
  unset(_zcsr_guard_sep)
  unset(_zcsr_guard_in_repo)
endif()

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
# A parent project (runtime/editor) may have created this cache entry as empty;
# an empty path would silently drop the engine headers that
# uim_text_input.c needs for zcsr/ui_text_edit.h. Fall back to the sibling repo.
if(NOT ZCSR_ENGINE_INCLUDE)
  set(ZCSR_ENGINE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../zero-coupling-signal-router/include")
endif()

# Explicit component source list: zcsr_uim_component is the core UI instance
# model only. Page/node document APIs live in zcsr_uim_document. Project
# document and asset registry belong to zcsr_uim_project. Never glob src/*.c
# here, or target boundaries collapse.
set(UIM_COMPONENT_SOURCES
    src/uim_component_manager.c
    src/uim_component_manager_lifecycle.c
    src/uim_component_registry.c
    src/uim_game_dpi.c
    src/uim_input_scale.c
    src/uim_text_input.c
    src/uim_selection.c)
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

# --- Header-only atoms: pick only what you use --------------------------------
# Every atom is an independent INTERFACE target with no archive and no link
# dependency: a page that only needs layout links zcsr_uim_atom_layout, a host
# that wants the full kit links zcsr_uim_atoms. Cross-atom edges exist only
# where one header includes another (theme->color, scroll->layout,
# text_layout->types + the engine's UTF-8 decoder).
set(UIM_ATOMS types color layout theme text_layout scroll focus placement
              format progress virtual_list anim tabs toast drag
              button toggle slider dropdown stepper
              numeric_field dialog menu tree tooltip property_row splitter)
set(UIM_ATOM_TARGETS)
foreach(atom ${UIM_ATOMS})
  add_library(zcsr_uim_atom_${atom} INTERFACE)
  target_include_directories(zcsr_uim_atom_${atom} INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  list(APPEND UIM_ATOM_TARGETS zcsr_uim_atom_${atom})
endforeach()
foreach(atom ${UIM_ATOMS})
  if(NOT atom STREQUAL "types")
    target_link_libraries(zcsr_uim_atom_${atom} INTERFACE zcsr_uim_atom_types)
  endif()
endforeach()
target_link_libraries(zcsr_uim_atom_theme INTERFACE zcsr_uim_atom_color)
target_link_libraries(zcsr_uim_atom_scroll INTERFACE zcsr_uim_atom_layout)
target_link_libraries(zcsr_uim_atom_drag INTERFACE zcsr_uim_atom_layout)
target_link_libraries(zcsr_uim_atom_property_row INTERFACE zcsr_uim_atom_layout)
target_link_libraries(zcsr_uim_atom_splitter INTERFACE zcsr_uim_atom_layout)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_uim_atom_text_layout INTERFACE
      $<BUILD_INTERFACE:${ZCSR_ENGINE_INCLUDE}>)
endif()
add_library(zcsr_uim_atoms INTERFACE)
target_link_libraries(zcsr_uim_atoms INTERFACE ${UIM_ATOM_TARGETS})

# --- Install / export package -------------------------------------------------
set(UIM_INSTALL_TARGETS zcsr_uim zcsr_uim_component zcsr_uim_document zcsr_uim_project
    ${UIM_ATOM_TARGETS} zcsr_uim_atoms)
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

# Input contract: window-API pointer -> logical units plus game sensitivity,
# and the editable text component with strict data handoff.
add_executable(input_scale_test tests/test_input_scale.c)
target_include_directories(input_scale_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(input_scale_test PRIVATE zcsr_uim_component)
add_test(NAME uim_input_scale_contract COMMAND input_scale_test)

add_executable(text_input_test tests/test_text_input.c)
target_include_directories(text_input_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(text_input_test PRIVATE zcsr_uim_component)
add_test(NAME uim_text_input_contract COMMAND text_input_test)

add_executable(game_dpi_test tests/test_game_dpi.c)
target_include_directories(game_dpi_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(game_dpi_test PRIVATE zcsr_uim_component)
add_test(NAME uim_game_dpi_contract COMMAND game_dpi_test)

add_executable(selection_test tests/test_selection.c)
target_include_directories(selection_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(selection_test PRIVATE zcsr_uim_component)
add_test(NAME uim_selection_contract COMMAND selection_test)

# One contract test for all header-only atoms: geometry, text layout, scroll,
# focus, placement, formatting, progress, virtual list, easing, tabs, toasts.
add_executable(atoms_test tests/test_atoms.c)
target_link_libraries(atoms_test PRIVATE zcsr_uim_atoms)
add_test(NAME uim_atoms_contract COMMAND atoms_test)

add_executable(atoms_stress_test tests/test_atoms_stress.c)
target_link_libraries(atoms_stress_test PRIVATE zcsr_uim_atoms)
add_test(NAME uim_atoms_stress_contract COMMAND atoms_stress_test)

# Header-only atoms use hypotf/fmodf/powf/floorf/ceilf. On Unix these live in
# libm; MSVC and Apple's libSystem resolve them without a separate link item.
if(UNIX AND NOT APPLE)
  target_link_libraries(atoms_test PRIVATE m)
  target_link_libraries(atoms_stress_test PRIVATE m)
endif()

# C++ consumption contract: UIM is C11, but its public headers are included by
# C++ consumers (the editor). Build this only when UIM is the top-level project
# so adding UIM to a pure-C parent never turns on a C++ language requirement.
set(ZCSR_UIM_CPP_TEST_AVAILABLE OFF)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  include(CheckLanguage)
  check_language(CXX)
  if(CMAKE_CXX_COMPILER)
    enable_language(CXX)
    add_executable(cpp_consumer_test tests/test_cpp_consumer.cpp)
    set_target_properties(cpp_consumer_test PROPERTIES
        CXX_STANDARD 20
        CXX_STANDARD_REQUIRED ON)
    target_link_libraries(cpp_consumer_test PRIVATE zcsr_uim_component zcsr_uim_atoms)
    add_test(NAME uim_cpp_consumer_contract COMMAND cpp_consumer_test)

    # Same source at C++14: proves the public C headers do not require C++20.
    add_executable(cpp_consumer_cxx14_test tests/test_cpp_consumer.cpp)
    set_target_properties(cpp_consumer_cxx14_test PROPERTIES
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON)
    target_link_libraries(cpp_consumer_cxx14_test PRIVATE zcsr_uim_component zcsr_uim_atoms)
    add_test(NAME uim_cpp_consumer_cxx14_contract COMMAND cpp_consumer_cxx14_test)

    set(ZCSR_UIM_CPP_TEST_AVAILABLE ON)
  else()
    message(STATUS "UIM: no C++ compiler; cpp consumer contract test skipped")
  endif()
endif()

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
    uim_document_boundary uim_file_service_abi
    uim_input_scale_contract uim_text_input_contract
    uim_game_dpi_contract
    uim_selection_contract uim_atoms_contract uim_atoms_stress_contract)
if(ZCSR_UIM_CPP_TEST_AVAILABLE)
  list(APPEND ZCSR_UIM_TESTS uim_cpp_consumer_contract uim_cpp_consumer_cxx14_contract)
endif()
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
