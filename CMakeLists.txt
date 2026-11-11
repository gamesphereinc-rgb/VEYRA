cmake_minimum_required(VERSION 3.24)
project(VEYRA VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "The first VEYRA milestone targets Windows 10/11 x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CEF_VERSION "154.0.34+g14c5a08+chromium-154.0.8037.98" CACHE STRING "Pinned stable CEF release")
if(NOT DEFINED CEF_ROOT)
  set(CEF_ROOT "${CMAKE_SOURCE_DIR}/third_party/cef/cef_binary_${CEF_VERSION}_windows64_minimal" CACHE PATH "Extracted minimal CEF distribution")
endif()
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(FATAL_ERROR "CEF not found. Run powershell -ExecutionPolicy Bypass -File scripts/setup.ps1")
endif()

# The minimal CEF package contains Release binaries only. Keep Debug available
# when a full CEF distribution is explicitly supplied by a developer.
if(EXISTS "${CEF_ROOT}/Debug/libcef.lib")
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
else()
  set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
  message(STATUS "Minimal CEF distribution detected; VEYRA is configured for Release only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")
find_package(CEF REQUIRED)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)

add_library(VEYRA SHARED
  src/main_win.cpp
  src/app_state.cpp
  src/browser_client.cpp
  src/window.cpp
  resources/veyra.rc
)
target_include_directories(VEYRA PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CEF_ROOT}")
target_compile_definitions(VEYRA PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX CEF_USE_BOOTSTRAP)
if(MSVC)
  target_compile_options(VEYRA PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
target_link_libraries(VEYRA PRIVATE
  libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS}
  gdiplus dwmapi comctl32 shlwapi shell32
)
set_target_properties(VEYRA PROPERTIES
  PREFIX "" OUTPUT_NAME "VEYRA"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

# The CEF bootstrap launcher provides the Windows sandbox handle to VEYRA.dll.
if(COMMAND SET_LPAC_ACLS)
  SET_LPAC_ACLS(VEYRA)
endif()
if(COMMAND COPY_FILES)
  COPY_FILES("VEYRA" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "$<TARGET_FILE_DIR:VEYRA>")
  COPY_FILES("VEYRA" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:VEYRA>")
endif()
add_custom_command(TARGET VEYRA POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/bootstrap.exe" "$<TARGET_FILE_DIR:VEYRA>/VEYRA.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:VEYRA>/ui"
  VERBATIM
)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(VEYRA PROPERTIES
    VS_DEBUGGER_COMMAND "$<TARGET_FILE_DIR:VEYRA>/VEYRA.exe"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VEYRA>")
endif()

message(STATUS "VEYRA uses CEF ${CEF_VERSION} at ${CEF_ROOT}")
