file(GLOB generateSources "generate.cpp")
add_executable(test_app_generate ${generateSources})
target_link_libraries(test_app_generate ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})

set(generatedHeader ${CMAKE_BINARY_DIR}/generated.hpp)
add_custom_target(generatedHeaderTarget COMMAND $<TARGET_FILE:test_app_generate> ${generatedHeader} DEPENDS test_app_generate)

file(GLOB useSources "use.cpp" ${generatedHeader})
set(formatted ${formatted} ${generateSources} ${useSources} PARENT_SCOPE)
add_executable(test_app_use ${useSources})
target_link_libraries(test_app_use ${Boost_LIBRARIES} ${CMAKE_THREAD_LIBS_INIT} ${CONAN_LIBS})
add_dependencies(test_app_use generatedHeaderTarget)
target_include_directories(test_app_use PRIVATE ${CMAKE_BINARY_DIR})
