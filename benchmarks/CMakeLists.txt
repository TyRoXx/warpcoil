add_executable(benchmark_interfaces_generate benchmark_interface.hpp benchmark_interface.cpp)
target_link_libraries(benchmark_interfaces_generate ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT} ${WARPCOIL_LIBRT})

set(generatedHeader ${CMAKE_BINARY_DIR}/benchmark_interfaces.hpp)
if(WARPCOIL_USE_CLANG_FORMAT AND WARPCOIL_CLANG_FORMAT)
    add_custom_target(benchmark_interfaces_header
        COMMAND $<TARGET_FILE:benchmark_interfaces_generate> ${generatedHeader} ${WARPCOIL_CLANG_FORMAT} ${CMAKE_SOURCE_DIR}
        DEPENDS benchmark_interfaces_generate clang-format)
else()
    add_custom_target(benchmark_interfaces_header
        COMMAND $<TARGET_FILE:benchmark_interfaces_generate> ${generatedHeader}
        DEPENDS benchmark_interfaces_generate)
endif()

file(GLOB sources main.cpp in_process.cpp local_socket.cpp)
file(GLOB_RECURSE headers "../warpcoil/*.hpp" "*.hpp")
set(allSources ${sources} ${headers})
if(EXISTS ${generatedHeader})
    set(allSources ${allSources} ${generatedHeader})
endif()
add_executable(benchmarks ${allSources})
target_link_libraries(benchmarks ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT})
add_dependencies(benchmarks benchmark_interfaces_header)
target_include_directories(benchmarks PRIVATE ${CMAKE_BINARY_DIR})

file(GLOB formattedSources *.hpp *.cpp)
set(formatted ${formatted} ${formattedSources} PARENT_SCOPE)
