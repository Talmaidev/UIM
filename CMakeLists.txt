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

set(ZCSR_ENGINE_INCLUDE "C:/Users/chen/Documents/GitHub/zero-coupling-signal-router/include" CACHE PATH "Path to zero-coupling-signal-router/include")

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
add_library(zcsr_ui_page_manager STATIC ${PAGE_SOURCES})
target_include_directories(zcsr_ui_page_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/zcsr_ui_page_manager/include)
if(ZCSR_ENGINE_INCLUDE)
  target_include_directories(zcsr_ui_page_manager PRIVATE ${ZCSR_ENGINE_INCLUDE})
endif()
target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_uim)

# When this is built inside a consumer that already defines zcsr_ui,
# link it without adding an engine subdirectory here.
if(TARGET zcsr_ui)
  target_link_libraries(zcsr_ui_page_manager PUBLIC zcsr_ui)
endif()

enable_testing()
add_executable(page_manager_test tests/test_page_manager.c)
target_link_libraries(page_manager_test PRIVATE zcsr_ui_page_manager)
add_test(NAME page_manager COMMAND page_manager_test)

# Settings is a page in zcsr_ui_page_manager, not an engine module.
# Build this test only when the consuming build also provides zcsr_ui.
if(TARGET zcsr_ui)
  add_executable(game_settings_overrides_test tests/test_game_settings_overrides.c)
  target_include_directories(game_settings_overrides_test PRIVATE
      ${ZCSR_ENGINE_INCLUDE})
  target_link_libraries(game_settings_overrides_test PRIVATE
      zcsr_ui_page_manager zcsr_ui zcsr_glrender zcsr_core)
  add_test(NAME game_settings_overrides COMMAND game_settings_overrides_test)
endif()
