add_executable(snow_image_core_tests core_tests.cpp)
target_link_libraries(snow_image_core_tests PRIVATE snow_image::snow_image)
target_compile_features(snow_image_core_tests PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(snow_image_core_tests PRIVATE /utf-8 /permissive- /Zc:__cplusplus /W4)
else()
    target_compile_options(snow_image_core_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME snow_image_core_tests COMMAND snow_image_core_tests)
