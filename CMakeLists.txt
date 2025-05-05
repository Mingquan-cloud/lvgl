cmake_minimum_required(VERSION 3.12.4)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(NOT ESP_PLATFORM)
  if(NOT (CMAKE_C_COMPILER_ID STREQUAL "MSVC"))
    project(lvgl LANGUAGES C CXX ASM HOMEPAGE_URL https://github.com/lvgl/lvgl)
  else()
    project(lvgl LANGUAGES C CXX HOMEPAGE_URL https://github.com/lvgl/lvgl)
  endif()
endif()

set(LVGL_ROOT_DIR ${CMAKE_CURRENT_LIST_DIR})

if(ESP_PLATFORM)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/esp.cmake)
elseif(ZEPHYR_BASE)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/zephyr.cmake)
elseif(MICROPY_DIR)
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/micropython.cmake)
else()
  include(${CMAKE_CURRENT_LIST_DIR}/env_support/cmake/custom.cmake)
endif()

#[[
    unfortunately CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS does not work for global data. 
    for global data we still need decl specs.
    Check out the docs to learn more about the limitations of CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS
    https://cmake.org/cmake/help/latest/prop_tgt/WINDOWS_EXPORT_ALL_SYMBOLS.html#prop_tgt:WINDOWS_EXPORT_ALL_SYMBOLS

    For all compiled sources within the library (i.e. basically all lvgl files) we need to use dllexport.
    For all compiled sources from outside the library (i.e. files which include lvgl headers) we need to use dllimport.
    We can do this by using CMakes INTERFACE and PRIVATE keyword.
  ]]
if (MSVC)
  target_compile_definitions(lvgl
    INTERFACE LV_ATTRIBUTE_EXTERN_DATA=__declspec\(dllimport\)
    PRIVATE LV_ATTRIBUTE_EXTERN_DATA=__declspec\(dllexport\)
  )
endif()

# Build executable from main.c
add_executable(lvgl_app main.c)
target_link_libraries(lvgl_app PRIVATE lvgl)

# Add include directories for the executable
target_include_directories(lvgl_app PRIVATE ${LVGL_ROOT_DIR})

if(NOT LV_CONF_BUILD_DISABLE_DEMOS)
  target_link_libraries(lvgl_app PRIVATE lvgl_demos)
endif()

if(NOT LV_CONF_BUILD_DISABLE_EXAMPLES)
  target_link_libraries(lvgl_app PRIVATE lvgl_examples)
endif()

if(NOT LV_CONF_BUILD_DISABLE_THORVG_INTERNAL)
  target_link_libraries(lvgl_app PRIVATE lvgl_thorvg)
endif()

# Set executable output directory
set_target_properties(
  lvgl_app
  PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Install executable
install(
  TARGETS lvgl_app
  RUNTIME DESTINATION "${RUNTIME_INSTALL_DIR}")