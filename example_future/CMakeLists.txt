file(GLOB sources "*.hpp" "*.cpp")
set(allSources ${sources})
set(formatted ${formatted} ${allSources} PARENT_SCOPE)
add_executable(example_future ${allSources})
target_link_libraries(example_future ${CONAN_LIBS} ${CMAKE_THREAD_LIBS_INIT})
target_include_directories(example_future PRIVATE ${CMAKE_BINARY_DIR})
