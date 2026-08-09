if(VCPKG_TARGET_IS_OSX OR VCPKG_TARGET_IS_IOS)
    if("framework" IN_LIST FEATURES)
        vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)
    endif()
endif()

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO microsoft/onnxruntime
    REF "v${VERSION}"
    SHA512 373c51575ada457b8aead5d195a5f3eba62fb747b6370a2a9889fff875c40ea30af8fd49104d58cc86f79247410e829086b0979f37ca8635c6dd34960e9cc424
    PATCHES
        "${VCPKG_ROOT_DIR}/ports/onnxruntime/fix-cmake.patch"
        "${VCPKG_ROOT_DIR}/ports/onnxruntime/fix-cmake-cuda.patch"
        fix-static-delay-load.patch
        generate-reduced-ops-during-configure.patch
)

find_program(PROTOC NAMES protoc PATHS "${CURRENT_HOST_INSTALLED_DIR}/tools/protobuf" REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)
find_program(FLATC NAMES flatc PATHS "${CURRENT_HOST_INSTALLED_DIR}/tools/flatbuffers" REQUIRED NO_DEFAULT_PATH NO_CMAKE_PATH)
x_vcpkg_get_python_packages(
    PYTHON_VERSION "3"
    PACKAGES flatbuffers
    OUT_PYTHON_VAR PYTHON3
)

set(SNOW_SHOT_REQUIRED_OPERATORS "${CMAKE_CURRENT_LIST_DIR}/required_operators.config")
if(NOT EXISTS "${SNOW_SHOT_REQUIRED_OPERATORS}")
    message(FATAL_ERROR
        "Snow Shot ONNX Runtime operator configuration is missing: ${SNOW_SHOT_REQUIRED_OPERATORS}")
endif()

vcpkg_execute_required_process(
    COMMAND "${PYTHON3}" onnxruntime/core/flatbuffers/schema/compile_schema.py --flatc "${FLATC}"
    LOGNAME compile_schema_core
    WORKING_DIRECTORY "${SOURCE_PATH}"
)
vcpkg_execute_required_process(
    COMMAND "${PYTHON3}" onnxruntime/lora/adapter_format/compile_schema.py --flatc "${FLATC}"
    LOGNAME compile_schema_lora
    WORKING_DIRECTORY "${SOURCE_PATH}"
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        python    onnxruntime_ENABLE_PYTHON
        training  onnxruntime_ENABLE_TRAINING
        training  onnxruntime_ENABLE_TRAINING_APIS
        cuda      onnxruntime_USE_CUDA
        cuda      onnxruntime_USE_CUDA_NHWC_OPS
        openvino  onnxruntime_USE_OPENVINO
        tensorrt  onnxruntime_USE_TENSORRT
        tensorrt  onnxruntime_USE_TENSORRT_BUILTIN_PARSER
        directml  onnxruntime_USE_DML
        winml     onnxruntime_USE_WINML
        coreml    onnxruntime_USE_COREML
        mimalloc  onnxruntime_USE_MIMALLOC
        valgrind  onnxruntime_USE_VALGRIND
        xnnpack   onnxruntime_USE_XNNPACK
        nnapi     onnxruntime_USE_NNAPI_BUILTIN
        azure     onnxruntime_USE_AZURE
        test      onnxruntime_BUILD_UNIT_TESTS
        test      onnxruntime_BUILD_BENCHMARKS
        test      onnxruntime_RUN_ONNX_TESTS
        framework onnxruntime_BUILD_APPLE_FRAMEWORK
        framework onnxruntime_BUILD_OBJC
        nccl      onnxruntime_USE_NCCL
    INVERTED_FEATURES
        cuda      onnxruntime_USE_MEMORY_EFFICIENT_ATTENTION
)

if("cuda" IN_LIST FEATURES)
    vcpkg_find_cuda(OUT_CUDA_TOOLKIT_ROOT cuda_toolkit_root)
    list(APPEND FEATURE_OPTIONS
        "-DCMAKE_CUDA_COMPILER=${NVCC}"
        "-DCUDAToolkit_ROOT=${cuda_toolkit_root}"
        "-DCMAKE_CUDA_FLAGS=-Xcudafe --diag_suppress=2803 -Wno-deprecated-gpu-targets"
    )
endif()

if("tensorrt" IN_LIST FEATURES)
    if(DEFINED ENV{TENSORRT_HOME})
        set(TENSORRT_HOME "$ENV{TENSORRT_HOME}")
    endif()
    if(DEFINED TENSORRT_HOME)
        list(APPEND FEATURE_OPTIONS "-Donnxruntime_TENSORRT_HOME:PATH=${TENSORRT_HOME}")
    else()
        message(WARNING "Define TENSORRT_HOME for onnxruntime_TENSORRT_HOME")
    endif()
endif()

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" BUILD_SHARED)
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static" AND "directml" IN_LIST FEATURES)
    set(VCPKG_POLICY_DLLS_IN_STATIC_LIBRARY enabled)
endif()

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}/cmake"
    OPTIONS
        ${FEATURE_OPTIONS}
        "-DPython_EXECUTABLE:FILEPATH=${PYTHON3}"
        "-DProtobuf_PROTOC_EXECUTABLE:FILEPATH=${PROTOC}"
        "-DONNX_CUSTOM_PROTOC_EXECUTABLE:FILEPATH=${PROTOC}"
        -DBUILD_PKGCONFIG_FILES=ON
        -Donnxruntime_BUILD_SHARED_LIB=${BUILD_SHARED}
        -Donnxruntime_CROSS_COMPILING=${VCPKG_CROSSCOMPILING}
        -Donnxruntime_USE_EXTENSIONS=OFF
        -Donnxruntime_USE_NNAPI_BUILTIN=${VCPKG_TARGET_IS_ANDROID}
        -Donnxruntime_USE_VCPKG=ON
        -Donnxruntime_USE_CUSTOM_DIRECTML=OFF
        -Donnxruntime_ENABLE_DELAY_LOADING_WIN_DLLS=ON
        -Donnxruntime_ENABLE_CPUINFO=ON
        -Donnxruntime_ENABLE_MICROSOFT_INTERNAL=OFF
        -Donnxruntime_ENABLE_BITCODE=OFF
        -Donnxruntime_ENABLE_PYTHON=OFF
        -Donnxruntime_ENABLE_EXTERNAL_CUSTOM_OP_SCHEMAS=OFF
        -Donnxruntime_ENABLE_MEMORY_PROFILE=OFF
        -Donnxruntime_ENABLE_LAZY_TENSOR=OFF
        -Donnxruntime_MINIMAL_BUILD=OFF
        -Donnxruntime_REDUCED_OPS_BUILD=ON
        "-Donnxruntime_REDUCED_OPS_CONFIG=${SNOW_SHOT_REQUIRED_OPERATORS}"
        -Donnxruntime_DISABLE_RTTI=OFF
        -Donnxruntime_DISABLE_ABSEIL=OFF
        --compile-no-warning-as-error
    OPTIONS_DEBUG
        -Donnxruntime_ENABLE_MEMLEAK_CHECKER=OFF
        -Donnxruntime_DEBUG_NODE_INPUTS_OUTPUTS=1
    MAYBE_UNUSED_VARIABLES
        Python_EXECUTABLE
        onnxruntime_TENSORRT_PLACEHOLDER_BUILDER
        onnxruntime_NVCC_THREADS
        CMAKE_CUDA_FLAGS
)
if("cuda" IN_LIST FEATURES)
    vcpkg_cmake_build(TARGET onnxruntime_providers_cuda LOGFILE_BASE build-cuda)
endif()
if("tensorrt" IN_LIST FEATURES)
    vcpkg_cmake_build(TARGET onnxruntime_providers_tensorrt LOGFILE_BASE build-tensorrt)
endif()
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/onnxruntime)
vcpkg_fixup_pkgconfig()

function(reolocate_ort_providers)
    if(VCPKG_TARGET_IS_WINDOWS AND VCPKG_LIBRARY_LINKAGE STREQUAL "dynamic")
        file(GLOB PROVIDER_BINS_DEBUG "${CURRENT_PACKAGES_DIR}/debug/lib/onnxruntime_providers_*.dll")
        file(COPY ${PROVIDER_BINS_DEBUG} DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
        file(GLOB PROVIDER_BINS_RELEASE "${CURRENT_PACKAGES_DIR}/lib/onnxruntime_providers_*.dll")
        file(COPY ${PROVIDER_BINS_RELEASE} DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
        file(REMOVE ${PROVIDER_BINS_DEBUG} ${PROVIDER_BINS_RELEASE})
    endif()
endfunction()

reolocate_ort_providers()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/bin" "${CURRENT_PACKAGES_DIR}/bin")
endif()

if("directml" IN_LIST FEATURES)
    set(DIRECTML_PACKAGE_DIR "${CURRENT_BUILDTREES_DIR}/packages/Microsoft.AI.DirectML.1.15.4")
    set(DIRECTML_RUNTIME "${DIRECTML_PACKAGE_DIR}/bin/x64-win/DirectML.dll")
    set(DIRECTML_IMPORT_LIBRARY "${DIRECTML_PACKAGE_DIR}/bin/x64-win/DirectML.lib")
    foreach(DIRECTML_FILE IN ITEMS "${DIRECTML_RUNTIME}" "${DIRECTML_IMPORT_LIBRARY}")
        if(NOT EXISTS "${DIRECTML_FILE}")
            message(FATAL_ERROR "DirectML package file was not restored: ${DIRECTML_FILE}")
        endif()
    endforeach()

    file(INSTALL "${DIRECTML_IMPORT_LIBRARY}" DESTINATION "${CURRENT_PACKAGES_DIR}/lib")
    set(ONNXRUNTIME_TARGETS_FILE
        "${CURRENT_PACKAGES_DIR}/share/onnxruntime/onnxruntimeTargets.cmake")
    file(TO_CMAKE_PATH "${DIRECTML_IMPORT_LIBRARY}" DIRECTML_IMPORT_LIBRARY_CMAKE)
    file(READ "${ONNXRUNTIME_TARGETS_FILE}" ONNXRUNTIME_TARGETS_CONTENT)
    string(REPLACE
        "${DIRECTML_IMPORT_LIBRARY_CMAKE}"
        "\${_IMPORT_PREFIX}/lib/DirectML.lib"
        RELOCATABLE_ONNXRUNTIME_TARGETS_CONTENT
        "${ONNXRUNTIME_TARGETS_CONTENT}"
    )
    if(RELOCATABLE_ONNXRUNTIME_TARGETS_CONTENT STREQUAL ONNXRUNTIME_TARGETS_CONTENT)
        message(FATAL_ERROR
            "ONNX Runtime targets did not contain the expected DirectML import library path")
    endif()
    file(WRITE "${ONNXRUNTIME_TARGETS_FILE}" "${RELOCATABLE_ONNXRUNTIME_TARGETS_CONTENT}")

    file(INSTALL "${DIRECTML_RUNTIME}" DESTINATION "${CURRENT_PACKAGES_DIR}/bin")
    file(INSTALL "${DIRECTML_RUNTIME}" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
