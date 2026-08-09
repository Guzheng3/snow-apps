if(NOT WIN32)
    message(FATAL_ERROR "The Snow Apps MSVC toolchain is Windows-only.")
endif()

if(DEFINED _VCPKG_ROOT_DIR)
    set(_snow_vcpkg_root "${_VCPKG_ROOT_DIR}")
else()
    get_filename_component(_snow_vcpkg_root
        "${CMAKE_CURRENT_LIST_DIR}/../.tools/vcpkg" ABSOLUTE)
endif()
set(_snow_vcpkg_windows_toolchain
    "${_snow_vcpkg_root}/scripts/toolchains/windows.cmake")
if(NOT EXISTS "${_snow_vcpkg_windows_toolchain}")
    message(FATAL_ERROR
        "The vcpkg Windows toolchain was not found: ${_snow_vcpkg_windows_toolchain}")
endif()

# A chainloaded toolchain replaces vcpkg's platform toolchain. Load its Windows
# defaults explicitly so the selected triplet still controls /MT versus /MD,
# optimization flags, and variables forwarded to non-CMake build systems.
include("${_snow_vcpkg_windows_toolchain}")

set(_snow_msvc_145_root "C:/VS2026/BuildTools/VC/Tools/MSVC/14.51.36231")
set(_snow_msvc_145_bin "${_snow_msvc_145_root}/bin/HostX64/x64")
set(CMAKE_C_COMPILER "${_snow_msvc_145_bin}/cl.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_snow_msvc_145_bin}/cl.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_LINKER "${_snow_msvc_145_bin}/link.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_AR "${_snow_msvc_145_bin}/lib.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_MT "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/mt.exe"
    CACHE FILEPATH "" FORCE)
set(CMAKE_RC_COMPILER "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/rc.exe"
    CACHE FILEPATH "" FORCE)
# Use the 8.3 SDK path because CMake passes these linker flags unquoted to
# link.exe when it drives Ninja.
set(_snow_windows_sdk_root "C:/PROGRA~2/WI3CF2~1/10/Lib/100261~1.0")
set(_snow_windows_sdk_include "C:/PROGRA~2/WI3CF2~1/10/Include/100261~1.0")
set(_snow_msvc_libpath "/LIBPATH:${_snow_msvc_145_root}/lib/x64")
set(_snow_sdk_libpath
    "/LIBPATH:${_snow_windows_sdk_root}/um/x64 /LIBPATH:${_snow_windows_sdk_root}/ucrt/x64")
set(_snow_include_flags "/I${_snow_msvc_145_root}/include /I${_snow_windows_sdk_include}/ucrt /I${_snow_windows_sdk_include}/shared /I${_snow_windows_sdk_include}/um /I${_snow_windows_sdk_include}/winrt /I${_snow_windows_sdk_include}/cppwinrt")
set(_snow_rc_include_flags "/I${_snow_msvc_145_root}/include /I${_snow_windows_sdk_include}/ucrt /I${_snow_windows_sdk_include}/um /I${_snow_windows_sdk_include}/shared /I${_snow_windows_sdk_include}/winrt /I${_snow_windows_sdk_include}/cppwinrt")
set(_snow_linker_paths "${_snow_msvc_libpath} ${_snow_sdk_libpath}")

foreach(_snow_language IN ITEMS C CXX)
    string(FIND "${CMAKE_${_snow_language}_FLAGS}"
        "${_snow_msvc_145_root}/include" _snow_has_msvc_include)
    if(_snow_has_msvc_include EQUAL -1)
        set(CMAKE_${_snow_language}_FLAGS
            "${CMAKE_${_snow_language}_FLAGS} ${_snow_include_flags}"
            CACHE STRING "" FORCE)
    endif()
endforeach()

string(FIND "${CMAKE_RC_FLAGS}" "${_snow_msvc_145_root}/include"
    _snow_has_rc_msvc_include)
if(_snow_has_rc_msvc_include EQUAL -1)
    set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} ${_snow_rc_include_flags}"
        CACHE STRING "" FORCE)
endif()

foreach(_snow_linker_kind IN ITEMS EXE SHARED MODULE)
    string(FIND "${CMAKE_${_snow_linker_kind}_LINKER_FLAGS}"
        "${_snow_msvc_145_root}/lib/x64" _snow_has_msvc_libpath)
    if(_snow_has_msvc_libpath EQUAL -1)
        set(CMAKE_${_snow_linker_kind}_LINKER_FLAGS
            "${CMAKE_${_snow_linker_kind}_LINKER_FLAGS} ${_snow_linker_paths}"
            CACHE STRING "" FORCE)
    endif()
endforeach()

unset(_snow_vcpkg_root)
unset(_snow_vcpkg_windows_toolchain)
unset(_snow_windows_sdk_root)
unset(_snow_windows_sdk_include)
unset(_snow_msvc_libpath)
unset(_snow_sdk_libpath)
unset(_snow_include_flags)
unset(_snow_rc_include_flags)
unset(_snow_linker_paths)
unset(_snow_language)
unset(_snow_has_msvc_include)
unset(_snow_has_rc_msvc_include)
unset(_snow_linker_kind)
unset(_snow_has_msvc_libpath)
unset(_snow_msvc_145_root)
unset(_snow_msvc_145_bin)
