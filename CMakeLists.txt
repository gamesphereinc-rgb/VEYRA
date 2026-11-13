cmake_minimum_required(VERSION 3.24)
project(VEYRA VERSION 0.4.1 LANGUAGES CXX RC)

if(NOT WIN32)
  message(FATAL_ERROR "The first VEYRA milestone targets Windows 10/11 x64.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Generate a real Windows icon before compiling the application resources.
find_program(POWERSHELL_EXECUTABLE NAMES powershell pwsh REQUIRED)
execute_process(
  COMMAND "${POWERSHELL_EXECUTABLE}" -NoLogo -NoProfile -ExecutionPolicy Bypass
          -File "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate-icon.ps1"
          -OutputPath "${CMAKE_CURRENT_SOURCE_DIR}/resources/veyra.ico"
  RESULT_VARIABLE VEYRA_ICON_GENERATION_RESULT
)
if(NOT VEYRA_ICON_GENERATION_RESULT EQUAL 0 OR
   NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/veyra.ico")
  message(FATAL_ERROR "VEYRA application icon generation failed.")
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated-ui")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ui/version.js.in"
               "${CMAKE_BINARY_DIR}/generated-ui/version.js" @ONLY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/veyra_version.h.in"
               "${CMAKE_BINARY_DIR}/generated/veyra_version.h" @ONLY)

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/installer")
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/out" VEYRA_BUILD_DIRECTORY)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/resources/veyra.ico" VEYRA_ICON_PATH)
file(TO_CMAKE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" VEYRA_LICENSE_PATH)
file(TO_CMAKE_PATH "${CMAKE_BINARY_DIR}/installer" VEYRA_INSTALLER_DIRECTORY)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/installer/veyra.iss.in"
               "${CMAKE_BINARY_DIR}/veyra.iss" @ONLY)

set(CEF_VERSION "156.0.7+g47dbc1d+chromium-156.0.8078.17" CACHE STRING "Pinned CEF beta build for Chromium 156 testing")
if(NOT DEFINED CEF_ROOT)
  set(CEF_ROOT "${CMAKE_SOURCE_DIR}/third_party/cef/cef_binary_${CEF_VERSION}_windows64_beta_minimal" CACHE PATH "Extracted minimal CEF distribution")
endif()
if(NOT EXISTS "${CEF_ROOT}/cmake/FindCEF.cmake")
  message(FATAL_ERROR "CEF not found. Run powershell -ExecutionPolicy Bypass -File scripts/setup.ps1")
endif()

# CEF's generated CMake macros pass library paths through set_target_properties.
# Normalize Windows drive paths first so backslashes are not parsed as escapes
# by newer CMake policies (for example, the "\a" in "D:\a\...").
file(TO_CMAKE_PATH "${CEF_ROOT}" CEF_ROOT)

# The minimal CEF package contains Release binaries only. Keep Debug available
# when a full CEF distribution is explicitly supplied by a developer.
if(EXISTS "${CEF_ROOT}/Debug/libcef.lib")
  set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
else()
  set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)
  message(STATUS "Minimal CEF distribution detected; VEYRA is configured for Release only.")
endif()

list(APPEND CMAKE_MODULE_PATH "${CEF_ROOT}/cmake")

# The CEF Windows wrapper defaults to /MT, but Visual Studio application
# targets use /MD by default. Keep the wrapper on the same CRT to avoid
# LNK2038 RuntimeLibrary mismatches and duplicate CRT symbols at link time.
if(MSVC)
  set(CEF_RUNTIME_LIBRARY_FLAG "/MD" CACHE STRING
      "MSVC runtime library flag used by CEF targets" FORCE)
endif()

find_package(CEF REQUIRED)
ADD_LOGICAL_TARGET("libcef_lib" "${CEF_LIB_DEBUG}" "${CEF_LIB_RELEASE}")
add_subdirectory("${CEF_LIBCEF_DLL_WRAPPER_PATH}" libcef_dll_wrapper)

add_library(VEYRA SHARED
  src/main_win.cpp
  src/app_state.cpp
  src/browser_client.cpp
  src/window.cpp
  src/svg_icons.cpp
  src/update_manager.cpp
  resources/veyra.rc
)
target_include_directories(VEYRA PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/generated" "${CEF_ROOT}")
target_compile_definitions(VEYRA PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX CEF_USE_BOOTSTRAP)
if(MSVC)
  target_compile_options(VEYRA PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()
target_link_libraries(VEYRA PRIVATE
  libcef_lib libcef_dll_wrapper ${CEF_STANDARD_LIBS}
  gdiplus dwmapi comctl32 shlwapi shell32 winhttp bcrypt
)
set_target_properties(VEYRA PROPERTIES
  PREFIX "" OUTPUT_NAME "VEYRA"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/out"
  ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
  LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/out"
  ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_BINARY_DIR}/lib"
)

# The CEF bootstrap launcher provides the Windows sandbox handle to VEYRA.dll.
if(COMMAND SET_LPAC_ACLS)
  # CEF's helper expects this variable to be populated by its sample target
  # setup macros. VEYRA defines its own output directory, so provide the
  # matching per-configuration directory explicitly.
  set(CEF_TARGET_OUT_DIR "$<TARGET_FILE_DIR:VEYRA>")
  SET_LPAC_ACLS(VEYRA)
endif()
if(COMMAND COPY_FILES)
  COPY_FILES("VEYRA" "${CEF_BINARY_FILES}" "${CEF_BINARY_DIR}" "$<TARGET_FILE_DIR:VEYRA>")
  COPY_FILES("VEYRA" "${CEF_RESOURCE_FILES}" "${CEF_RESOURCE_DIR}" "$<TARGET_FILE_DIR:VEYRA>")
endif()
add_custom_command(TARGET VEYRA POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/bootstrap.exe" "$<TARGET_FILE_DIR:VEYRA>/VEYRA.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CEF_BINARY_DIR}/bootstrap.exe" "$<TARGET_FILE_DIR:VEYRA>/bootstrap.exe"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/resources/veyra.ico" "$<TARGET_FILE_DIR:VEYRA>/VEYRA.ico"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/ui" "$<TARGET_FILE_DIR:VEYRA>/ui"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_BINARY_DIR}/generated-ui/version.js" "$<TARGET_FILE_DIR:VEYRA>/ui/version.js"
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:VEYRA>/scripts"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/scripts/apply-update.ps1" "$<TARGET_FILE_DIR:VEYRA>/scripts/apply-update.ps1"
  VERBATIM
)
find_program(INNO_SETUP_COMPILER NAMES ISCC.exe ISCC
  PATHS "C:/Program Files (x86)/Inno Setup 6" "C:/Program Files/Inno Setup 6"
)
if(INNO_SETUP_COMPILER)
  add_custom_target(VEYRAInstaller
    COMMAND "${INNO_SETUP_COMPILER}" "${CMAKE_BINARY_DIR}/veyra.iss"
    DEPENDS VEYRA
    COMMENT "Building the VEYRA Windows setup wizard"
    VERBATIM
  )
else()
  message(STATUS "Inno Setup 6 was not found. Install it to build VEYRAInstaller.")
endif()

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(VEYRA PROPERTIES
    VS_DEBUGGER_COMMAND "$<TARGET_FILE_DIR:VEYRA>/VEYRA.exe"
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:VEYRA>")
endif()

message(STATUS "VEYRA uses CEF ${CEF_VERSION} at ${CEF_ROOT}")
